/* gbp-sysroot-runtime-provider.c
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-sysroot-runtime-provider"

#include <glib/gi18n.h>

#include "gbp-sysroot-runtime.h"
#include "gbp-sysroot-runtime-provider.h"
#include "gbp-sysroot-manager.h"

struct _GbpSysrootRuntimeProvider
{
  IdeObject  parent_instance;

  GPtrArray *runtimes;
  IdeRuntimeManager *runtime_manager;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpSysrootRuntimeProvider,
                        gbp_sysroot_runtime_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER,
                                               runtime_provider_iface_init))

static void
sysroot_runtime_provider_remove_target (GbpSysrootRuntimeProvider *self,
                                        const gchar               *target)
{
  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          GbpSysrootRuntime *runtime = g_ptr_array_index (self->runtimes, i);
          const gchar *sysroot_id = gbp_sysroot_runtime_get_sysroot_id (runtime);

          if (g_strcmp0 (target, sysroot_id) == 0)
            {
              ide_runtime_manager_remove (self->runtime_manager, IDE_RUNTIME (runtime));
              return;
            }
        }
    }
}

static void
sysroot_runtime_provider_add_target (GbpSysrootRuntimeProvider *self,
                                     const gchar               *target)
{
  g_autoptr(GbpSysrootRuntime) runtime = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSROOT_RUNTIME_PROVIDER (self));
  g_assert (target != NULL);

  runtime = gbp_sysroot_runtime_new (target);
  ide_object_append (IDE_OBJECT (self), IDE_OBJECT (runtime));

  ide_runtime_manager_add (self->runtime_manager, IDE_RUNTIME (runtime));
  g_ptr_array_add (self->runtimes, g_steal_pointer (&runtime));
}

static void
sysroot_runtime_provider_target_changed (GbpSysrootRuntimeProvider               *self,
                                         const gchar                             *target,
                                         GbpSysrootManagerTargetModificationType  mod_type,
                                         gpointer                                 user_data)
{
  g_assert (GBP_IS_SYSROOT_RUNTIME_PROVIDER (self));

  if (mod_type == GBP_SYSROOT_MANAGER_TARGET_CREATED)
    sysroot_runtime_provider_add_target (self, target);
  else if (mod_type == GBP_SYSROOT_MANAGER_TARGET_REMOVED)
    sysroot_runtime_provider_remove_target (self, target);
}

static void
gbp_sysroot_runtime_provider_class_init (GbpSysrootRuntimeProviderClass *klass)
{
}

static void
gbp_sysroot_runtime_provider_init (GbpSysrootRuntimeProvider *self)
{
}

static void
gbp_sysroot_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  GbpSysrootRuntimeProvider *self = (GbpSysrootRuntimeProvider *)provider;
  GbpSysrootManager *sysroot_manager = NULL;
  g_auto(GStrv) sysroots = NULL;
  guint sysroots_length = 0;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSROOT_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->runtime_manager = manager;
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);

  sysroot_manager = gbp_sysroot_manager_get_default ();
  sysroots = gbp_sysroot_manager_list (sysroot_manager);
  sysroots_length = g_strv_length (sysroots);
  for (guint i = 0; i < sysroots_length; i++)
    {
      sysroot_runtime_provider_add_target (self, sysroots[i]);
    }

  /* Hold extra ref during plugin load */
  g_object_ref (sysroot_manager);

  g_signal_connect_object (sysroot_manager,
                           "target-changed",
                           G_CALLBACK (sysroot_runtime_provider_target_changed),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_sysroot_runtime_provider_unload (IdeRuntimeProvider *provider,
                                     IdeRuntimeManager  *manager)
{
  GbpSysrootRuntimeProvider *self = (GbpSysrootRuntimeProvider *)provider;
  GbpSysrootManager *sysroot_manager = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SYSROOT_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  sysroot_manager = gbp_sysroot_manager_get_default ();

  /* Drop shared instance if last instance usage */
  if (G_OBJECT (sysroot_manager)->ref_count == 2)
    g_object_unref (sysroot_manager);

  /* Drop our ref from project load */
  g_object_unref (sysroot_manager);

  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);

          ide_runtime_manager_remove (manager, runtime);
        }
    }

  g_clear_pointer (&self->runtimes, g_ptr_array_unref);

  IDE_EXIT;
}

static void
runtime_provider_iface_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_sysroot_runtime_provider_load;
  iface->unload = gbp_sysroot_runtime_provider_unload;
}
