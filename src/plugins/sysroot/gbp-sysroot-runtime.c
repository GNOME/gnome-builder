/* gbp-sysroot-runtime.c
 *
 * Copyright 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright 2018 Collabora Ltd.
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-sysroot-runtime"

#include "config.h"

#include "gbp-sysroot-runtime.h"
#include "gbp-sysroot-manager.h"
#include "gbp-sysroot-subprocess-launcher.h"

// This is a list of common libdirs to use
#define RUNTIME_PREFIX "sysroot:"

struct _GbpSysrootRuntime
{
  IdeRuntime  parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpSysrootRuntime, gbp_sysroot_runtime, IDE_TYPE_RUNTIME)

GbpSysrootRuntime *
gbp_sysroot_runtime_new (const gchar *sysroot_id)
{
  g_autoptr(GbpSysrootRuntime) runtime = NULL;
  g_autofree gchar *built_id = NULL;

  g_return_val_if_fail (sysroot_id != NULL, NULL);

  built_id = g_strconcat (RUNTIME_PREFIX, sysroot_id, NULL);
  runtime = g_object_new (GBP_TYPE_SYSROOT_RUNTIME,
                          "id", built_id,
                          "display-name", "",
                          NULL);

  return g_steal_pointer (&runtime);
}

/**
 * gbp_sysroot_runtime_get_sysroot_id:
 * @self: a #GbpSysrootRuntime
 *
 * Gets the associated unique identifier of the sysroot target.
 *
 * Returns: (transfer none): the unique identifier of the sysroot target.
 */
const gchar *
gbp_sysroot_runtime_get_sysroot_id (GbpSysrootRuntime *self)
{
  const gchar *runtime_id = ide_runtime_get_id (IDE_RUNTIME (self));

  if (!g_str_has_prefix (runtime_id, RUNTIME_PREFIX))
    return runtime_id;

  return runtime_id + strlen (RUNTIME_PREFIX);
}

static void
gbp_sysroot_runtime_prepare_run_context (IdeRuntime    *runtime,
                                         IdePipeline   *pipeline,
                                         IdeRunContext *run_context)
{
  GbpSysrootRuntime *self = GBP_SYSROOT_RUNTIME(runtime);
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree char *sysroot_flag = NULL;
  g_autofree char *sysroot_path = NULL;
  g_autofree char *pkgconfig_dirs = NULL;
  const char *sysroot_id = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSROOT_RUNTIME (self));
  g_assert (!pipeline || IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  ide_run_context_push_host (run_context);

  sysroot_id = gbp_sysroot_runtime_get_sysroot_id (self);
  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroot_path = gbp_sysroot_manager_get_target_path (sysroot_manager, sysroot_id);
  sysroot_flag = g_strconcat ("--sysroot=", sysroot_path, NULL);
  pkgconfig_dirs = gbp_sysroot_manager_get_target_pkg_config_path (sysroot_manager, sysroot_id);

  ide_run_context_setenv (run_context, "CFLAGS", sysroot_flag);
  ide_run_context_setenv (run_context, "LDFLAGS", sysroot_flag);
  ide_run_context_setenv (run_context, "PKG_CONFIG_DIR", "");
  ide_run_context_setenv (run_context, "PKG_CONFIG_LIBDIR", pkgconfig_dirs);
  ide_run_context_setenv (run_context, "PKG_CONFIG_SYSROOT_DIR", sysroot_path);
  ide_run_context_setenv (run_context, "QEMU_LD_PREFIX", sysroot_path);

  IDE_EXIT;
}

static gchar **
gbp_sysroot_runtime_get_system_include_dirs (IdeRuntime *runtime)
{
  GbpSysrootRuntime *self = (GbpSysrootRuntime *)runtime;
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *sysroot_path = NULL;
  g_autofree gchar *full_path = NULL;
  const gchar *sysroot_id = NULL;
  const gchar *result_paths[2] = { NULL, NULL };

  g_assert (GBP_IS_SYSROOT_RUNTIME (self));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroot_id = gbp_sysroot_runtime_get_sysroot_id (self);
  sysroot_path = gbp_sysroot_manager_get_target_path (sysroot_manager, sysroot_id);
  full_path = g_build_filename (G_DIR_SEPARATOR_S, sysroot_path, "/usr/include", NULL);
  result_paths[0] = full_path;

  return g_strdupv ((char**) result_paths);
}

