/* ide-sysroot-runtime-provider.c
 *
 * Copyright (C) 2018 Corentin Noël <corentin.noel@collabora.com>
 * Copyright (C) 2018 Collabora Ltd.
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

#define G_LOG_DOMAIN "ide-sysroot-runtime-provider"

#include <glib/gi18n.h>

#include "ide-sysroot-runtime.h"
#include "ide-sysroot-runtime-provider.h"
#include "ide-sysroot-manager.h"

struct _IdeSysrootRuntimeProvider
{
  IdeObject  parent_instance;

  GPtrArray *runtimes;
  IdeRuntimeManager *runtime_manager;
};

static void runtime_provider_iface_init (IdeRuntimeProviderInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeSysrootRuntimeProvider,
                        ide_sysroot_runtime_provider,
                        IDE_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER,
                                               runtime_provider_iface_init))

static void
sysroot_runtime_provider_remove_target (IdeSysrootRuntimeProvider *self, gchar *target)
{
  if (self->runtimes != NULL)
    {
      for (guint i= 0; i < self->runtimes->len; i++)
        {
          IdeRuntime *runtime = g_ptr_array_index (self->runtimes, i);
          const gchar *sysroot_id = ide_sysroot_runtime_get_sysroot_id (IDE_SYSROOT_RUNTIME (runtime));
          if (g_strcmp0 (target, sysroot_id) == 0)
            {
              ide_runtime_manager_remove (self->runtime_manager, runtime);
              return;
            }
        }
    }
}

static void
sysroot_runtime_provider_add_target (IdeSysrootRuntimeProvider *self, gchar *target)
{
  GObject *runtime = NULL;
  IdeContext *context = NULL;

  context = ide_object_get_context (IDE_OBJECT (self->runtime_manager));
  runtime = ide_sysroot_runtime_new (context, target);

  ide_runtime_manager_add (self->runtime_manager, IDE_RUNTIME (runtime));
  g_ptr_array_add (self->runtimes, g_steal_pointer (&runtime));
}

static void
sysroot_runtime_provider_target_changed (IdeSysrootRuntimeProvider *self,
                                         gchar *target,
                                         IdeSysrootManagerTargetModificationType mod_type,
                                         gpointer user_data)
{
  if (mod_type == IDE_SYSROOT_MANAGER_TARGET_CREATED)
    {
      sysroot_runtime_provider_add_target (self, target);
    }
  else if (mod_type == IDE_SYSROOT_MANAGER_TARGET_REMOVED)
    {
      sysroot_runtime_provider_remove_target (self, target);
    }
}

static void
ide_sysroot_runtime_provider_class_init (IdeSysrootRuntimeProviderClass *klass)
{
  
}

static void
ide_sysroot_runtime_provider_init (IdeSysrootRuntimeProvider *self)
{
  
}

static void
ide_sysroot_runtime_provider_load (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  IdeSysrootRuntimeProvider *self = IDE_SYSROOT_RUNTIME_PROVIDER (provider);
  IdeSysrootManager *sysroot_manager = NULL;
  GArray *sysroots = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SYSROOT_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  self->runtime_manager = manager;
  self->runtimes = g_ptr_array_new_with_free_func (g_object_unref);

  sysroot_manager = ide_sysroot_manager_get_default ();
  sysroots = ide_sysroot_manager_list (sysroot_manager);
  for (guint i = 0; i < sysroots->len; i++)
    {
      gchar *sysroot_id = g_array_index (sysroots, gchar*, i);
      sysroot_runtime_provider_add_target (self, sysroot_id);
    }

  g_signal_connect_swapped (sysroot_manager, "target-changed", G_CALLBACK (sysroot_runtime_provider_target_changed), self);

  g_array_free (sysroots, TRUE);


  IDE_EXIT;
}

static void
ide_sysroot_runtime_provider_unload (IdeRuntimeProvider *provider,
                                   IdeRuntimeManager  *manager)
{
  IdeSysrootRuntimeProvider *self = IDE_SYSROOT_RUNTIME_PROVIDER (provider);
  IdeSysrootManager *sysroot_manager = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SYSROOT_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (manager));

  sysroot_manager = ide_sysroot_manager_get_default ();
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
  iface->load = ide_sysroot_runtime_provider_load;
  iface->unload = ide_sysroot_runtime_provider_unload;
}
