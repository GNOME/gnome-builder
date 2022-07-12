/* gbp-cmake-toolchain.c
 *
 * Copyright 2018 Collabora Ltd.
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
 *
 * Authors: Corentin Noël <corentin.noel@collabora.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-cmake-toolchain"

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "gbp-cmake-toolchain.h"

struct _GbpCMakeToolchain
{
  IdeSimpleToolchain      parent_instance;
  gchar                  *file_path;
};

G_DEFINE_FINAL_TYPE (GbpCMakeToolchain, gbp_cmake_toolchain, IDE_TYPE_SIMPLE_TOOLCHAIN)

enum {
  PROP_0,
  PROP_FILE_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpCMakeToolchain *
gbp_cmake_toolchain_new (IdeContext *context)
{
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GbpCMakeToolchain) toolchain = NULL;

  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  triplet = ide_triplet_new_from_system ();
  toolchain = g_object_new (GBP_TYPE_CMAKE_TOOLCHAIN,
                            "host-triplet", triplet,
                            NULL);

  return g_steal_pointer (&toolchain);
}

/**
 * gbp_cmake_toolchain_get_file_path:
 * @self: an #GbpCMakeToolchain
 *
 * Gets the path to the CMake cross-compilation definitions
 *
 * Returns: (transfer none): the path to the CMake cross-compilation definitions file.
 */
const gchar *
gbp_cmake_toolchain_get_file_path (GbpCMakeToolchain  *self)
{
  g_return_val_if_fail (GBP_IS_CMAKE_TOOLCHAIN (self), NULL);

  return self->file_path;
}

static gchar *
_gbp_cmake_toolchain_deploy_temporary_cmake (GbpCMakeToolchain *self,
                                             GCancellable      *cancellable)
{
  IdeContext *context;
  g_autofree gchar *defined_path = NULL;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN(self));

  context = ide_object_get_context (IDE_OBJECT(self));
  defined_path = ide_context_cache_filename (context, "cmake", "toolchain-detection", NULL);
  if (!g_file_test (defined_path, G_FILE_TEST_EXISTS))
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) tmp_file = g_file_new_for_path (defined_path);
      g_autoptr(GFile) cmake_lists_res = g_file_new_for_uri ("resource:///plugins/cmake/CMakeLists.txt");
      g_autoptr(GFile) cmake_ini_res = g_file_new_for_uri ("resource:///plugins/cmake/toolchain-info.ini.cmake");
      g_autoptr(GFile) cmake_lists = g_file_get_child (tmp_file, "CMakeLists.txt");
      g_autoptr(GFile) cmake_ini = g_file_get_child (tmp_file, "toolchain-info.ini.cmake");

      if (g_mkdir_with_parents (defined_path, 0750) != 0)
        {
          g_critical ("Error creating temporary CMake folder at %s", defined_path);
          return NULL;
        }

      g_file_copy (cmake_lists_res, cmake_lists, G_FILE_COPY_NONE, cancellable, NULL, NULL, &error);
      if (error != NULL)
        {
          g_critical ("Error creating temporary CMake folder: %s", error->message);
          return NULL;
        }

      g_file_copy (cmake_ini_res, cmake_ini, G_FILE_COPY_NONE, cancellable, NULL, NULL, &error);
      if (error != NULL)
        {
          g_critical ("Error creating temporary CMake folder: %s", error->message);
          return NULL;
        }
  }

  for (guint i = 0; i < G_MAXUINT; i++)
    {
      g_autofree gchar *build_folder = g_strdup_printf ("build%u", i);
      g_autofree gchar *builddir = g_build_filename (defined_path, build_folder, NULL);
      if (!g_file_test (builddir, G_FILE_TEST_EXISTS) && g_mkdir (builddir, 0750) == 0)
        return g_steal_pointer (&builddir);
    }

  return NULL;
}