static IdeTriplet *
gbp_sysroot_runtime_get_triplet (IdeRuntime *runtime)
{
  GbpSysrootRuntime *self = GBP_SYSROOT_RUNTIME(runtime);
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *target_arch = NULL;
  const gchar *sysroot_id = NULL;

  g_assert (GBP_IS_SYSROOT_RUNTIME (self));

  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroot_id = gbp_sysroot_runtime_get_sysroot_id (self);
  target_arch = gbp_sysroot_manager_get_target_arch (sysroot_manager, sysroot_id);

  return ide_triplet_new (target_arch);
}

static gboolean
gbp_sysroot_runtime_supports_toolchain (IdeRuntime   *runtime,
                                        IdeToolchain *toolchain)
{
  g_autoptr(IdeTriplet) host_triplet = NULL;
  g_autofree gchar *runtime_arch = NULL;

  g_assert (GBP_IS_SYSROOT_RUNTIME (runtime));
  g_assert (IDE_IS_TOOLCHAIN (toolchain));

  runtime_arch = ide_runtime_get_arch (runtime);
  host_triplet = ide_toolchain_get_host_triplet (toolchain);
  return g_strcmp0 (runtime_arch, ide_triplet_get_arch (host_triplet)) == 0;
}

static void
sysroot_runtime_target_name_changed (GbpSysrootRuntime *self,
                                     const gchar       *target,
                                     const gchar       *new_name,
                                     GbpSysrootManager *manager)
{
  const gchar* sysroot_id;

  g_assert (GBP_IS_SYSROOT_RUNTIME (self));
  g_assert (GBP_IS_SYSROOT_MANAGER (manager));

  sysroot_id = gbp_sysroot_runtime_get_sysroot_id (self);

  if (g_strcmp0 (target, sysroot_id) == 0)
    ide_runtime_set_display_name (IDE_RUNTIME (self), new_name);
}

static void
gbp_sysroot_runtime_constructed (GObject *object)
{
  GbpSysrootManager *sysroot_manager = NULL;
  g_autofree gchar *display_name = NULL;
  const gchar* sysroot_id = NULL;

  g_assert (GBP_IS_SYSROOT_RUNTIME (object));

  sysroot_id = gbp_sysroot_runtime_get_sysroot_id (GBP_SYSROOT_RUNTIME (object));
  sysroot_manager = gbp_sysroot_manager_get_default ();
  display_name = gbp_sysroot_manager_get_target_name (sysroot_manager, sysroot_id);
  ide_runtime_set_display_name (IDE_RUNTIME (object), display_name);

  g_signal_connect_object (sysroot_manager,
                           "target-name-changed",
                           G_CALLBACK (sysroot_runtime_target_name_changed),
                           object,
                           G_CONNECT_SWAPPED);

  G_OBJECT_CLASS (gbp_sysroot_runtime_parent_class)->constructed (object);
}

static void
gbp_sysroot_runtime_class_init (GbpSysrootRuntimeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeRuntimeClass *runtime_class = IDE_RUNTIME_CLASS (klass);

  object_class->constructed = gbp_sysroot_runtime_constructed;

  runtime_class->prepare_to_build = gbp_sysroot_runtime_prepare_run_context;
  runtime_class->prepare_to_run = gbp_sysroot_runtime_prepare_run_context;
  runtime_class->get_system_include_dirs = gbp_sysroot_runtime_get_system_include_dirs;
  runtime_class->get_triplet = gbp_sysroot_runtime_get_triplet;
  runtime_class->supports_toolchain = gbp_sysroot_runtime_supports_toolchain;
}

static void
gbp_sysroot_runtime_init (GbpSysrootRuntime *self)
{
}
