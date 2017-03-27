/* ide-run-manager.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-run-manager"

#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-target.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-manager.h"
#include "runner/ide-run-manager.h"
#include "runner/ide-run-manager-private.h"
#include "runner/ide-runner.h"
#include "runtimes/ide-runtime.h"

struct _IdeRunManager
{
  IdeObject                parent_instance;

  GCancellable            *cancellable;
  IdeBuildTarget          *build_target;

  const IdeRunHandlerInfo *handler;
  GList                   *handlers;

  guint                    busy : 1;
};

static void action_group_iface_init     (GActionGroupInterface *iface);
static void ide_run_manager_notify_busy (IdeRunManager         *self);

G_DEFINE_TYPE_EXTENDED (IdeRunManager, ide_run_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_HANDLER,
  PROP_BUILD_TARGET,
  N_PROPS
};

enum {
  RUN,
  STOPPED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_run_manager_real_run (IdeRunManager *self,
                          IdeRunner     *runner)
{
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  /*
   * If the current handler has a callback specified (our default "run" handler
   * does not), then we need to allow that handler to prepare the runner.
   */
  if (self->handler != NULL && self->handler->handler != NULL)
    self->handler->handler (self, runner, self->handler->handler_data);
}

static void
ide_run_handler_info_free (gpointer data)
{
  IdeRunHandlerInfo *info = data;

  g_free (info->id);
  g_free (info->title);
  g_free (info->icon_name);
  g_free (info->accel);

  if (info->handler_data_destroy)
    info->handler_data_destroy (info->handler_data);

  g_slice_free (IdeRunHandlerInfo, info);
}

static void
ide_run_manager_finalize (GObject *object)
{
  IdeRunManager *self = (IdeRunManager *)object;

  g_clear_object (&self->cancellable);
  g_clear_object (&self->build_target);

  g_list_free_full (self->handlers, ide_run_handler_info_free);
  self->handlers = NULL;

  G_OBJECT_CLASS (ide_run_manager_parent_class)->finalize (object);
}

