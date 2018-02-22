/* ide-sysroot-runtime.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, eitIher version 3 of the License, or
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

#define G_LOG_DOMAIN "ide-sysroot-runtime"

#include "config.h"

#include "ide-sysroot-runtime.h"
#include "ide-sysroot-manager.h"
#include "ide-host-subprocess-launcher.h"

// This is a list of common libdirs to use
#define BASIC_LIBDIRS "/usr/lib/pkgconfig:/usr/share/pkgconfig"
#define RUNTIME_PREFIX "sysroot:"

struct _IdeSysrootRuntime
{
  IdeRuntime  parent_instance;
};

G_DEFINE_TYPE (IdeSysrootRuntime, ide_sysroot_runtime, IDE_TYPE_RUNTIME)

IdeSysrootRuntime *
ide_sysroot_runtime_new (IdeContext  *context,
                         const gchar *sysroot_id)
{
  g_autoptr(IdeRuntime) runtime = NULL;
  g_autofree gchar *built_id = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (sysroot_id != NULL, NULL);

  built_id = g_strconcat (RUNTIME_PREFIX, sysroot_id, NULL);
  runtime = g_object_new (IDE_TYPE_SYSROOT_RUNTIME,
                          "id", g_steal_pointer (&built_id),
                          "context", context,
                          "display-name", "",
                          NULL);
  return g_steal_pointer (&runtime);
}

const gchar *
ide_sysroot_runtime_get_sysroot_id (IdeSysrootRuntime *self)
{
  const gchar *runtime_id = ide_runtime_get_id (IDE_RUNTIME (self));

  if (!g_str_has_prefix (runtime_id, RUNTIME_PREFIX))
    return runtime_id;

  return runtime_id + strlen(RUNTIME_PREFIX);
}

static IdeSubprocessLauncher *
ide_sysroot_runtime_create_launcher (IdeRuntime  *runtime,
                                     GError      **error)
{
  IdeSubprocessLauncher *ret;
  IdeSysrootRuntime *self = IDE_SYSROOT_RUNTIME(runtime);

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SYSROOT_RUNTIME (self), NULL);

  ret = (IdeSubprocessLauncher *)ide_host_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);

  if (ret != NULL)
    {
      IdeSysrootManager *sysroot_manager = NULL;
      const gchar *env_var = NULL;
      const gchar *sysroot_id = NULL;
      g_autofree gchar *sysroot_cflags = NULL;
      g_autofree gchar *sysroot_libdirs = NULL;
      g_auto(GStrv) path_parts = NULL;
      g_autofree gchar *sysroot_path = NULL;
      g_autofree gchar *pkgconfig_dirs = NULL;

      sysroot_id = ide_sysroot_runtime_get_sysroot_id (self);

      sysroot_manager = ide_sysroot_manager_get_default ();

      ide_subprocess_launcher_set_run_on_host (ret, TRUE);
      ide_subprocess_launcher_set_clear_env (ret, FALSE);

      sysroot_path = ide_sysroot_manager_get_target_path (sysroot_manager, sysroot_id);

      env_var = ide_subprocess_launcher_getenv (ret, "CFLAGS");
      sysroot_cflags = g_strconcat ("--sysroot=", sysroot_path, NULL);
      ide_subprocess_launcher_setenv (ret, "CFLAGS", g_strjoin (" ", sysroot_cflags, env_var, NULL), TRUE);

      ide_subprocess_launcher_setenv (ret, "PKG_CONFIG_DIR", "", TRUE);

      ide_subprocess_launcher_setenv (ret, "PKG_CONFIG_SYSROOT_DIR", g_strdup (sysroot_path), TRUE);

      // Prepend the sysroot path to the BASIC_LIBDIRS values
      path_parts = g_strsplit (BASIC_LIBDIRS, ":", 0);
      for (gint i = g_strv_length (path_parts) - 1; i >= 0; i--)
        {
          g_autofree gchar *path_i = NULL;
          g_autofree gchar *libdir_tmp = NULL;

          path_i = g_build_path (G_DIR_SEPARATOR_S, sysroot_path, path_parts[i], NULL);
          libdir_tmp = g_strjoin (":", path_i, sysroot_libdirs, NULL);
          sysroot_libdirs = g_steal_pointer (&libdir_tmp);
        }

      pkgconfig_dirs = ide_sysroot_manager_get_target_pkg_config_path (sysroot_manager, sysroot_id);
      if (pkgconfig_dirs != NULL && g_strcmp0 (pkgconfig_dirs, "") != 0)
        {
          g_autofree gchar *libdir_tmp = NULL;

          libdir_tmp = g_strjoin (":", pkgconfig_dirs, sysroot_libdirs, NULL);
          sysroot_libdirs = g_steal_pointer (&libdir_tmp);
        }

      ide_subprocess_launcher_setenv (ret, "PKG_CONFIG_LIBDIR", sysroot_libdirs, TRUE);
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


static void
sysroot_runtime_target_name_changed (IdeSysrootRuntime *self,
                                     gchar             *target,
                                     gchar             *new_name,
                                     gpointer           user_data)
{
  const gchar* sysroot_id = ide_sysroot_runtime_get_sysroot_id (self);

  if (g_strcmp0 (target, sysroot_id) == 0)
    ide_runtime_set_display_name (IDE_RUNTIME (self), new_name);
}

static void
ide_sysroot_runtime_constructed (GObject *object)
{
  IdeSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *display_name = NULL;
  const gchar* sysroot_id = NULL;

  sysroot_id = ide_sysroot_runtime_get_sysroot_id (IDE_SYSROOT_RUNTIME (object));
  sysroot_manager = ide_sysroot_manager_get_default ();
  display_name = ide_sysroot_manager_get_target_name (sysroot_manager, sysroot_id);
  ide_runtime_set_display_name (IDE_RUNTIME (object), display_name);

  g_signal_connect_swapped (sysroot_manager, "target-name-changed", G_CALLBACK (sysroot_runtime_target_name_changed), object);
}

static void
ide_sysroot_runtime_class_init (IdeSysrootRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->constructed = ide_sysroot_runtime_constructed;

  runtime_class->create_launcher = ide_sysroot_runtime_create_launcher;
}

static void
ide_sysroot_runtime_init (IdeSysrootRuntime *self)
{
  
}

