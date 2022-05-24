/* gbp-gopls-code-action-provider.c
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

#define G_LOG_DOMAIN "gbp-gopls-code-action-provider"

#include "config.h"

#include "gbp-gopls-code-action-provider.h"
#include "gbp-gopls-service.h"

struct _GbpGoplsCodeActionProvider
{
  IdeLspCodeActionProvider parent_instance;
};

static void
gbp_gopls_code_action_provider_load (IdeCodeActionProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GOPLS_CODE_ACTION_PROVIDER (provider));

  klass = g_type_class_ref (GBP_TYPE_GOPLS_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
code_action_provider_iface_init (IdeCodeActionProviderInterface *iface)
{
  iface->load = gbp_gopls_code_action_provider_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGoplsCodeActionProvider, gbp_gopls_code_action_provider, IDE_TYPE_LSP_CODE_ACTION_PROVIDER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_ACTION_PROVIDER, code_action_provider_iface_init))

static void
gbp_gopls_code_action_provider_class_init (GbpGoplsCodeActionProviderClass *klass)
{
}

static void
gbp_gopls_code_action_provider_init (GbpGoplsCodeActionProvider *self)
{
}
