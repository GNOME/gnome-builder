/* gbp-gcc-toolchain-provider.c
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
 */

#define G_LOG_DOMAIN "gbp-gcc-toolchain-provider"

#include <glib/gi18n.h>

#include "gbp-gcc-toolchain-provider.h"

struct _GbpGccToolchainProvider
{
  IdeObject            parent_instance;
  GPtrArray           *toolchains;
};

typedef struct
{
  GList     *folders;
  GPtrArray *found_files;
  IdeTask   *task;
} FileSearching;

static FileSearching *
gbp_gcc_toolchain_provider_file_searching_new (void)
{
  FileSearching *file_searching;

  file_searching = g_slice_new0 (FileSearching);
  file_searching->task = NULL;
  file_searching->folders = NULL;
  file_searching->found_files = g_ptr_array_new ();
  IDE_PTR_ARRAY_SET_FREE_FUNC (file_searching->found_files, g_object_unref);

  return file_searching;
}

static void
gbp_gcc_toolchain_provider_file_searching_free (FileSearching *file_searching)
{
  if (file_searching->task)
    g_object_unref (file_searching->task);

  if (file_searching->found_files)
    g_ptr_array_unref (file_searching->found_files);

  if (file_searching->folders)
    g_list_free_full (file_searching->folders, g_object_unref);

  g_slice_free (FileSearching, file_searching);
}

static gchar *
_create_tool_path (const gchar *parent_name,
                   const gchar *arch,
                   const gchar *toolname)
{
  g_autofree gchar *tool_name = g_strdup_printf ("%s%s", arch, toolname);
  g_autofree gchar *tool_path = g_build_filename (parent_name, tool_name, NULL);
  if (!g_file_test (tool_path, G_FILE_TEST_EXISTS))
    return NULL;

  return g_steal_pointer (&tool_path);
}

static IdeToolchain *
gbp_gcc_toolchain_provider_get_toolchain_from_file (GbpGccToolchainProvider *self,
                                                    GFile                   *file,
                                                    const gchar             *arch)
{
  g_autoptr(IdeTriplet) triplet = ide_triplet_new (arch);
  g_autoptr(IdeSimpleToolchain) toolchain = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *parent_path = NULL;
  g_autofree gchar *gcc_path = NULL;
  g_autofree gchar *toolchain_id = NULL;
  g_autofree gchar *display_name = NULL;
  g_autofree gchar *sdk_cplusplus_path = NULL;
  g_autofree gchar *sdk_ar_path = NULL;
  g_autofree gchar *sdk_ld_path = NULL;
  g_autofree gchar *sdk_strip_path = NULL;
  g_autofree gchar *sdk_pkg_config_path = NULL;
  IdeContext *context;

  gcc_path = g_file_get_path (file);
  toolchain_id = g_strdup_printf ("gcc:%s", gcc_path);
  display_name = g_strdup_printf (_("GCC %s Cross-Compiler (System)"), arch);
  context = ide_object_get_context (IDE_OBJECT (self));
  toolchain = ide_simple_toolchain_new (context, toolchain_id, display_name);
  ide_toolchain_set_host_triplet (IDE_TOOLCHAIN (toolchain), triplet);
  ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_C, IDE_TOOLCHAIN_TOOL_CC, gcc_path);

  parent = g_file_get_parent (file);
  parent_path = g_file_get_path (parent);

  sdk_cplusplus_path = _create_tool_path (parent_path, arch, "-g++");
  sdk_ar_path = _create_tool_path (parent_path, arch, "-ar");
  sdk_ld_path = _create_tool_path (parent_path, arch, "-ld");
  sdk_strip_path = _create_tool_path (parent_path, arch, "-strip");
  sdk_pkg_config_path = _create_tool_path (parent_path, arch, "-pkg-config");

  if (sdk_cplusplus_path != NULL)
    ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS, IDE_TOOLCHAIN_TOOL_CC, sdk_cplusplus_path);

  if (sdk_ar_path != NULL)
    ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_AR, sdk_ar_path);

  if (sdk_ld_path != NULL)
    ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_LD, sdk_ld_path);

  if (sdk_strip_path != NULL)
    ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_STRIP, sdk_strip_path);

  if (sdk_pkg_config_path != NULL)
    ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_PKG_CONFIG, sdk_pkg_config_path);

  return IDE_TOOLCHAIN (g_steal_pointer (&toolchain));
}

static void
gbp_gcc_toolchain_provider_load_worker (IdeTask      *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  GbpGccToolchainProvider *self = source_object;
  g_autoptr(GPtrArray) toolchains = NULL;
  GPtrArray *files = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GCC_TOOLCHAIN_PROVIDER (self));
  g_assert (files != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  toolchains = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autofree gchar *basename = NULL;
      glong basename_length = 0;

      basename = g_file_get_basename (file);
      basename_length = g_utf8_strlen (basename, -1);
      if (basename_length > strlen ("-gcc"))
        {
          g_autofree gchar *arch = NULL;
          arch = g_utf8_substring (basename, 0, g_utf8_strlen (basename, -1) - strlen ("-gcc"));
          /* MinGW is out of the scope of this provider */
          if (g_strrstr (arch, "-") != NULL && g_strrstr (arch, "mingw32") == NULL)
            {
              g_autoptr(IdeTriplet) system_triplet = ide_triplet_new_from_system ();
              /* The default toolchain already covers the system triplet */
              if (g_strcmp0 (ide_triplet_get_full_name (system_triplet), arch) != 0)
                {
                  IdeToolchain *toolchain = NULL;

                  toolchain = gbp_gcc_toolchain_provider_get_toolchain_from_file (self, file, arch);
                  g_ptr_array_add (toolchains, toolchain);
                }
            }
        }
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&toolchains),
                           (GDestroyNotify)g_ptr_array_unref);
}

