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
#include "buildsystem/ide-build-target.h"
#include "buildsystem/ide-configuration.h"
#include "buildsystem/ide-configuration-manager.h"
#include "runner/ide-run-manager.h"
#include "runner/ide-runner.h"
#include "runtimes/ide-runtime.h"

struct _IdeRunManager
{
  IdeObject           parent_instance;

  GCancellable       *cancellable;
  GSimpleActionGroup *actions;

  guint               busy : 1;
};

G_DEFINE_TYPE (IdeRunManager, ide_run_manager, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BUSY,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_run_manager_finalize (GObject *object)
{
  IdeRunManager *self = (IdeRunManager *)object;

  g_clear_object (&self->cancellable);
  g_clear_object (&self->actions);

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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_run_manager_class_init (IdeRunManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_run_manager_finalize;
  object_class->get_property = ide_run_manager_get_property;

  properties [PROP_BUSY] =
    g_param_spec_boolean ("busy",
                          "Busy",
                          "Busy",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_run_manager_init (IdeRunManager *self)
{
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
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_TASK (task));

  if (!ide_runner_run_finish (runner, result, &error))
    {
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  g_task_return_boolean (task, TRUE);

failure:
  IDE_EXIT;
}

static void
ide_run_manager_install_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeBuildManager *build_manager = (IdeBuildManager *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeRunner) runner = NULL;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;
  IdeBuildTarget *build_target;
  IdeRunManager *self;
  GCancellable *cancellable;
  IdeContext *context;
  IdeRuntime *runtime;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (G_IS_TASK (task));

  if (!ide_build_manager_build_finish (build_manager, result, &error))
    {
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  build_target = g_task_get_task_data (task);
  self = g_task_get_source_object (task);

  g_assert (IDE_IS_BUILD_TARGET (build_target));
  g_assert (IDE_IS_RUN_MANAGER (self));

  context = ide_object_get_context (IDE_OBJECT (self));

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
      IDE_GOTO (failure);
    }

  runner = ide_runtime_create_runner (runtime, build_target);
  cancellable = g_task_get_cancellable (task);

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_runner_run_async (runner,
                        cancellable,
                        ide_run_manager_run_cb,
                        g_steal_pointer (&task));

failure:
  IDE_EXIT;
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

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

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
  IdeBuildManager *build_manager;
  IdeContext *context;
  GError *error = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_RUN_MANAGER (self));
  g_return_if_fail (IDE_IS_BUILD_TARGET (build_target));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (cancellable == NULL)
    cancellable = local_cancellable = g_cancellable_new ();

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_run_manager_run_async);
  g_task_set_task_data (task, g_object_ref (build_target), g_object_unref);

  if (ide_run_manager_check_busy (self, &error))
    {
      g_task_return_error (task, error);
      IDE_GOTO (failure);
    }

  /*
   * First we need to make sure the target is up to date and installed
   * so that all the dependent resources are available.
   */

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);

  self->busy = TRUE;

  g_set_object (&self->cancellable, cancellable);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_run_manager_task_completed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_build_manager_install_async (build_manager,
                                   cancellable,
                                   ide_run_manager_install_cb,
                                   g_steal_pointer (&task));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

failure:
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
  GCancellable *cancellable = user_data;

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