static void
ide_run_manager_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeRunManager *self = IDE_RUN_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_run_manager_get_busy (self));
      break;

    case PROP_HANDLER:
      g_value_set_string (value, ide_run_manager_get_handler (self));
      break;

    case PROP_BUILD_TARGET:
      g_value_set_object (value, ide_run_manager_get_build_target (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_manager_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeRunManager *self = IDE_RUN_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUILD_TARGET:
      ide_run_manager_set_build_target (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
ide_run_manager_class_init (IdeRunManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_run_manager_finalize;
  object_class->get_property = ide_run_manager_get_property;
  object_class->set_property = ide_run_manager_set_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "Busy",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HANDLER] =
    g_param_spec_string ("handler",
                         "Handler",
                         "Handler",
                         "run",
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BUILD_TARGET] =
    g_param_spec_object ("build-target",
                         "Build Target",
                         "The IdeBuildTarget that will be run",
                         IDE_TYPE_BUILD_TARGET,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeRunManager::run:
   * @self: An #IdeRunManager
   * @runner: An #IdeRunner
   *
   * This signal is emitted right before ide_runner_run_async() is called
   * on an #IdeRunner. It can be used by plugins to tweak things right
   * before the runner is executed.
   *
   * The current run handler (debugger, profiler, etc) is run as the default
   * handler for this function. So connect with %G_SIGNAL_AFTER if you want
   * to be nofied after the run handler has executed. It's unwise to change
   * things that the run handler might expect. Generally if you want to
   * change settings, do that before the run handler has exected.
   */
  signals [RUN] =
    g_signal_new_class_handler ("run",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_run_manager_real_run),
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE,
                                1,
                                IDE_TYPE_RUNNER);

  /**
   * IdeRunManager::stopped:
   *
   * This signal is emitted when the run manager has stopped the currently
   * executing inferior.
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
ide_run_manager_init (IdeRunManager *self)
{
  ide_run_manager_add_handler (self,
                               "run",
                               _("Run"),
                               "media-playback-start-symbolic",
                               "<Control>F5",
                               NULL,
                               NULL,
                               NULL);
}

gboolean
ide_run_manager_get_busy (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), FALSE);

  return self->busy;
}

static gboolean
ide_run_manager_check_busy (IdeRunManager  *self,
                            GError        **error)
{
  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (error != NULL);

  if (ide_run_manager_get_busy (self))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_BUSY,
                   "%s",
                   _("Cannot run target, another target is running"));
      return TRUE;
    }

  return FALSE;
}

static void
ide_run_manager_run_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeRunner *runner = (IdeRunner *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_RUN_MANAGER (self));

  self->busy = FALSE;

  if (!ide_runner_run_finish (runner, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  g_signal_emit (self, signals [STOPPED], 0);

  ide_run_manager_notify_busy (self);

  IDE_EXIT;
}

static void
do_run_async (IdeRunManager *self,
              GTask         *task)
{
  IdeBuildTarget *build_target;
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeRuntime *runtime;
  g_autoptr(IdeRunner) runner = NULL;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_TASK (task));

  build_target = g_task_get_task_data (task);
  context = ide_object_get_context (IDE_OBJECT (self));

  g_assert (IDE_IS_BUILD_TARGET (build_target));
  g_assert (IDE_IS_CONTEXT (context));

  config_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (config_manager);
  runtime = ide_configuration_get_runtime (config);

  if (runtime == NULL)
    {
      g_task_return_new_error (task,
                               IDE_RUNTIME_ERROR,
                               IDE_RUNTIME_ERROR_NO_SUCH_RUNTIME,
                               "%s “%s”",
                               _("Failed to locate runtime"),
                               ide_configuration_get_runtime_id (config));
      IDE_EXIT;
    }

  runner = ide_runtime_create_runner (runtime, build_target);
  cancellable = g_task_get_cancellable (task);

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_signal_emit (self, signals [RUN], 0, runner);

  if (ide_runner_get_failed (runner))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to execute the application");
      IDE_EXIT;
    }

  self->busy = TRUE;
  ide_run_manager_notify_busy (self);

  ide_runner_run_async (runner,
                        cancellable,
                        ide_run_manager_run_cb,
                        g_object_ref (task));

  IDE_EXIT;
}

static void
ide_run_manager_run_discover_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  g_autoptr(IdeBuildTarget) build_target = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  build_target = ide_run_manager_discover_default_target_finish (self, result, &error);

  if (build_target == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_run_manager_set_build_target (self, build_target);

  g_task_set_task_data (task, g_steal_pointer (&build_target), g_object_unref);

  do_run_async (self, task);

  IDE_EXIT;
}

static void
ide_run_manager_install_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeRunManager *self;
  IdeBuildTarget *build_target;
  GCancellable *cancellable;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (IDE_IS_RUN_MANAGER (self));

  if (!ide_build_manager_execute_finish (build_manager, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  build_target = ide_run_manager_get_build_target (self);

  if (build_target == NULL)
    {
      cancellable = g_task_get_cancellable (task);
      g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

      ide_run_manager_discover_default_target_async (self,
                                                     cancellable,
                                                     ide_run_manager_run_discover_cb,
                                                     g_steal_pointer (&task));
      IDE_EXIT;
    }

  g_task_set_task_data (task, g_object_ref (build_target), g_object_unref);

  do_run_async (self, task);

  IDE_EXIT;
}

static void
ide_run_manager_notify_busy (IdeRunManager *self)
{
  g_assert (IDE_IS_RUN_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (self), "run", self->busy == FALSE);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (self), "run-with-handler", self->busy == FALSE);
  g_action_group_action_enabled_changed (G_ACTION_GROUP (self), "stop", self->busy == TRUE);
}

static void
ide_run_manager_task_completed (IdeRunManager *self,
                                GParamSpec    *pspec,
                                GTask         *task)
{
  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (pspec != NULL);
  g_assert (G_IS_TASK (task));

  self->busy = FALSE;

  ide_run_manager_notify_busy (self);

  IDE_EXIT;
}

static void
ide_run_manager_do_install_before_run (IdeRunManager *self,
                                       GTask         *task)
{
  IdeBuildManager *build_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_TASK (task));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);

  /*
   * First we need to make sure the target is up to date and installed
   * so that all the dependent resources are available.
   */

  self->busy = TRUE;

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_run_manager_task_completed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_build_manager_execute_async (build_manager,
                                   IDE_BUILD_PHASE_INSTALL,
                                   g_task_get_cancellable (task),
                                   ide_run_manager_install_cb,
                                   g_object_ref (task));

  ide_run_manager_notify_busy (self);

  IDE_EXIT;
}

void
ide_run_manager_run_async (IdeRunManager       *self,
                           IdeBuildTarget      *build_target,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GCancellable) local_cancellable = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!build_target || IDE_IS_BUILD_TARGET (build_target));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_run_manager_run_async);

  g_set_object (&self->cancellable, cancellable);

  if (ide_run_manager_check_busy (self, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (build_target != NULL)
    ide_run_manager_set_build_target (self, build_target);

  ide_run_manager_do_install_before_run (self, task);

  IDE_EXIT;
}

gboolean
ide_run_manager_run_finish (IdeRunManager  *self,
                            GAsyncResult   *result,
                            GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
do_cancel_in_timeout (gpointer user_data)
{
  g_autoptr(GCancellable) cancellable = user_data;

  IDE_ENTRY;

  g_assert (G_IS_CANCELLABLE (cancellable));

  if (!g_cancellable_is_cancelled (cancellable))
    g_cancellable_cancel (cancellable);

  IDE_RETURN (G_SOURCE_REMOVE);
}

void
ide_run_manager_cancel (IdeRunManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  if (self->cancellable != NULL)
    g_timeout_add (0, do_cancel_in_timeout, g_object_ref (self->cancellable));

  IDE_EXIT;
}

void
ide_run_manager_set_handler (IdeRunManager *self,
                             const gchar   *id)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));

  self->handler = NULL;

  for (GList *iter = self->handlers; iter; iter = iter->next)
    {
      const IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, id) == 0)
        {
          self->handler = info;
          IDE_TRACE_MSG ("run handler set to %s", info->title);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HANDLER]);
          break;
        }
    }
}

