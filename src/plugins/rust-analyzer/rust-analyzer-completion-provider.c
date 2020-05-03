/* rust-analyzer-completion-provider.c
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

#include "rust-analyzer-completion-provider.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerCompletionProvider
{
  IdeLspCompletionProvider parent_instance;
};

static void provider_iface_init (IdeCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RustAnalyzerCompletionProvider,
                         rust_analyzer_completion_provider,
                         IDE_TYPE_LSP_COMPLETION_PROVIDER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_PROVIDER, provider_iface_init))

static void
rust_analyzer_completion_provider_class_init (RustAnalyzerCompletionProviderClass *klass)
{
}

static void
rust_analyzer_completion_provider_init (RustAnalyzerCompletionProvider *self)
{
}

static void
rust_analyzer_completion_provider_load (IdeCompletionProvider *self,
                                        IdeContext            *context)
{
  RustAnalyzerService *service = ide_object_ensure_child_typed (IDE_OBJECT (context), RUST_TYPE_ANALYZER_SERVICE);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);
}

static gint
rust_analyzer_completion_provider_get_priority (IdeCompletionProvider *provider,
                                                IdeCompletionContext  *context)
{
  return -1000;
}

static void
provider_iface_init (IdeCompletionProviderInterface *iface)
{
  iface->load = rust_analyzer_completion_provider_load;
  iface->get_priority = rust_analyzer_completion_provider_get_priority;
}