static gboolean
_gbp_cmake_toolchain_parse_keyfile (GbpCMakeToolchain  *self,
                                    const gchar        *folder)
{
  g_autofree gchar *filename = g_build_filename (folder, "toolchain-info.ini", NULL);
  g_autoptr(GKeyFile) keyfile = g_key_file_new ();
  g_autoptr(IdeTriplet) host_triplet = NULL;
  g_auto(GStrv) compilers = NULL;
  gsize compilers_length;
  g_autofree gchar *system = NULL;
  g_autofree gchar *system_lowercase = NULL;
  g_autofree gchar *cpu = NULL;
  g_autofree gchar *pkg_config = NULL;
  g_autofree gchar *ar_path = NULL;
  g_autofree gchar *exec_path = NULL;

  if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL))
    return FALSE;

  cpu = g_key_file_get_string (keyfile, "general", "cpu", NULL);

  system = g_key_file_get_string (keyfile, "general", "system", NULL);
  if (system != NULL)
    system_lowercase = g_ascii_strdown (system, -1);

  host_triplet = ide_triplet_new_with_triplet (cpu, system_lowercase, NULL);
  ide_toolchain_set_host_triplet (IDE_TOOLCHAIN(self), host_triplet);

  exec_path = g_key_file_get_string (keyfile, "binaries", "exe_wrapper", NULL);
  if (exec_path != NULL && g_strcmp0 (exec_path, "") != 0)
    ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                IDE_TOOLCHAIN_TOOL_EXEC,
                                                exec_path);

  ar_path = g_key_file_get_string (keyfile, "binaries", "ar", NULL);
  if (ar_path != NULL && g_strcmp0 (ar_path, "") != 0)
    ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                IDE_TOOLCHAIN_TOOL_AR,
                                                ar_path);

  pkg_config = g_key_file_get_string (keyfile, "binaries", "pkg_config", NULL);
  if (pkg_config != NULL && g_strcmp0 (pkg_config, "") != 0)
    ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                IDE_TOOLCHAIN_LANGUAGE_ANY,
                                                IDE_TOOLCHAIN_TOOL_PKG_CONFIG,
                                                pkg_config);

  compilers = g_key_file_get_keys (keyfile, "compilers", &compilers_length, NULL);
  for (gint i = 0; i < compilers_length; i++)
    {
      g_autofree gchar *compiler_path = g_key_file_get_string (keyfile, "compilers", compilers[i], NULL);
      if (compiler_path != NULL && g_strcmp0 (compiler_path, "") != 0)
        ide_simple_toolchain_set_tool_for_language (IDE_SIMPLE_TOOLCHAIN(self),
                                                    compilers[i],
                                                    IDE_TOOLCHAIN_TOOL_CC,
                                                    compiler_path);
    }

  return TRUE;
}

/* It is far easier and more reliable to get the variables from cmake itself,
 * Here is a small projects that exports the content of the cross-file */
gboolean
gbp_cmake_toolchain_load (GbpCMakeToolchain *self,
                          GFile             *file,
                          GCancellable      *cancellable,
                          GError           **error)
{
  g_autofree gchar *build_dir = NULL;
  g_autofree gchar *toolchain_arg = NULL;
  g_autoptr(IdeSubprocessLauncher) cmake_launcher = NULL;
  g_autoptr(IdeSubprocess) cmake_subprocess = NULL;
  g_autofree gchar *id = NULL;
  g_autofree gchar *display_name = NULL;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN (self));

  g_clear_pointer (&self->file_path, g_free);
  self->file_path = g_file_get_path (file);

  id = g_strconcat ("cmake:", self->file_path, NULL);
  ide_toolchain_set_id (IDE_TOOLCHAIN(self), id);

  display_name = g_strdup_printf (_("%s (CMake)"), self->file_path);
  ide_toolchain_set_display_name (IDE_TOOLCHAIN(self), display_name);

  build_dir = _gbp_cmake_toolchain_deploy_temporary_cmake (self, cancellable);
  if (build_dir == NULL)
    return FALSE;

  toolchain_arg = g_strdup_printf ("-DCMAKE_TOOLCHAIN_FILE=%s", self->file_path);

  cmake_launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE|G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_push_argv (cmake_launcher, "cmake");
  ide_subprocess_launcher_push_argv (cmake_launcher, "..");
  ide_subprocess_launcher_push_argv (cmake_launcher, toolchain_arg);
  ide_subprocess_launcher_set_cwd (cmake_launcher, build_dir);
  cmake_subprocess = ide_subprocess_launcher_spawn (cmake_launcher, cancellable, error);
  if (cmake_subprocess == NULL)
    return FALSE;

  if (!ide_subprocess_wait_check (cmake_subprocess, cancellable, error))
    return FALSE;

  if (!_gbp_cmake_toolchain_parse_keyfile (self, build_dir))
    return FALSE;

  return TRUE;
}

static void
gbp_cmake_toolchain_finalize (GObject *object)
{
  GbpCMakeToolchain *self = (GbpCMakeToolchain *)object;

  g_clear_pointer (&self->file_path, g_free);

  G_OBJECT_CLASS (gbp_cmake_toolchain_parent_class)->finalize (object);
}

static void
gbp_cmake_toolchain_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbpCMakeToolchain *self = GBP_CMAKE_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_FILE_PATH:
      g_value_set_string (value, gbp_cmake_toolchain_get_file_path (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_cmake_toolchain_class_init (GbpCMakeToolchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_cmake_toolchain_finalize;
  object_class->get_property = gbp_cmake_toolchain_get_property;

  properties [PROP_FILE_PATH] =
    g_param_spec_string ("file-path",
                         "File path",
                         "The path of the cross-file",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_cmake_toolchain_init (GbpCMakeToolchain *self)
{
}
