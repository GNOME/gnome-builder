/* ide-runtime.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include <dazzle.h>
#include <string.h>

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
  IdeRuntimePrivate *priv = ide_runtime_get_instance_private (self);
  g_autoptr(GFile) installdir = NULL;
  g_auto(GStrv) argv = NULL;
  IdeContext *context;
  IdeRunner *runner;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (!build_target || IDE_IS_BUILD_TARGET (build_target));

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  runner = ide_runner_new (context);
  g_assert (IDE_IS_RUNNER (runner));

  if (dzl_str_equal0 (priv->id, "host"))
    ide_runner_set_run_on_host (runner, TRUE);

  if (build_target != NULL)
    {
      installdir = ide_build_target_get_install_directory (build_target);
      argv = ide_build_target_get_argv (build_target);
    }

  /* Possibly translate relative paths for the binary */
  if (argv && argv[0] && !g_path_is_absolute (argv[0]))
    {
      const gchar *slash = strchr (argv[0], '/');
      g_autofree gchar *copy = g_strdup (slash ? (slash + 1) : argv[0]);

      g_free (argv[0]);

      if (installdir != NULL)
        {
          g_autoptr(GFile) dest = g_file_get_child (installdir, copy);
          argv[0] = g_file_get_path (dest);
        }
      else
        argv[0] = g_steal_pointer (&copy);
    }

  if (installdir != NULL)
    {
      g_autoptr(GFile) parentdir = NULL;
      g_autofree gchar *schemadir = NULL;
      g_autofree gchar *parentpath = NULL;

      /* GSettings requires an env var for non-standard dirs */
      if (NULL != (parentdir = g_file_get_parent (installdir)))
        {
          IdeEnvironment *env = ide_runner_get_environment (runner);

          parentpath = g_file_get_path (parentdir);
          schemadir = g_build_filename (parentpath, "share", "glib-2.0", "schemas", NULL);
          ide_environment_setenv (env, "GSETTINGS_SCHEMA_DIR", schemadir);
        }
    }

  if (argv != NULL)
    ide_runner_push_args (runner, (const gchar * const *)argv);

  return runner;
}

static GFile *
ide_runtime_real_translate_file (IdeRuntime *self,
                                 GFile      *file)
{
  g_autofree gchar *path = NULL;

  g_assert (IDE_IS_RUNTIME (self));
  g_assert (G_IS_FILE (file));

  /* We only need to translate when running as flatpak */
  if (!ide_is_flatpak ())
    return NULL;

  /* Only deal with native files */
  if (!g_file_is_native (file) || NULL == (path = g_file_get_path (file)))
    return NULL;

  /* If this is /usr or /etc, then translate to /run/host/$dir,
   * as that is where flatpak 0.10.1 and greater will mount them
   * when --filesystem=host.
   */
  if (g_str_has_prefix (path, "/usr/") || g_str_has_prefix (path, "/etc/"))
    return g_file_new_build_filename ("/run/host/", path, NULL);

  return NULL;
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
  klass->translate_file = ide_runtime_real_translate_file;

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
 * @self: An #IdeRuntime
 * @build_target: (nullable): An #IdeBuildTarget or %NULL
 *
 * Creates a new runner that can be used to execute the build target within
 * the runtime. This should be used to implement such features as "run target"
 * or "run unit test" inside the target runtime.
 *
 * If @build_target is %NULL, the runtime should create a runner that allows
 * the caller to specify the binary using the #IdeRunner API.
 *
 * Returns: (transfer full) (nullable): An #IdeRunner if successful, otherwise
 *   %NULL and @error is set.
 */
IdeRunner *
ide_runtime_create_runner (IdeRuntime     *self,
                           IdeBuildTarget *build_target)
{
  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);
  g_return_val_if_fail (!build_target || IDE_IS_BUILD_TARGET (build_target), NULL);

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
 * @file: a #GFile
 *
 * Translates the file from a path within the runtime to a path that can
 * be accessed from the host system.
 *
 * Returns: (transfer full) (not nullable): a #GFile.
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

/**
 * ide_runtime_get_system_include_dirs:
 * @self: a #IdeRuntime
 *
 * Gets the system include dirs for the runtime. Usually, this is just
 * "/usr/include", but more complex runtimes may include additional.
 *
 * Returns: (transfer full) (array zero-terminated=1): A newly allocated
 *   string containing the include dirs.
 *
 * Since: 3.28
 */
gchar **
ide_runtime_get_system_include_dirs (IdeRuntime *self)
{
  static const gchar *basic[] = { "/usr/include", NULL };

  g_return_val_if_fail (IDE_IS_RUNTIME (self), NULL);

  if (IDE_RUNTIME_GET_CLASS (self)->get_system_include_dirs)
    return IDE_RUNTIME_GET_CLASS (self)->get_system_include_dirs (self);

  return g_strdupv ((gchar **)basic);
}
