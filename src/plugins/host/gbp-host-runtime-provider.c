/* gbp-host-runtime-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-host-runtime-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>

#include "gbp-host-runtime.h"
#include "gbp-host-runtime-provider.h"

struct _GbpHostRuntimeProvider
{
  IdeObject       parent_instance;
  GbpHostRuntime *runtime;
};

static void
gbp_host_runtime_provider_load (IdeRuntimeProvider *provider,
                                IdeRuntimeManager  *runtime_manager)
{
  GbpHostRuntimeProvider *self = (GbpHostRuntimeProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_HOST_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  self->runtime = g_object_new (GBP_TYPE_HOST_RUNTIME,
                                "id", "host",
                                "name", _("Host Operating System"),
                                "category", _("Host System"),
                                "parent", self,
                                NULL);
  ide_runtime_manager_add (runtime_manager, IDE_RUNTIME (self->runtime));

  IDE_EXIT;
}

static void
gbp_host_runtime_provider_unload (IdeRuntimeProvider *provider,
                                  IdeRuntimeManager  *runtime_manager)
{
  GbpHostRuntimeProvider *self = (GbpHostRuntimeProvider *)provider;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_HOST_RUNTIME_PROVIDER (self));
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  ide_runtime_manager_remove (runtime_manager, IDE_RUNTIME (self->runtime));
  ide_clear_and_destroy_object (&self->runtime);

  IDE_EXIT;
}

static void
runtime_provider_iface_emit (IdeRuntimeProviderInterface *iface)
{
  iface->load = gbp_host_runtime_provider_load;
  iface->unload = gbp_host_runtime_provider_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpHostRuntimeProvider, gbp_host_runtime_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUNTIME_PROVIDER, runtime_provider_iface_emit))

static void
gbp_host_runtime_provider_class_init (GbpHostRuntimeProviderClass *klass)
{
}

static void
gbp_host_runtime_provider_init (GbpHostRuntimeProvider *self)
{
}
