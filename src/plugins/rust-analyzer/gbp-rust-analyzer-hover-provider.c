/* gbp-rust-analyzer-hover-provider.c
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

#define G_LOG_DOMAIN "gbp-rust-analyzer-hover-provider"

#include "config.h"

#include "gbp-rust-analyzer-hover-provider.h"
#include "gbp-rust-analyzer-service.h"

struct _GbpRustAnalyzerHoverProvider
{
  IdeLspHoverProvider parent_instance;
};

static void
gbp_rust_analyzer_hover_provider_prepare (IdeLspHoverProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_RUST_ANALYZER_HOVER_PROVIDER (provider));

  g_object_set (provider,
                "category", "Typescript",
                "priority", 200,
                NULL);

  klass = g_type_class_ref (GBP_TYPE_RUST_ANALYZER_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

G_DEFINE_FINAL_TYPE (GbpRustAnalyzerHoverProvider, gbp_rust_analyzer_hover_provider, IDE_TYPE_LSP_HOVER_PROVIDER)

static void
gbp_rust_analyzer_hover_provider_class_init (GbpRustAnalyzerHoverProviderClass *klass)
{
  IdeLspHoverProviderClass *lsp_hover_provider_class = IDE_LSP_HOVER_PROVIDER_CLASS (klass);

  lsp_hover_provider_class->prepare = gbp_rust_analyzer_hover_provider_prepare;
}

static void
gbp_rust_analyzer_hover_provider_init (GbpRustAnalyzerHoverProvider *self)
{
}
