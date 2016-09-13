/* ide-build-manager.c
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

#define G_LOG_DOMAIN "ide-build-manager"

#include <egg-signal-group.h>
#include <glib/gi18n.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-builder.h"
#include "buildsystem/ide-build-manager.h"
#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-build-system.h"
#include "buildsystem/ide-build-target.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-manager.h"

struct _IdeBuildManager
{
  IdeObject             parent_instance;

  EggSignalGroup       *signals;
  IdeBuildResult       *build_result;
  GCancellable         *cancellable;
  GDateTime            *last_build_time;
  GSimpleActionGroup   *actions;

  guint                 has_diagnostics : 1;
};

static void action_group_iface_init (GActionGroupInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeBuildManager, ide_build_manager, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, action_group_iface_init))

enum {
  PROP_0,
  PROP_BUSY,
  PROP_HAS_DIAGNOSTICS,
  PROP_LAST_BUILD_TIME,
  PROP_MESSAGE,
  PROP_RUNNING_TIME,
  N_PROPS
};

enum {
  BUILD_STARTED,
  BUILD_FINISHED,
  BUILD_FAILED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
ide_build_manager__build_result__notify_mode (IdeBuildManager *self,
                                              GParamSpec      *mode_pspec,
                                              IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
}

static void
ide_build_manager__build_result__notify_running (IdeBuildManager *self,
                                                 GParamSpec      *running_pspec,
                                                 IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager__build_result__notify_running_time (IdeBuildManager *self,
                                                      GParamSpec      *running_time_pspec,
                                                      IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
}

static void
ide_build_manager__build_result__diagnostic (IdeBuildManager *self,
                                             IdeDiagnostic   *diagnostic,
                                             IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (diagnostic != NULL);
  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (self->has_diagnostics == FALSE)
    {
      self->has_diagnostics = TRUE;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);
    }
}

static void
ide_build_manager_build_activate (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  IdeBuildManager *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_build_async (self,
                                 NULL,
                                 IDE_BUILDER_BUILD_FLAGS_NONE,
                                 NULL,
                                 NULL,
                                 NULL);
}

static void
ide_build_manager_rebuild_activate (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  IdeBuildManager *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_build_async (self,
                                 NULL,
                                 IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN,
                                 NULL,
                                 NULL,
                                 NULL);
}

static void
ide_build_manager_cancel_activate (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  IdeBuildManager *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_cancel (self);
}

static void
ide_build_manager_clean_activate (GSimpleAction *action,
                                  GVariant      *parameter,
                                  gpointer       user_data)
{
  IdeBuildManager *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_BUILD_MANAGER (self));

  ide_build_manager_build_async (self,
                                 NULL,
                                 (IDE_BUILDER_BUILD_FLAGS_FORCE_CLEAN |
                                  IDE_BUILDER_BUILD_FLAGS_NO_BUILD),
                                 NULL,
                                 NULL,
                                 NULL);
}

static void
ide_build_manager__build_result__notify_failed (IdeBuildManager *self,
                                                GParamSpec      *pspec,
                                                IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  if (ide_build_result_get_failed (build_result))
    g_signal_emit (self, signals [BUILD_FAILED], 0, build_result);
}

static void
ide_build_manager_real_build_started (IdeBuildManager *self,
                                      IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager_real_build_failed (IdeBuildManager *self,
                                     IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager_real_build_finished (IdeBuildManager *self,
                                       IdeBuildResult  *build_result)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

static void
ide_build_manager_finalize (GObject *object)
{
  IdeBuildManager *self = (IdeBuildManager *)object;

  g_clear_object (&self->build_result);
  g_clear_object (&self->signals);
  g_clear_object (&self->actions);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->last_build_time, g_date_time_unref);

  G_OBJECT_CLASS (ide_build_manager_parent_class)->finalize (object);
}

static void
ide_build_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeBuildManager *self = IDE_BUILD_MANAGER (object);

  switch (prop_id)
    {
    case PROP_BUSY:
      g_value_set_boolean (value, ide_build_manager_get_busy (self));
      break;

    case PROP_LAST_BUILD_TIME:
      g_value_set_boxed (value, ide_build_manager_get_last_build_time (self));
      break;

    case PROP_HAS_DIAGNOSTICS:
      g_value_set_boolean (value, self->has_diagnostics);
      break;

    case PROP_MESSAGE:
      g_value_take_string (value, ide_build_manager_get_message (self));
      break;

    case PROP_RUNNING_TIME:
      g_value_set_int64 (value, ide_build_manager_get_running_time (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_manager_class_init (IdeBuildManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_manager_finalize;
  object_class->get_property = ide_build_manager_get_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "If the build manager is busy building",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_LAST_BUILD_TIME] =
    g_param_spec_boxed ("last-build-time",
                        "Last Build Time",
                        "The time the last build was submitted",
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_DIAGNOSTICS] =
    g_param_spec_boolean ("has-diagnostics",
                          "Has Diagnostics",
                          "If the build result has diagnostics",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_MESSAGE] =
    g_param_spec_string ("message",
                         "Message",
                         "The current build message",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNNING_TIME] =
    g_param_spec_int64 ("running-time",
                        "Running Time",
                        "The duration of the build as a GTimeSpan",
                        0,
                        G_MAXINT64,
                        0,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [BUILD_STARTED] =
    g_signal_new_class_handler ("build-started",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_started),
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_RESULT);

  signals [BUILD_FAILED] =
    g_signal_new_class_handler ("build-failed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_failed),
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_RESULT);

  signals [BUILD_FINISHED] =
    g_signal_new_class_handler ("build-finished",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_build_manager_real_build_finished),
                                NULL,
                                NULL,
                                NULL,
                                G_TYPE_NONE, 1, IDE_TYPE_BUILD_RESULT);
}

static void
ide_build_manager_init (IdeBuildManager *self)
{
  static const GActionEntry action_entries[] = {
    { "build", ide_build_manager_build_activate },
    { "cancel", ide_build_manager_cancel_activate },
    { "clean", ide_build_manager_clean_activate },
    { "rebuild", ide_build_manager_rebuild_activate },
  };

  self->signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->signals,
                                   "notify::failed",
                                   G_CALLBACK (ide_build_manager__build_result__notify_failed),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::mode",
                                   G_CALLBACK (ide_build_manager__build_result__notify_mode),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::running",
                                   G_CALLBACK (ide_build_manager__build_result__notify_running),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "notify::running-time",
                                   G_CALLBACK (ide_build_manager__build_result__notify_running_time),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->signals,
                                   "diagnostic",
                                   G_CALLBACK (ide_build_manager__build_result__diagnostic),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->actions = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (self->actions),
                                   action_entries,
                                   G_N_ELEMENTS (action_entries),
                                   self);

  g_object_bind_property (self,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions), "build"),
                          "enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (self,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions), "rebuild"),
                          "enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (self,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions), "clean"),
                          "enabled",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  g_object_bind_property (self,
                          "busy",
                          g_action_map_lookup_action (G_ACTION_MAP (self->actions), "cancel"),
                          "enabled",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (self->actions,
                           "action-enabled-changed",
                           G_CALLBACK (g_action_group_action_enabled_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_build_manager_set_build_result (IdeBuildManager *self,
                                    IdeBuildResult  *build_result)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!build_result || IDE_IS_BUILD_RESULT (build_result));

  if (g_set_object (&self->build_result, build_result))
    {
      egg_signal_group_set_target (self->signals, build_result);

      self->has_diagnostics = FALSE;

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUNNING_TIME]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_DIAGNOSTICS]);

      g_signal_emit (self, signals [BUILD_STARTED], 0, build_result);
    }

  IDE_EXIT;
}

static gboolean
ide_build_manager_check_busy (IdeBuildManager  *self,
                              GError          **error)
{
  g_assert (IDE_IS_BUILD_MANAGER (self));

  if (ide_build_manager_get_busy (self))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_BUSY,
                   "%s",
                   _("A build is already in progress"));
      return TRUE;
    }

  return FALSE;
}

static IdeBuilder *
ide_build_manager_get_builder (IdeBuildManager  *self,
                               GError          **error)
{
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeBuildSystem *build_system;
  IdeContext *context;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  config_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (config_manager);

  build_system = ide_context_get_build_system (context);

  return ide_build_system_get_builder (build_system, config, error);
}

static void
ide_build_manager_build_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GTask) task = user_data;
  IdeBuildManager *self;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDER (builder));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  build_result = ide_builder_build_finish (builder, result, &error);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!build_result || IDE_IS_BUILD_RESULT (build_result));

  if (self->build_result != NULL)
    g_signal_emit (self, signals [BUILD_FINISHED], 0, self->build_result);

  if (build_result == NULL)
    {
      IDE_TRACE_MSG ("%s", error->message);
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  g_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

void
ide_build_manager_build_async (IdeBuildManager      *self,
                               IdeBuildTarget       *build_target,
                               IdeBuilderBuildFlags  build_flags,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GCancellable) local_cancellable = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!build_target || IDE_IS_BUILD_TARGET (build_target));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_manager_build_async);

  if (ide_build_manager_check_busy (self, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  if (NULL == (builder = ide_build_manager_get_builder (self, &error)))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_set_object (&self->cancellable, cancellable);

  /*
   * TODO: We need to add support to IdeBuilder to allow specifying what
   *       build target we want to ensure is built. That way, we can possibly
   *       reduce how much we build in the future. Probably something like:
   *       ide_builder_add_build_target(builder, build_target);
   */

  ide_builder_build_async (builder,
                           build_flags,
                           &build_result,
                           cancellable,
                           ide_build_manager_build_cb,
                           g_object_ref (task));

  ide_build_manager_set_build_result (self, build_result);

  /*
   * Update our last build time.
   */
  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  self->last_build_time = g_date_time_new_now_local ();
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);

  IDE_EXIT;
}

