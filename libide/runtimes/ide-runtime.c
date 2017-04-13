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

#include "ide-context.h"
#include "ide-debug.h"

#include "buildsystem/ide-configuration.h"
#include "projects/ide-project.h"
#include "runtimes/ide-runtime.h"
#include "subprocess/ide-subprocess.h"
#include "subprocess/ide-subprocess-launcher.h"
#include "util/ide-flatpak.h"

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

static IdeSubprocessLauncher *
ide_runtime_real_create_launcher (IdeRuntime  *self,
                                  GError     **error)
{
  IdeSubprocessLauncher *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_RUNTIME (self));

  ret = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

  if (ret != NULL)
    {
      ide_subprocess_launcher_set_run_on_host (ret, TRUE);
      ide_subprocess_launcher_set_clear_env (ret, FALSE);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "An unknown error ocurred");
    }

  IDE_RETURN (ret);
}

static gboolean
ide_runtime_real_contains_program_in_path (IdeRuntime   *self,
                                           const gchar  *program,
                                           GCancellable *cancellable)
{
  g_assert (IDE_IS_RUNTIME (self));
  g_assert (program != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!ide_is_flatpak ())
    {
      g_autofree gchar *path = NULL;
      path = g_find_program_in_path (program);
      return path != NULL;
    }
  else
    {
      g_autoptr(IdeSubprocessLauncher) launcher = NULL;

      /*
       * If we are in flatpak, we have to execute a program on the host to
       * determine if there is a program available, as we cannot resolve
       * file paths from inside the mount namespace.
       */

      if (NULL != (launcher = ide_runtime_create_launcher (self, NULL)))
        {
          g_autoptr(IdeSubprocess) subprocess = NULL;

          ide_subprocess_launcher_set_run_on_host (launcher, TRUE);
          ide_subprocess_launcher_push_argv (launcher, "which");
          ide_subprocess_launcher_push_argv (launcher, program);

          if (NULL != (subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, NULL)))
            return ide_subprocess_wait_check (subprocess, NULL, NULL);
        }

      return FALSE;
    }

  g_assert_not_reached ();
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

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (IDE_IS_CONFIGURATION (configuration));

  if (NULL == ide_configuration_get_prefix (configuration))
    {
      g_autofree gchar *install_path = NULL;
      IdeContext *context;
      IdeProject *project;
      const gchar *project_id;

      context = ide_object_get_context (IDE_OBJECT (self));
      project = ide_context_get_project (context);
      project_id = ide_project_get_id (project);

      install_path = g_build_filename (g_get_user_cache_dir (),
                                       "gnome-builder",
                                       "install",
                                       project_id,
                                       priv->id,
                                       NULL);

      ide_configuration_set_prefix (configuration, install_path);
    }
}

static IdeRunner *
ide_runtime_real_create_runner (IdeRuntime     *self,
                                IdeBuildTarget *build_target)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *binpath = NULL;
  g_autofree gchar *schemadir = NULL;
  g_autofree gchar *parentpath = NULL;
  g_autoptr(GFile) installdir = NULL;
  g_autoptr(GFile) parentdir = NULL;
  g_autoptr(GFile) bin = NULL;
  IdeContext *context;
  IdeRunner *runner;
  IdeEnvironment *env;
  gchar *slash;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (IDE_IS_BUILD_TARGET (build_target));

  context = ide_object_get_context (IDE_OBJECT (self));

  g_assert (IDE_IS_CONTEXT (context));

  runner = ide_runner_new (context);

  g_assert (IDE_IS_RUNNER (runner));

  g_object_get (build_target,
                "install-directory", &installdir,
                "name", &name,
                NULL);

  /* Targets might be relative in autotools */
  if ((slash = strrchr (name, '/')))
    {
      gchar *tmp = g_strdup (slash + 1);
      g_free (name);
      name = tmp;
    }

  if (installdir != NULL)
    {
      /* GSettings requires an env var for non-standard dirs */
      parentdir = g_file_get_parent (installdir);
      if (parentdir)
        {
          parentpath = g_file_get_path (parentdir);
          schemadir = g_build_filename (parentpath, "share",
                                    "glib-2.0", "schemas", NULL);

          env = ide_runner_get_environment (runner);
          ide_environment_setenv (env, "GSETTINGS_SCHEMA_DIR", schemadir);
        }

      bin = g_file_get_child (installdir, name);
      binpath = g_file_get_path (bin);

      ide_runner_append_argv (runner, binpath);
    }
  else
    ide_runner_append_argv (runner, name);


  return runner;
}

static void
ide_runtime_finalize (GObject *object)
{
  IdeRuntime *self = (IdeRuntime *)object;
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
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

  klass->create_launcher = ide_runtime_real_create_launcher;
  klass->create_runner = ide_runtime_real_create_runner;
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
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  return g_object_new (IDE_TYPE_RUNTIME,
                       "context", context,
                       "id", id,
                       "display-name", display_name,
                       NULL);
}

/**
 * ide_runtime_create_launcher:
 *
 * Creates a launcher for the runtime.
 *
 * This can be used to execute a command within a runtime.
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

/**
 * ide_runtime_create_runner:
 *
 * Creates a new runner that can be used to execute the build target within
 * the runtime. This should be used to implement such features as "run target"
 * or "run unit test" inside the target runtime.
 *
 * Returns: (transfer full) (nullable): An #IdeRunner if successful, otherwise
 *   %NULL and @error is set.
 */
IdeRunner *
ide_runtime_create_runner (IdeRuntime     *self,
                           IdeBuildTarget *build_target)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);
  g_return_val_if_fail (IDE_IS_BUILD_TARGET (build_target), NULL);

  return IDE_RUNTIME_GET_CLASS (self)->create_runner (self, build_target);
}

GQuark
ide_runtime_error_quark (void)
{
  static GQuark quark = 0;

  if G_UNLIKELY (quark == 0)
    quark = g_quark_from_static_string ("ide_runtime_error_quark");

  return quark;
}

/**
 * ide_runtime_translate_file:
 * @self: An #IdeRuntime
 * @file: A #GFile
 *
 * Translates the file from a path within the runtime to a path that can
 * be accessed from the host system.
 *
 * Returns: (transfer full) (not nullable): A #GFile.
 */
GFile *
ide_runtime_translate_file (IdeRuntime *self,
                            GFile      *file)
{
  GFile *ret = NULL;

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (IDE_RUNTIME_GET_CLASS (self)->translate_file)
    ret = IDE_RUNTIME_GET_CLASS (self)->translate_file (self, file);

  if (ret == NULL)
    ret = g_object_ref (file);

  return ret;
}