void
ide_run_manager_add_handler (IdeRunManager  *self,
                             const gchar    *id,
                             const gchar    *title,
                             const gchar    *icon_name,
                             const gchar    *accel,
                             IdeRunHandler   run_handler,
                             gpointer        user_data,
                             GDestroyNotify  user_data_destroy)
{
  IdeRunHandlerInfo *info;
  g_autofree gchar *action_name = NULL;
  const gchar *accels[] = { accel, NULL };
  GApplication *app;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (id != NULL);
  g_return_if_fail (title != NULL);

  info = g_slice_new (IdeRunHandlerInfo);
  info->id = g_strdup (id);
  info->title = g_strdup (title);
  info->icon_name = g_strdup (icon_name);
  info->accel = g_strdup (accel);
  info->handler = run_handler;
  info->handler_data = user_data;
  info->handler_data_destroy = user_data_destroy;

  app = g_application_get_default ();
  action_name = g_strdup_printf ("run-manager.run-with-handler('%s')", id);

  if (accel != NULL && app != NULL)
    gtk_application_set_accels_for_action (GTK_APPLICATION (app), action_name, accels);

  self->handlers = g_list_append (self->handlers, info);

  if (self->handler == NULL)
    self->handler = info;
}

void
ide_run_manager_remove_handler (IdeRunManager *self,
                                const gchar   *id)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (id != NULL);

  for (GList *iter = self->handlers; iter; iter = iter->next)
    {
      IdeRunHandlerInfo *info = iter->data;

      if (g_strcmp0 (info->id, id) == 0)
        {
          self->handlers = g_list_remove_link (self->handlers, iter);

          if (self->handler == info && self->handlers != NULL)
            self->handler = self->handlers->data;
          else
            self->handler = NULL;

          ide_run_handler_info_free (info);

          break;
        }
    }
}

/**
 * ide_run_manager_get_build_target:
 *
 * Gets the build target that will be executed by the run manager which
 * was either specified to ide_run_manager_run_async() or determined by
 * the build system.
 *
 * Returns: (transfer none): An #IdeBuildTarget or %NULL if no build target
 *   has been set.
 */
IdeBuildTarget *
ide_run_manager_get_build_target (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  return self->build_target;
}

void
ide_run_manager_set_build_target (IdeRunManager  *self,
                                  IdeBuildTarget *build_target)
{
  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (IDE_IS_BUILD_TARGET (build_target));

  if (g_set_object (&self->build_target, build_target))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUILD_TARGET]);
}

static IdeBuildTarget *
find_best_target (GPtrArray *targets)
{
  IdeBuildTarget *ret = NULL;
  guint i;

  g_assert (targets != NULL);

  /* TODO:
   *
   * This is just a barebones way to try to discover a target that matters. We
   * could probably defer this off to the build system. Either way, it's shit
   * and should be thought through by someone.
   */

  for (i = 0; i < targets->len; i++)
    {
      IdeBuildTarget *target = g_ptr_array_index (targets, i);
      g_autoptr(GFile) installdir = NULL;

      installdir = ide_build_target_get_install_directory (target);

      if (installdir == NULL)
        continue;

      if (ret == NULL)
        ret = target;
    }

  return ret;
}

static void
ide_run_manager_discover_default_target_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GPtrArray) targets = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildTarget *best_match;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));

  targets = ide_build_system_get_build_targets_finish (build_system, result, &error);

  if (targets == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  best_match = find_best_target (targets);

  if (best_match == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to locate build target");
      IDE_EXIT;
    }

  g_task_return_pointer (task, g_object_ref (best_match), g_object_unref);

  IDE_EXIT;
}

