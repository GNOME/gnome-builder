/* ide-runtime-provider.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-runtime-manager.h"
#include "ide-runtime-provider.h"

G_DEFINE_INTERFACE (IdeRuntimeProvider, ide_runtime_provider, G_TYPE_OBJECT)

static void
ide_runtime_provider_real_load (IdeRuntimeProvider *self,
                                IdeRuntimeManager  *manager)
{
}

static void
ide_runtime_provider_real_unload (IdeRuntimeProvider *self,
                                  IdeRuntimeManager  *manager)
{
}

static void
ide_runtime_provider_default_init (IdeRuntimeProviderInterface *iface)
{
  iface->load = ide_runtime_provider_real_load;
  iface->unload = ide_runtime_provider_real_unload;
}

void
ide_runtime_provider_load (IdeRuntimeProvider *self,
                           IdeRuntimeManager  *manager)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (manager));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->load (self, manager);
}

void
ide_runtime_provider_unload (IdeRuntimeProvider *self,
                             IdeRuntimeManager  *manager)
{
  g_return_if_fail (IDE_IS_RUNTIME_PROVIDER (self));
  g_return_if_fail (IDE_IS_RUNTIME_MANAGER (manager));

  IDE_RUNTIME_PROVIDER_GET_IFACE (self)->unload (self, manager);
}
