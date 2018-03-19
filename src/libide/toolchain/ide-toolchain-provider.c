/* ide-toolchain-provider.c
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

#define G_LOG_DOMAIN "ide-toolchain-provider"

#include "ide-debug.h"

#include "toolchain/ide-toolchain-provider.h"

G_DEFINE_INTERFACE (IdeToolchainProvider, ide_toolchain_provider, G_TYPE_OBJECT)

static void
ide_toolchain_provider_default_init (IdeToolchainProviderInterface *iface)
{
  
}

void
ide_toolchain_provider_load (IdeToolchainProvider *self,
                             IdeToolchainManager  *manager)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (manager));

  IDE_TOOLCHAIN_PROVIDER_GET_IFACE (self)->load (self, manager);
}

void
ide_toolchain_provider_unload (IdeToolchainProvider *self,
                               IdeToolchainManager  *manager)
{
  g_return_if_fail (IDE_IS_TOOLCHAIN_PROVIDER (self));
  g_return_if_fail (IDE_IS_TOOLCHAIN_MANAGER (manager));

  IDE_TOOLCHAIN_PROVIDER_GET_IFACE (self)->unload (self, manager);
}
