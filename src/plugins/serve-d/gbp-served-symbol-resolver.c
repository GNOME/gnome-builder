/* gbp-served-symbol-resolver.c
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

#define G_LOG_DOMAIN "gbp-served-symbol-resolver"

#include "config.h"

#include "gbp-served-symbol-resolver.h"
#include "gbp-served-service.h"

struct _GbpServedSymbolResolver
{
  IdeLspSymbolResolver parent_instance;
};

static void
gbp_served_symbol_provider_load (IdeSymbolResolver *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_SERVED_SYMBOL_RESOLVER (provider));

  klass = g_type_class_ref (GBP_TYPE_SERVED_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
symbol_provider_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->load = gbp_served_symbol_provider_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpServedSymbolResolver, gbp_served_symbol_provider, IDE_TYPE_LSP_SYMBOL_RESOLVER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_provider_iface_init))

static void
gbp_served_symbol_provider_class_init (GbpServedSymbolResolverClass *klass)
{
}

static void
gbp_served_symbol_provider_init (GbpServedSymbolResolver *self)
{
}
