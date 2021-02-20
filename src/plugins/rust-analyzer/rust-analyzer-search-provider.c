/* rust-analyzer-search-provider.c
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

#include <libide-search.h>

#include "rust-analyzer-search-provider.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerSearchProvider
{
  IdeLspSearchProvider parent_instance;
};

static void provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RustAnalyzerSearchProvider,
                         rust_analyzer_search_provider,
                         IDE_TYPE_LSP_SEARCH_PROVIDER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, provider_iface_init))

static void
rust_analyzer_search_provider_class_init (RustAnalyzerSearchProviderClass *klass)
{
}

static void
rust_analyzer_search_provider_init (RustAnalyzerSearchProvider *self)
{
}

static void
rust_analyzer_search_provider_load (IdeSearchProvider *self,
                                    IdeContext        *context)
{
  RustAnalyzerService *service;

  IDE_ENTRY;

  g_assert (RUST_IS_ANALYZER_SEARCH_PROVIDER (self));
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  service = ide_object_ensure_child_typed (IDE_OBJECT (context), RUST_TYPE_ANALYZER_SERVICE);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);

  IDE_EXIT;
}

static void
provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = rust_analyzer_search_provider_load;
}