void
ide_run_manager_discover_default_target_async (IdeRunManager       *self,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_run_manager_discover_default_target_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);

  ide_build_system_get_build_targets_async (build_system,
                                            cancellable,
                                            ide_run_manager_discover_default_target_cb,
                                            g_object_ref (task));

  IDE_EXIT;
}

/**
 * ide_run_manager_discover_default_target_finish:
 *
 * Returns: (transfer full): An #IdeBuildTarget if successful; otherwise %NULL
 *   and @error is set.
 */
IdeBuildTarget *
ide_run_manager_discover_default_target_finish (IdeRunManager  *self,
                                                GAsyncResult   *result,
                                                GError        **error)
{
  IdeBuildTarget *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  IDE_RETURN (ret);
}

const GList *
_ide_run_manager_get_handlers (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  return self->handlers;
}

const gchar *
ide_run_manager_get_handler (IdeRunManager *self)
{
  g_return_val_if_fail (IDE_IS_RUN_MANAGER (self), NULL);

  if (self->handler != NULL)
    return self->handler->id;

  return NULL;
}

static const gchar *action_names[] = {
  "run",
  "run-with-handler",
  "stop",
  NULL
};

static gboolean
ide_run_manager_has_action (GActionGroup *group,
                            const gchar  *action_name)
{
  g_assert (G_IS_ACTION_GROUP (group));
  g_assert (action_name != NULL);

  for (guint i = 0; action_names[i]; i++)
    {
      if (g_strcmp0 (action_names[i], action_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static gchar **
ide_run_manager_list_actions (GActionGroup *group)
{
  return g_strdupv ((gchar **)action_names);
}

static gboolean
ide_run_manager_query_action (GActionGroup        *group,
                              const gchar         *action_name,
                              gboolean            *enabled,
                              const GVariantType **parameter_type,
                              const GVariantType **state_type,
                              GVariant           **state_hint,
                              GVariant           **state)
{
  IdeRunManager *self = (IdeRunManager *)group;
  const GVariantType *real_parameter_type = NULL;
  gboolean real_enabled = FALSE;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (action_name != NULL);

  if (g_strcmp0 (action_name, "run-with-handler") == 0)
    {
      real_enabled = self->busy == FALSE;
      real_parameter_type = G_VARIANT_TYPE_STRING;
      goto finish;
    }

  if (g_strcmp0 (action_name, "run") == 0)
    {
      real_enabled = self->busy == FALSE;
      goto finish;
    }

  if (g_strcmp0 (action_name, "stop") == 0)
    {
      real_enabled = self->busy == TRUE;
      goto finish;
    }

finish:
  if (state_type)
    *state_type = NULL;

  if (state_hint)
    *state_hint = NULL;

  if (state)
    *state = NULL;

  if (enabled)
    *enabled = real_enabled;

  if (parameter_type)
    *parameter_type = real_parameter_type;

  return TRUE;
}

static void
ide_run_manager_run_action_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeRunManager *self = (IdeRunManager *)object;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  context = ide_object_get_context (IDE_OBJECT (self));

  /* Propagate the error to the context */
  if (!ide_run_manager_run_finish (self, result, &error))
    ide_context_warning (context, "%s", error->message);
}

static void
ide_run_manager_activate_action (GActionGroup *group,
                                 const gchar  *action_name,
                                 GVariant     *parameter)
{
  IdeRunManager *self = (IdeRunManager *)group;
  g_autoptr(GVariant) sunk = NULL;

  g_assert (IDE_IS_RUN_MANAGER (self));
  g_assert (action_name != NULL);

  if (parameter != NULL && g_variant_is_floating (parameter))
    sunk = g_variant_ref_sink (parameter);

  if (FALSE) {}
  else if (g_strcmp0 (action_name, "run-with-handler") == 0)
    {
      const gchar *handler = NULL;

      if (parameter != NULL)
        handler = g_variant_get_string (parameter, NULL);

      /* "" translates to current handler */
      if (handler && *handler)
        ide_run_manager_set_handler (self, handler);

      ide_run_manager_run_async (self,
                                 NULL,
                                 NULL,
                                 ide_run_manager_run_action_cb,
                                 NULL);
    }
  else if (g_strcmp0 (action_name, "run") == 0)
    {
      ide_run_manager_run_async (self,
                                 NULL,
                                 NULL,
                                 ide_run_manager_run_action_cb,
                                 NULL);
    }
  else if (g_strcmp0 (action_name, "stop") == 0)
    {
      ide_run_manager_cancel (self);
    }
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->has_action = ide_run_manager_has_action;
  iface->list_actions = ide_run_manager_list_actions;
  iface->query_action = ide_run_manager_query_action;
  iface->activate_action = ide_run_manager_activate_action;
}