gboolean
ide_build_manager_build_finish (IdeBuildManager  *self,
                                GAsyncResult     *result,
                                GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_manager_install_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeBuilder *builder = (IdeBuilder *)object;
  g_autoptr(IdeBuildResult) build_result = NULL;
  g_autoptr(GTask) task = user_data;
  IdeBuildManager *self;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILDER (builder));

  self = g_task_get_source_object (task);
  build_result = ide_builder_install_finish (builder, result, &error);

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (!build_result || IDE_IS_BUILD_RESULT (build_result));

  if (self->build_result != NULL)
    g_signal_emit (self, signals [BUILD_FINISHED], 0, self->build_result);

  if (build_result == NULL)
    {
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  g_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

void
ide_build_manager_install_async (IdeBuildManager     *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(IdeBuilder) builder = NULL;
  g_autoptr(IdeBuildResult) build_result = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_manager_install_async);

  if (ide_build_manager_check_busy (self, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  if (NULL == (builder = ide_build_manager_get_builder (self, &error)))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_set_object (&self->cancellable, cancellable);

  /*
   * We might be able to save some build time if we can limit the target
   * that needs to be installed. However, it's unclear that we want that
   * because it could result in incomplete installation unless the build
   * system compensates for it.
   */

  ide_builder_install_async (builder,
                             &build_result,
                             cancellable,
                             ide_build_manager_install_cb,
                             g_object_ref (task));

  ide_build_manager_set_build_result (self, build_result);

  /*
   * Update our last build time.
   */
  g_clear_pointer (&self->last_build_time, g_date_time_unref);
  self->last_build_time = g_date_time_new_now_local ();
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LAST_BUILD_TIME]);

  IDE_EXIT;
}

