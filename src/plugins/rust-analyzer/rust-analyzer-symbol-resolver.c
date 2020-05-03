/* rust-analyzer-symbol-resolver.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "rust-analyzer-symbol-resolver.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerSymbolResolver
{
  IdeLspSymbolResolver parent_instance;
};

static void symbol_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RustAnalyzerSymbolResolver,
                         rust_analyzer_symbol_resolver,
                         IDE_TYPE_LSP_SYMBOL_RESOLVER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_iface_init))

static void
rust_analyzer_symbol_resolver_class_init (RustAnalyzerSymbolResolverClass *klass)
{
}

static void
rust_analyzer_symbol_resolver_init (RustAnalyzerSymbolResolver *self)
{
}

static void
rust_analyzer_symbol_resolver_load (IdeSymbolResolver *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  RustAnalyzerService *service = ide_object_ensure_child_typed (IDE_OBJECT (context), RUST_TYPE_ANALYZER_SERVICE);
  rust_analyzer_service_ensure_started (service);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
}

static void
symbol_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->load = rust_analyzer_symbol_resolver_load;
}
