/* ide-build-command.c
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

#define G_LOG_DOMAIN "ide-build-command"

#include "ide-debug.h"

#include "buildsystem/ide-build-command.h"
#include "buildsystem/ide-build-result.h"
#include "buildsystem/ide-environment.h"
#include "runtimes/ide-runtime.h"
#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"

typedef struct
{
  gchar *command_text;
} IdeBuildCommandPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildCommand, ide_build_command, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COMMAND_TEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_build_command_wait_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!ide_subprocess_wait_finish (subprocess, result, &error))
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static IdeSubprocessLauncher *
create_launcher (IdeBuildCommand  *self,
                 IdeRuntime       *runtime,
                 IdeEnvironment   *environment,
                 IdeBuildResult   *build_result,
                 const gchar      *command_text,
                 GError          **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;

  g_assert (IDE_IS_BUILD_COMMAND (self));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (IDE_IS_ENVIRONMENT (environment));
  g_assert (IDE_IS_BUILD_RESULT (build_result));

  if (command_text == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "No command was specified");
      return NULL;
    }

  if (NULL == (launcher = ide_runtime_create_launcher (runtime, error)))
    return NULL;

  ide_subprocess_launcher_set_flags (launcher, (G_SUBPROCESS_FLAGS_STDERR_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE));
  ide_subprocess_launcher_overlay_environment (launcher, environment);

  /* TODO: ide_subprocess_launcher_set_cwd (launcher, builddir); */
  /* TODO: set $BUILDDIR and $SRCDIR for scripts? */

  ide_subprocess_launcher_push_argv (launcher, "sh");
  ide_subprocess_launcher_push_argv (launcher, "-c");
  ide_subprocess_launcher_push_argv (launcher, command_text);

  return g_steal_pointer (&launcher);
}

static gboolean
ide_build_command_real_run (IdeBuildCommand  *self,
                            IdeRuntime       *runtime,
                            IdeEnvironment   *environment,
                            IdeBuildResult   *build_result,
                            GCancellable     *cancellable,
                            GError          **error)
{
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_COMMAND (self));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (IDE_IS_ENVIRONMENT (environment));
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  launcher = create_launcher (self,
                              runtime,
                              environment,
                              build_result,
                              priv->command_text,
                              error);
  if (launcher == NULL)
    IDE_RETURN (FALSE);

  subprocess = ide_subprocess_launcher_spawn_sync (launcher, cancellable, error);
  if (subprocess == NULL)
    return FALSE;

  ide_build_result_log_subprocess (build_result, subprocess);

  ret = ide_subprocess_wait (subprocess, cancellable, error);

  IDE_RETURN (ret);
}

static void
ide_build_command_real_run_async (IdeBuildCommand     *self,
                                  IdeRuntime          *runtime,
                                  IdeEnvironment      *environment,
                                  IdeBuildResult      *build_result,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GTask) task = NULL;
  GError *error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_COMMAND (self));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (IDE_IS_ENVIRONMENT (environment));
  g_assert (IDE_IS_BUILD_RESULT (build_result));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_command_real_run_async);

  launcher = create_launcher (self,
                              runtime,
                              environment,
                              build_result,
                              priv->command_text,
                              &error);

  if (launcher == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  subprocess = ide_subprocess_launcher_spawn_sync (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, error);
      IDE_EXIT;
    }

  ide_build_result_log_subprocess (build_result, subprocess);

  ide_subprocess_wait_async (subprocess,
                             cancellable,
                             ide_build_command_wait_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_build_command_real_run_finish (IdeBuildCommand  *self,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_COMMAND (self));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static IdeBuildCommand *
ide_build_command_real_copy (IdeBuildCommand *self)
{
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);

  g_assert (IDE_IS_BUILD_COMMAND (self));

  return g_object_new (G_OBJECT_TYPE (self),
                       "command-text", priv->command_text,
                       NULL);
}

static void
ide_build_command_finalize (GObject *object)
{
  IdeBuildCommand *self = (IdeBuildCommand *)object;
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);

  g_clear_pointer (&priv->command_text, g_free);

  G_OBJECT_CLASS (ide_build_command_parent_class)->finalize (object);
}

static void
ide_build_command_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeBuildCommand *self = IDE_BUILD_COMMAND (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      g_value_set_string (value, ide_build_command_get_command_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_command_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeBuildCommand *self = IDE_BUILD_COMMAND (object);

  switch (prop_id)
    {
    case PROP_COMMAND_TEXT:
      ide_build_command_set_command_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_command_class_init (IdeBuildCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_command_finalize;
  object_class->get_property = ide_build_command_get_property;
  object_class->set_property = ide_build_command_set_property;

  klass->copy = ide_build_command_real_copy;
  klass->run = ide_build_command_real_run;
  klass->run_async = ide_build_command_real_run_async;
  klass->run_finish = ide_build_command_real_run_finish;

  properties [PROP_COMMAND_TEXT] =
    g_param_spec_string ("command-text",
                         "Command Text",
                         "Command Text",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_build_command_init (IdeBuildCommand *self)
{
}

gboolean
ide_build_command_run (IdeBuildCommand  *self,
                       IdeRuntime       *runtime,
                       IdeEnvironment   *environment,
                       IdeBuildResult   *build_result,
                       GCancellable     *cancellable,
                       GError          **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_COMMAND (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNTIME (runtime), FALSE);
  g_return_val_if_fail (IDE_IS_ENVIRONMENT (environment), FALSE);
  g_return_val_if_fail (IDE_IS_BUILD_RESULT (build_result), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return IDE_BUILD_COMMAND_GET_CLASS (self)->run (self, runtime, environment, build_result, cancellable, error);
}

void
ide_build_command_run_async (IdeBuildCommand     *self,
                             IdeRuntime          *runtime,
                             IdeEnvironment      *environment,
                             IdeBuildResult      *build_result,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_BUILD_COMMAND (self));
  g_return_if_fail (IDE_IS_RUNTIME (runtime));
  g_return_if_fail (IDE_IS_ENVIRONMENT (environment));
  g_return_if_fail (IDE_IS_BUILD_RESULT (build_result));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_COMMAND_GET_CLASS (self)->run_async (self, runtime, environment, build_result, cancellable, callback, user_data);
}

gboolean
ide_build_command_run_finish (IdeBuildCommand  *self,
                              GAsyncResult     *result,
                              GError          **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_COMMAND (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return IDE_BUILD_COMMAND_GET_CLASS (self)->run_finish (self, result, error);
}

IdeBuildCommand *
ide_build_command_new (void)
{
  return g_object_new (IDE_TYPE_BUILD_COMMAND, NULL);
}

const gchar *
ide_build_command_get_command_text (IdeBuildCommand *self)
{
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_COMMAND (self), NULL);

  return priv->command_text;
}

void
ide_build_command_set_command_text (IdeBuildCommand *self,
                                    const gchar     *command_text)
{
  IdeBuildCommandPrivate *priv = ide_build_command_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_COMMAND (self));

  if (command_text != priv->command_text)
    {
      g_free (priv->command_text);
      priv->command_text = g_strdup (command_text);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COMMAND_TEXT]);
    }
}

/**
 * ide_build_command_copy:
 *
 * Returns: (transfer full): An #IdeBuildCommand
 */
IdeBuildCommand *
ide_build_command_copy (IdeBuildCommand *self)
{
  return IDE_BUILD_COMMAND_GET_CLASS (self)->copy (self);
}