gboolean
ide_build_manager_install_finish (IdeBuildManager  *self,
                                  GAsyncResult     *result,
                                  GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);
  g_return_val_if_fail (g_task_is_valid (G_TASK (result), self), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

gboolean
ide_build_manager_get_busy (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), FALSE);

  if (self->build_result != NULL)
    return ide_build_result_get_running (self->build_result);

  return FALSE;
}

void
ide_build_manager_cancel (IdeBuildManager *self)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_BUILD_MANAGER (self));

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  IDE_EXIT;
}

gchar *
ide_build_manager_get_message (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  if (self->build_result != NULL)
    return ide_build_result_get_mode (self->build_result);

  return g_strdup (_("Ready"));
}

/**
 * ide_build_manager_get_last_build_time:
 * @self: An #IdeBuildManager
 *
 * Gets the time the last build was started. This is %NULL until a build
 * has been executed in the context.
 *
 * Returns: (nullable) (transfer none): A #GDateTime or %NULL.
 */
GDateTime *
ide_build_manager_get_last_build_time (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), NULL);

  return self->last_build_time;
}

GTimeSpan
ide_build_manager_get_running_time (IdeBuildManager *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_MANAGER (self), 0);

  if (self->build_result == NULL)
    return 0;

  return ide_build_result_get_running_time (self->build_result);
}

static gchar **
ide_build_manager_list_actions (GActionGroup *action_group)
{
  IdeBuildManager *self = (IdeBuildManager *)action_group;

  g_assert (IDE_IS_BUILD_MANAGER (self));

  return g_action_group_list_actions (G_ACTION_GROUP (self->actions));
}

static gboolean
ide_build_manager_query_action (GActionGroup        *action_group,
                                const gchar         *action_name,
                                gboolean            *enabled,
                                const GVariantType **parameter_type,
                                const GVariantType **state_type,
                                GVariant           **state_hint,
                                GVariant           **state)
{
  IdeBuildManager *self = (IdeBuildManager *)action_group;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (action_name != NULL);

  return g_action_group_query_action (G_ACTION_GROUP (self->actions),
                                      action_name,
                                      enabled,
                                      parameter_type,
                                      state_type,
                                      state_hint,
                                      state);
}

static void
ide_build_manager_change_action_state (GActionGroup *action_group,
                                       const gchar  *action_name,
                                       GVariant     *value)
{
  IdeBuildManager *self = (IdeBuildManager *)action_group;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (action_name != NULL);

  g_action_group_change_action_state (G_ACTION_GROUP (self->actions), action_name, value);
}

static void
ide_build_manager_activate_action (GActionGroup *action_group,
                                   const gchar  *action_name,
                                   GVariant     *parameter)
{
  IdeBuildManager *self = (IdeBuildManager *)action_group;

  g_assert (IDE_IS_BUILD_MANAGER (self));
  g_assert (action_name != NULL);

  g_action_group_activate_action (G_ACTION_GROUP (self->actions), action_name, parameter);
}

static void
action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = ide_build_manager_list_actions;
  iface->query_action = ide_build_manager_query_action;
  iface->change_action_state = ide_build_manager_change_action_state;
  iface->activate_action = ide_build_manager_activate_action;
}
