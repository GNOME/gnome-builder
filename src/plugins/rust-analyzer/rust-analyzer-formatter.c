/* rust-analyzer-formatter.c
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

#include "rust-analyzer-formatter.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerFormatter
{
  IdeLspFormatter parent_instance;
};

static void provider_iface_init (IdeFormatterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (RustAnalyzerFormatter,
                         rust_analyzer_formatter,
                         IDE_TYPE_LSP_FORMATTER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, provider_iface_init))

static void
rust_analyzer_formatter_class_init (RustAnalyzerFormatterClass *klass)
{
}

static void
rust_analyzer_formatter_init (RustAnalyzerFormatter *self)
{
}

static void
rust_analyzer_formatter_load (IdeFormatter *self)
{
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  RustAnalyzerService *service = ide_object_ensure_child_typed (IDE_OBJECT (context), RUST_TYPE_ANALYZER_SERVICE);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);
}

static void
provider_iface_init (IdeFormatterInterface *iface)
{
  iface->load = rust_analyzer_formatter_load;
}
