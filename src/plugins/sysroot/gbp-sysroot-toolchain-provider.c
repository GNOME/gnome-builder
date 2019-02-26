/* gbp-sysroot-toolchain-provider.c
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

#define G_LOG_DOMAIN "gbp-sysroot-toolchain-provider"

#include <glib/gi18n.h>

#include "gbp-sysroot-toolchain-provider.h"
#include "gbp-sysroot-manager.h"

struct _GbpSysrootToolchainProvider
{
  IdeObject            parent_instance;
  GPtrArray           *toolchains;
};

static gchar *
_create_tool_path (const gchar *base_path,
                   const gchar *original_basename,
                   const gchar *suffix)
{
  g_autofree gchar *tool_name = g_strdup_printf ("%s%s", original_basename, suffix);
  g_autofree gchar *tool_path = g_build_filename (base_path, tool_name, NULL);
  if (!g_file_test (tool_path, G_FILE_TEST_EXISTS))
    return NULL;

  return g_steal_pointer (&tool_path);
}

static gchar *
_test_sdk_dirs (const gchar *basepath,
                const gchar *dir)
{
  g_auto(GStrv) parts = NULL;
  guint parts_length;

  g_return_val_if_fail (basepath != NULL, NULL);
  g_return_val_if_fail (dir != NULL, NULL);

  parts = g_strsplit (dir, "-", -1);
  parts_length = g_strv_length (parts);

  if (parts_length <= 1)
    return NULL;

  for (guint i = 1; i < parts_length - 1; i++)
    {
      g_autofree gchar *head = NULL;
      g_autofree gchar *tail = NULL;
      g_autofree gchar *total = NULL;
      g_autofree gchar *total_path = NULL;
      gchar *part = parts[i];

      parts[i] = NULL;
      head = g_strjoinv ("-", parts);
      tail = g_strjoinv ("-", parts + i + 1);
      total = g_strdup_printf ("%s-%ssdk-%s", head, part, tail);
      total_path = g_build_filename (basepath, "..", total, NULL);
      parts[i] = part;

      if (g_file_test (total_path, G_FILE_TEST_EXISTS))
        return g_steal_pointer (&total_path);
    }

  return NULL;
}

/* Yocto systems are the most used ones, but the native toolchain is in a different folder */
static IdeToolchain *
gbp_sysroot_toolchain_provider_try_poky (GbpSysrootToolchainProvider *self,
                                         const gchar                 *sysroot_id)
{
  g_autoptr(IdeTriplet) system_triplet = NULL;
  g_autoptr(IdeTriplet) sysroot_triplet = NULL;
  g_autoptr(GRegex) arch_regex = NULL;
  g_autoptr(GError) regex_error = NULL;
  g_autofree gchar *sysroot_path = NULL;
  g_autofree gchar *sysroot_basename = NULL;
  g_autofree gchar *arch_escaped = NULL;
  g_autofree gchar *sdk_dir = NULL;
  g_autofree gchar *sysroot_arch = NULL;
  g_autofree gchar *sdk_path = NULL;
  GbpSysrootManager *sysroot_manager;

  g_assert (GBP_IS_SYSROOT_TOOLCHAIN_PROVIDER (self));
  g_assert (sysroot_id != NULL);

  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroot_path = gbp_sysroot_manager_get_target_path (sysroot_manager, sysroot_id);
  sysroot_basename = g_path_get_basename (sysroot_path);

  /* we need to change something like aarch64-poky-linux to x86_64-pokysdk-linux */
  sysroot_arch = gbp_sysroot_manager_get_target_arch (sysroot_manager, sysroot_id);
  system_triplet = ide_triplet_new_from_system ();
  sysroot_triplet = ide_triplet_new (sysroot_arch);

  arch_escaped = g_regex_escape_string (ide_triplet_get_arch (sysroot_triplet), -1);
  arch_regex = g_regex_new (arch_escaped, 0, 0, &regex_error);
  if (regex_error != NULL)
    return NULL;

  sdk_dir = g_regex_replace_literal (arch_regex, sysroot_basename, -1, 0, ide_triplet_get_arch (system_triplet), 0, &regex_error);
  if (regex_error != NULL)
    return NULL;

  sdk_path = _test_sdk_dirs (sysroot_path, sdk_dir);

  if (sdk_path != NULL)
    {
      g_autoptr(IdeSimpleToolchain) toolchain = NULL;
      g_autoptr(GFile) sdk_file = NULL;
      g_autofree gchar *toolchain_id = NULL;
      g_autofree gchar *display_name = NULL;
      g_autofree gchar *sdk_canonical_path = NULL;
      g_autofree gchar *sdk_tools_path = NULL;
      g_autofree gchar *sdk_cc_path = NULL;
      g_autofree gchar *sdk_cpp_path = NULL;
      g_autofree gchar *sdk_cplusplus_path = NULL;
      g_autofree gchar *sdk_ar_path = NULL;
      g_autofree gchar *sdk_ld_path = NULL;
      g_autofree gchar *sdk_strip_path = NULL;
      g_autofree gchar *sdk_pkg_config_path = NULL;
      g_autofree gchar *qemu_static_name = NULL;
      g_autofree gchar *qemu_static_path = NULL;

      sdk_file = g_file_new_for_path (sdk_path);
      sdk_canonical_path = g_file_get_path (sdk_file);
      toolchain_id = g_strdup_printf ("sysroot:%s", sdk_canonical_path);
      display_name = g_strdup_printf (_("%s (Sysroot SDK)"), sdk_canonical_path);
      toolchain = ide_simple_toolchain_new (toolchain_id, display_name);
      ide_toolchain_set_host_triplet (IDE_TOOLCHAIN (toolchain), sysroot_triplet);

      sdk_tools_path = g_build_filename (sdk_canonical_path, "usr", "bin", sysroot_basename, NULL);
      sdk_cc_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-gcc");
      sdk_cplusplus_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-g++");
      sdk_cpp_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-cpp");
      sdk_ar_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-ar");
      sdk_ld_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-ld");
      sdk_strip_path = _create_tool_path (sdk_tools_path, sysroot_basename, "-strip");
      sdk_pkg_config_path = g_build_filename (sdk_canonical_path, "usr", "bin", "pkg-config", NULL);

      if (sdk_cc_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_C, IDE_TOOLCHAIN_TOOL_CC, sdk_cc_path);

      if (sdk_cplusplus_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_CPLUSPLUS, IDE_TOOLCHAIN_TOOL_CC, sdk_cplusplus_path);

      if (sdk_ar_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_AR, sdk_ar_path);

      if (sdk_ld_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_LD, sdk_ld_path);

      if (sdk_strip_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_STRIP, sdk_strip_path);

      if (g_file_test (sdk_pkg_config_path, G_FILE_TEST_EXISTS))
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_PKG_CONFIG, sdk_pkg_config_path);

      qemu_static_name = g_strdup_printf ("qemu-%s-static", ide_triplet_get_arch (sysroot_triplet));
      qemu_static_path = g_find_program_in_path (qemu_static_name);
      if (qemu_static_path != NULL)
        ide_simple_toolchain_set_tool_for_language (toolchain, IDE_TOOLCHAIN_LANGUAGE_ANY, IDE_TOOLCHAIN_TOOL_EXEC, qemu_static_path);

      return IDE_TOOLCHAIN (g_steal_pointer (&toolchain));
    }

  return NULL;
}

