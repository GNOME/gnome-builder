/* gbp-cmake-toolchain.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
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
 */

#define G_LOG_DOMAIN "gbp-cmake-toolchain"

#include <glib/gstdio.h>

#include "gbp-cmake-toolchain.h"

struct _GbpCMakeToolchain
{
  IdeToolchain            parent_instance;
  gchar                  *file_path;
  gchar                  *exe_wrapper;
  gchar                  *archiver;
  gchar                  *pkg_config;
  GHashTable             *compilers;
};

G_DEFINE_TYPE (GbpCMakeToolchain, gbp_cmake_toolchain, IDE_TYPE_TOOLCHAIN)

enum {
  PROP_0,
  PROP_FILE_PATH,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

GbpCMakeToolchain *
gbp_cmake_toolchain_new (IdeContext   *context,
                         GFile        *file)
{
  g_autofree gchar *path = g_file_get_path (file);
  g_autofree gchar *id = g_strconcat ("cmake:", path, NULL);
  g_autoptr(IdeTriplet) triplet = NULL;
  g_autoptr(GbpCMakeToolchain) toolchain = NULL;

  triplet = ide_triplet_new_from_system ();
  toolchain = g_object_new (GBP_TYPE_CMAKE_TOOLCHAIN,
                            "context", context,
                            "file-path", path,
                            "id", id,
                            "host-triplet", triplet,
                            NULL);

  return g_steal_pointer (&toolchain);
}

/**
 * ide_toolchain_get_id:
 * @self: an #IdeToolchain
 *
 * Gets the internal identifier of the toolchain
 *
 * Returns: (transfer none): the unique identifier.
 *
 * Since: 3.30
 */
const gchar *
gbp_cmake_toolchain_get_file_path (GbpCMakeToolchain  *self)
{
  g_return_val_if_fail (GBP_IS_CMAKE_TOOLCHAIN (self), NULL);

  return self->file_path;
}

void
gbp_cmake_toolchain_set_file_path (GbpCMakeToolchain  *self,
                                   const gchar        *file_path)
{
  g_return_if_fail (GBP_IS_CMAKE_TOOLCHAIN (self));
  g_return_if_fail (file_path != NULL);

  if (g_strcmp0 (file_path, self->file_path) != 0)
    {
      g_clear_pointer (&self->file_path, g_free);
      self->file_path = g_strdup (file_path);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_PATH]);
    }
}

static gchar *
_gbp_cmake_toolchain_deploy_temporary_cmake (GCancellable  *cancellable)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) cmake_lists_res = NULL;
  g_autoptr(GFile) cmake_ini_res = NULL;
  g_autoptr(GFile) cmake_lists = NULL;
  g_autoptr(GFile) cmake_ini = NULL;
  g_autoptr(GFile) tmp_file = NULL;
  g_autofree gchar *tmp_dir = NULL;

  tmp_dir = g_dir_make_tmp (".builder-cmake-XXXXXX", &error);
  if (error != NULL)
    {
      //TODO
      return NULL;
    }

  tmp_file = g_file_new_for_path (tmp_dir);
  cmake_lists_res = g_file_new_for_uri ("resource:///org/gnome/builder/plugins/cmake/CMakeLists.txt");
  cmake_ini_res = g_file_new_for_uri ("resource:///org/gnome/builder/plugins/cmake/toolchain-info.ini.cmake");
  cmake_lists = g_file_get_child (tmp_file, "CMakeLists.txt");
  cmake_ini = g_file_get_child (tmp_file, "toolchain-info.ini.cmake");

  g_file_copy (cmake_lists_res, cmake_lists, G_FILE_COPY_NONE, cancellable, NULL, NULL, &error);
  if (error != NULL)
    {
      //TODO
      return NULL;
    }

  g_file_copy (cmake_ini_res, cmake_ini, G_FILE_COPY_NONE, cancellable, NULL, NULL, &error);
  if (error != NULL)
    {
      //TODO
      return NULL;
    }

  return g_steal_pointer (&tmp_dir);
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

  if (!g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL))
    return FALSE;

  cpu = g_key_file_get_string (keyfile, "general", "cpu", NULL);

  system = g_key_file_get_string (keyfile, "general", "system", NULL);
  if (system != NULL)
    system_lowercase = g_ascii_strdown (system, -1);

  host_triplet = ide_triplet_new_with_triplet (cpu, system_lowercase, NULL);
  ide_toolchain_set_host_triplet (IDE_TOOLCHAIN(self), host_triplet);

  self->exe_wrapper = g_key_file_get_string (keyfile, "binaries", "exe_wrapper", NULL);
  self->archiver = g_key_file_get_string (keyfile, "binaries", "ar", NULL);
  self->pkg_config = g_key_file_get_string (keyfile, "binaries", "pkg_config", NULL);

  compilers = g_key_file_get_keys (keyfile, "compilers", &compilers_length, NULL);
  for (gint i = 0; i < compilers_length; i++)
    {
      g_autofree gchar *compiler_path = g_key_file_get_string (keyfile, "compilers", compilers[i], NULL);
      g_hash_table_insert (self->compilers, g_strdup (compilers[i]), g_steal_pointer (&compiler_path));
    }

  return TRUE;
}

