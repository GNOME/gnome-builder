/* rust-analyzer-highlighter.c
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

#define G_LOG_DOMAIN "rust-analyzer-highlighter"

#include "config.h"

#include "rust-analyzer-highlighter.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerHighlighter
{
  IdeLspHighlighter parent_instance;
};

static void provider_iface_init (IdeHighlighterInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (RustAnalyzerHighlighter,
                         rust_analyzer_highlighter,
                         IDE_TYPE_LSP_HIGHLIGHTER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_HIGHLIGHTER, provider_iface_init))

static void
rust_analyzer_highlighter_class_init (RustAnalyzerHighlighterClass *klass)
{
}

static void
rust_analyzer_highlighter_init (RustAnalyzerHighlighter *self)
{
}

static void
rust_analyzer_highlighter_load (IdeHighlighter *self)
{
  IdeContext *context = NULL;
  RustAnalyzerService *service = NULL;

  g_assert (RUST_IS_ANALYZER_HIGHLIGHTER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  service = rust_analyzer_service_from_context (context);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);
}

static void
provider_iface_init (IdeHighlighterInterface *iface)
{
  iface->load = rust_analyzer_highlighter_load;
}