static void
gbp_sysroot_toolchain_provider_load_worker (IdeTask      *task,
                                            gpointer      source_object,
                                            gpointer      task_data,
                                            GCancellable *cancellable)
{
  GbpSysrootToolchainProvider *self = source_object;
  g_autoptr(GPtrArray) toolchains = NULL;
  g_auto(GStrv) sysroot_list = NULL;
  GbpSysrootManager *sysroot_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_SYSROOT_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  toolchains = g_ptr_array_new_with_free_func (g_object_unref);
  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroot_list = gbp_sysroot_manager_list (sysroot_manager);

  for (guint i = 0; sysroot_list[i] != NULL; i++)
    {
      g_autoptr(IdeToolchain) toolchain = NULL;

      toolchain = gbp_sysroot_toolchain_provider_try_poky (self, sysroot_list[i]);
      if (toolchain != NULL)
        g_ptr_array_add (toolchains, g_steal_pointer (&toolchain));
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&toolchains),
                           g_ptr_array_unref);

  IDE_EXIT;
}

static void
gbp_sysroot_toolchain_provider_load_async (IdeToolchainProvider     *provider,
                                           GCancellable             *cancellable,
                                           GAsyncReadyCallback       callback,
                                           gpointer                  user_data)
{
  GbpSysrootToolchainProvider *self = (GbpSysrootToolchainProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSROOT_TOOLCHAIN_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_sysroot_toolchain_provider_load_async);
  ide_task_run_in_thread (task, gbp_sysroot_toolchain_provider_load_worker);

  IDE_EXIT;
}

static gboolean
gbp_sysroot_toolchain_provider_load_finish (IdeToolchainProvider  *provider,
                                            GAsyncResult          *result,
                                            GError               **error)
{
  GbpSysrootToolchainProvider *self = (GbpSysrootToolchainProvider *)provider;
  g_autoptr(GPtrArray) toolchains = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSROOT_TOOLCHAIN_PROVIDER (self));
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

static void
gbp_sysroot_toolchain_provider_unload (IdeToolchainProvider  *provider,
                                       IdeToolchainManager   *manager)
{
  GbpSysrootToolchainProvider *self = (GbpSysrootToolchainProvider *) provider;

  g_assert (GBP_IS_SYSROOT_TOOLCHAIN_PROVIDER (self));
  g_assert (IDE_IS_TOOLCHAIN_MANAGER (manager));

  g_clear_pointer (&self->toolchains, g_ptr_array_unref);
}

static void
toolchain_provider_iface_init (IdeToolchainProviderInterface *iface)
{
  iface->load_async = gbp_sysroot_toolchain_provider_load_async;
  iface->load_finish = gbp_sysroot_toolchain_provider_load_finish;
  iface->unload = gbp_sysroot_toolchain_provider_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpSysrootToolchainProvider,
                         gbp_sysroot_toolchain_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TOOLCHAIN_PROVIDER,
                                                toolchain_provider_iface_init))

static void
gbp_sysroot_toolchain_provider_class_init (GbpSysrootToolchainProviderClass *klass)
{
}

static void
gbp_sysroot_toolchain_provider_init (GbpSysrootToolchainProvider *self)
{

}