const gchar *
gbp_cmake_toolchain_get_tool_for_language (IdeToolchain  *toolchain,
                                           const gchar   *language,
                                           const gchar   *tool_id)
{
  GbpCMakeToolchain *self = (GbpCMakeToolchain *)toolchain;

  g_return_val_if_fail (GBP_IS_CMAKE_TOOLCHAIN (self), NULL);
  g_return_val_if_fail (tool_id != NULL, NULL);

  return NULL;
}

static void
gbp_cmake_toolchain_verify_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  GbpCMakeToolchain *self = source_object;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *tmp_dir = NULL;
  g_autofree gchar *toolchain_arg = NULL;
  g_autoptr(IdeSubprocessLauncher) cmake_launcher = NULL;
  g_autoptr(IdeSubprocess) cmake_subprocess = NULL;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN (self));

  tmp_dir = _gbp_cmake_toolchain_deploy_temporary_cmake (cancellable);
  toolchain_arg = g_strdup_printf ("-DCMAKE_TOOLCHAIN_FILE=%s", self->file_path);

  cmake_launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE|G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  ide_subprocess_launcher_push_argv (cmake_launcher, "cmake");
  ide_subprocess_launcher_push_argv (cmake_launcher, ".");
  ide_subprocess_launcher_push_argv (cmake_launcher, toolchain_arg);
  ide_subprocess_launcher_set_cwd (cmake_launcher, tmp_dir);
  cmake_subprocess = ide_subprocess_launcher_spawn (cmake_launcher, cancellable, &error);
  if (!ide_subprocess_wait_check (cmake_subprocess, cancellable, &error))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Error Testing CMake Cross-compilation file : %s",
                                 self->file_path);
      return;
    }

  if (!_gbp_cmake_toolchain_parse_keyfile (self, tmp_dir))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Error Testing CMake Cross-compilation file : %s",
                                 self->file_path);
      return;
    }

  ide_task_return_boolean (task, TRUE);
}

/* It is far easier and more reliable to get the variables from cmake itself,
 * Here is a small projects that exports the content of the cross-file */
void
gbp_cmake_toolchain_verify_async (GbpCMakeToolchain    *self,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_TOOLCHAIN (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_run_in_thread (task, gbp_cmake_toolchain_verify_worker);

  IDE_EXIT;
}

gboolean
gbp_cmake_toolchain_verify_finish (GbpCMakeToolchain  *self,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  IdeTask *task = (IdeTask *)result;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (GBP_IS_CMAKE_TOOLCHAIN (self), FALSE);

  ret = ide_task_propagate_boolean (task, error);

  IDE_RETURN (ret);
}

static void
gbp_cmake_toolchain_finalize (GObject *object)
{
  GbpCMakeToolchain *self = (GbpCMakeToolchain *)object;

  g_clear_pointer (&self->file_path, g_free);
  g_clear_pointer (&self->exe_wrapper, g_free);
  g_clear_pointer (&self->archiver, g_free);
  g_clear_pointer (&self->pkg_config, g_free);
  g_clear_pointer (&self->compilers, g_hash_table_unref);

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
gbp_cmake_toolchain_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbpCMakeToolchain *self = GBP_CMAKE_TOOLCHAIN (object);

  switch (prop_id)
    {
    case PROP_FILE_PATH:
      gbp_cmake_toolchain_set_file_path (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_cmake_toolchain_class_init (GbpCMakeToolchainClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeToolchainClass *toolchain_class = IDE_TOOLCHAIN_CLASS (klass);

  object_class->finalize = gbp_cmake_toolchain_finalize;
  object_class->get_property = gbp_cmake_toolchain_get_property;
  object_class->set_property = gbp_cmake_toolchain_set_property;

  toolchain_class->get_tool_for_language = gbp_cmake_toolchain_get_tool_for_language;

  properties [PROP_FILE_PATH] =
    g_param_spec_string ("file-path",
                         "File path",
                         "The path of the cross-file",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_cmake_toolchain_init (GbpCMakeToolchain *self)
{
  self->compilers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}
