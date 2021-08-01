/* rust-analyzer-hover-provider.c
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

#include "rust-analyzer-hover-provider.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerHoverProvider
{
  IdeLspHoverProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (RustAnalyzerHoverProvider,
               rust_analyzer_hover_provider,
               IDE_TYPE_LSP_HOVER_PROVIDER)

static void
rust_analyzer_hover_provider_prepare (IdeLspHoverProvider *self)
{
  IdeContext *context = NULL;
  RustAnalyzerService *service = NULL;

  g_assert (RUST_IS_ANALYZER_HOVER_PROVIDER (self));

  g_object_set (self,
                "category", "Rust",
                "priority", 200,
                NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = rust_analyzer_service_from_context (context);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);
}


static void
rust_analyzer_hover_provider_class_init (RustAnalyzerHoverProviderClass *klass)
{
  IdeLspHoverProviderClass *lsp_class = IDE_LSP_HOVER_PROVIDER_CLASS (klass);

  lsp_class->prepare = rust_analyzer_hover_provider_prepare;
}

static void
rust_analyzer_hover_provider_init (RustAnalyzerHoverProvider *self)
{
}
