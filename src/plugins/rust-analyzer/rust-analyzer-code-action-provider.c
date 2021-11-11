/* rust-analyzer-code-action-provider.c
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#include "rust-analyzer-code-action-provider.h"
#include "rust-analyzer-service.h"

struct _RustAnalyzerCodeActionProvider
{
  IdeLspCodeActionProvider parent_instance;
};

static void provider_iface_init (IdeCodeActionProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (RustAnalyzerCodeActionProvider,
                         rust_analyzer_code_action_provider,
                         IDE_TYPE_LSP_CODE_ACTION_PROVIDER,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_ACTION_PROVIDER, provider_iface_init))

static void
rust_analyzer_code_action_provider_class_init (RustAnalyzerCodeActionProviderClass *klass)
{
}

static void
rust_analyzer_code_action_provider_init (RustAnalyzerCodeActionProvider *self)
{
}

static void
rust_analyzer_code_action_provider_load (IdeCodeActionProvider *self)
{
  RustAnalyzerService *service;
  IdeContext *context;

  g_assert (RUST_IS_ANALYZER_CODE_ACTION_PROVIDER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  service = rust_analyzer_service_from_context (context);
  g_object_bind_property (service, "client", self, "client", G_BINDING_SYNC_CREATE);
  rust_analyzer_service_ensure_started (service);
}

static void
provider_iface_init (IdeCodeActionProviderInterface *iface)
{
  iface->load = rust_analyzer_code_action_provider_load;
}