void
gbp_gcc_toolchain_provider_search_finish (FileSearching *file_searching,
                                          GError        *error)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ret = NULL;

  g_assert (file_searching != NULL);

  task = g_steal_pointer (&file_searching->task);
  ret = g_steal_pointer (&file_searching->found_files);
  gbp_gcc_toolchain_provider_file_searching_free (file_searching);

  if (error != NULL)
    {
      ide_task_return_error (task, error);
      return;
    }

  ide_task_set_task_data (task, g_steal_pointer (&ret), (GDestroyNotify)g_ptr_array_unref);
  ide_task_run_in_thread (task, gbp_gcc_toolchain_provider_load_worker);
}

static void
add_all_files (GFile     *file,
               GPtrArray *dest_array)
{
  g_assert (G_IS_FILE (file));
  g_assert (dest_array != NULL);

  g_ptr_array_add (dest_array, g_object_ref (file));
}

void
gbp_gcc_toolchain_provider_search_iterate (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  FileSearching *file_searching = user_data;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (file_searching != NULL);
  g_assert (IDE_IS_TASK (file_searching->task));

  ret = ide_g_file_find_finish (file, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, g_object_unref);

  if (ret == NULL)
    {
      gbp_gcc_toolchain_provider_search_finish (file_searching, g_steal_pointer (&error));
      return;
    }

  g_ptr_array_foreach (ret, (GFunc)add_all_files, file_searching->found_files);
  file_searching->folders = g_list_remove (file_searching->folders, file);
  if (file_searching->folders != NULL)
    ide_g_file_find_async (file_searching->folders->data,
                           "*-gcc",
                           ide_task_get_cancellable (file_searching->task),
                           gbp_gcc_toolchain_provider_search_iterate,
                           file_searching);
  else
    gbp_gcc_toolchain_provider_search_finish (file_searching, NULL);
}

void
gbp_gcc_toolchain_provider_search_init (GbpGccToolchainProvider *self,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
  GList *folders = NULL;
  g_autoptr (IdeTask) task = NULL;
  g_auto(GStrv) environ = NULL;
  g_auto(GStrv) paths = NULL;
  const gchar *path_env;
  FileSearching *file_searching;

  g_assert (GBP_IS_GCC_TOOLCHAIN_PROVIDER (self));

  environ = g_get_environ ();
  path_env = g_environ_getenv (environ, "PATH");
  paths = g_strsplit (path_env, ":", -1);
  for (int i = 0; paths[i] != NULL; i++)
    folders = g_list_append (folders, g_file_new_for_path (paths[i]));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_gcc_toolchain_provider_search_init);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  file_searching = gbp_gcc_toolchain_provider_file_searching_new ();
  file_searching->task = g_steal_pointer (&task);
  file_searching->folders = folders;

  /* GCC */
  ide_g_file_find_async (folders->data,
                         "*-gcc",
                         cancellable,
                         gbp_gcc_toolchain_provider_search_iterate,
                         file_searching);
}

static void
gbp_gcc_toolchain_provider_load_async (IdeToolchainProvider     *provider,
                                       GCancellable             *cancellable,
                                       GAsyncReadyCallback       callback,
                                       gpointer                  user_data)
{
  GbpGccToolchainProvider *self = (GbpGccToolchainProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GCC_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  gbp_gcc_toolchain_provider_search_init (self, cancellable, callback, user_data);

  IDE_EXIT;
}

static gboolean
gbp_gcc_toolchain_provider_load_finish (IdeToolchainProvider  *provider,
                                        GAsyncResult          *result,
                                        GError               **error)
{
  GbpGccToolchainProvider *self = (GbpGccToolchainProvider *)provider;
  g_autoptr(GPtrArray) toolchains = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GCC_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  toolchains = ide_task_propagate_pointer (IDE_TASK (result), error);

  if (toolchains == NULL)
    return FALSE;

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
  self->toolchains = g_ptr_array_ref (toolchains);

  for (guint i = 0; i < toolchains->len; i++)
    {
      IdeToolchain *toolchain = g_ptr_array_index (toolchains, i);

      g_assert (IDE_IS_TOOLCHAIN (toolchain));

      ide_toolchain_provider_emit_added (provider, toolchain);
    }

  return TRUE;
}

void
gbp_gcc_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                   IdeToolchainManager   *manager)
{
  GbpGccToolchainProvider *self = (GbpGccToolchainProvider *)provider;

  g_assert (GBP_IS_GCC_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load_async = gbp_gcc_toolchain_provider_load_async;
  iface->load_finish = gbp_gcc_toolchain_provider_load_finish;
  iface->unload = gbp_gcc_toolchain_provider_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGccToolchainProvider,
                         gbp_gcc_toolchain_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TOOLCHAIN_PROVIDER,
                                                toolchain_provider_iface_init))

static void
gbp_gcc_toolchain_provider_class_init (GbpGccToolchainProviderClass *klass)
{
}

static void
gbp_gcc_toolchain_provider_init (GbpGccToolchainProvider *self)
{
}
