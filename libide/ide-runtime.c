/* ide-runtime.c
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

#define G_LOG_DOMAIN "ide-runtime"

#include "ide-builder.h"
#include "ide-configuration.h"
#include "ide-context.h"
#include "ide-project.h"
#include "ide-runtime.h"

typedef struct
{
  gchar *id;
  gchar *display_name;
} IdeRuntimePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeRuntime, ide_runtime, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_DISPLAY_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_runtime_real_prebuild_async (IdeRuntime          *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_runtime_real_prebuild_finish (IdeRuntime    *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_RUNTIME (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_runtime_real_postbuild_async (IdeRuntime          *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_boolean (task, TRUE);
}

static gboolean
ide_runtime_real_postbuild_finish (IdeRuntime    *self,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_assert (IDE_IS_RUNTIME (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static IdeSubprocessLauncher *
ide_runtime_real_create_launcher (IdeRuntime  *self,
                                  GError     **error)
{
  IdeSubprocessLauncher *ret;
  g_auto(GStrv) env = NULL;

  g_assert (IDE_IS_RUNTIME (self));

  env = g_get_environ ();

  ret = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
  ide_subprocess_launcher_set_environ (ret, (const gchar * const *)env);

  return ret;
}

static gboolean
ide_runtime_real_contains_program_in_path (IdeRuntime   *self,
                                           const gchar  *program,
                                           GCancellable *cancellable)
{
  gchar *path;
  gboolean ret;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = g_find_program_in_path (program);
  ret = path != NULL;
  g_free (path);

  return ret;
}

gboolean
ide_runtime_contains_program_in_path (IdeRuntime   *self,
                                      const gchar  *program,
                                      GCancellable *cancellable)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), FALSE);
  g_return_val_if_fail (program != NULL, FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  return IDE_RUNTIME_GET_CLASS (self)->contains_program_in_path (self, program, cancellable);
}

static void
ide_runtime_real_prepare_configuration (IdeRuntime       *self,
                                        IdeConfiguration *configuration)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);
  g_autofree gchar *install_path = NULL;
  IdeContext *context;
  IdeProject *project;
  const gchar *project_name;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  context = ide_object_get_context (IDE_OBJECT (self));
  project = ide_context_get_project (context);
  project_name = ide_project_get_name (project);

  install_path = g_build_filename (g_get_user_cache_dir (),
                                   "gnome-builder",
                                   "install",
                                   project_name,
                                   priv->id,
                                   NULL);

  ide_configuration_set_prefix (configuration, install_path);
}

static void
ide_runtime_finalize (GObject *object)
{
  IdeRuntime *self = (IdeRuntime *)object;
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_clear_pointer (&priv->display_name, g_free);

  G_OBJECT_CLASS (ide_runtime_parent_class)->finalize (object);
}

static void
ide_runtime_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  IdeRuntime *self = IDE_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_runtime_get_id (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_runtime_get_display_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runtime_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  IdeRuntime *self = IDE_RUNTIME (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_runtime_set_id (self, g_value_get_string (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_runtime_set_display_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_runtime_class_init (IdeRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_runtime_finalize;
  object_class->get_property = ide_runtime_get_property;
  object_class->set_property = ide_runtime_set_property;

  klass->prebuild_async = ide_runtime_real_prebuild_async;
  klass->prebuild_finish = ide_runtime_real_prebuild_finish;
  klass->postbuild_async = ide_runtime_real_postbuild_async;
  klass->postbuild_finish = ide_runtime_real_postbuild_finish;
  klass->create_launcher = ide_runtime_real_create_launcher;
  klass->contains_program_in_path = ide_runtime_real_contains_program_in_path;
  klass->prepare_configuration = ide_runtime_real_prepare_configuration;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The runtime identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "Display Name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_runtime_init (IdeRuntime *self)
{
}

const gchar *
ide_runtime_get_id (IdeRuntime  *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return priv->id;
}

void
ide_runtime_set_id (IdeRuntime  *self,
                    const gchar *id)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (id != NULL);

  if (0 != g_strcmp0 (id, priv->id))
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const gchar *
ide_runtime_get_display_name (IdeRuntime *self)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return priv->display_name;
}

void
ide_runtime_set_display_name (IdeRuntime  *self,
                              const gchar *display_name)
{
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (display_name != NULL);

  if (g_strcmp0 (display_name, priv->display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

IdeRuntime *
ide_runtime_new (IdeContext  *context,
                 const gchar *id,
                 const gchar *display_name)
{
  return g_object_new (IDE_TYPE_RUNTIME,
                       "context", context,
                       "id", id,
                       "display-name", display_name,
                       NULL);
}

void
ide_runtime_prebuild_async (IdeRuntime          *self,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNTIME_GET_CLASS (self)->prebuild_async (self, cancellable, callback, user_data);
}

gboolean
ide_runtime_prebuild_finish (IdeRuntime    *self,
                             GAsyncResult  *result,
                             GError       **error)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), FALSE);

  return IDE_RUNTIME_GET_CLASS (self)->prebuild_finish (self, result, error);
}

void
ide_runtime_postbuild_async (IdeRuntime          *self,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_RUNTIME_GET_CLASS (self)->postbuild_async (self, cancellable, callback, user_data);
}

gboolean
ide_runtime_postbuild_finish (IdeRuntime    *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), FALSE);

  return IDE_RUNTIME_GET_CLASS (self)->postbuild_finish (self, result, error);
}

/**
 * ide_runtime_create_launcher:
 *
 * Creates a launcher for the runtime.
 *
 * This can be used to execute a command within a runtime.
 * If you are doing a build, you probably want to ensure you call
 * ide_runtime_prebuild_async() before using the launcher.
 *
 * It is important that this function can be run from a thread without
 * side effects.
 *
 * Returns: (transfer full): An #IdeSubprocessLauncher or %NULL upon failure.
 */
IdeSubprocessLauncher *
ide_runtime_create_launcher (IdeRuntime  *self,
                             GError     **error)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  return IDE_RUNTIME_GET_CLASS (self)->create_launcher (self, error);
}

void
ide_runtime_prepare_configuration (IdeRuntime       *self,
                                   IdeConfiguration *configuration)
{
  g_return_if_fail (IDE_IS_RUNTIME (self));
  g_return_if_fail (IDE_IS_CONFIGURATION (configuration));

  IDE_RUNTIME_GET_CLASS (self)->prepare_configuration (self, configuration);
}
