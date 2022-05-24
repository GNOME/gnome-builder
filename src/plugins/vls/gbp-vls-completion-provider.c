/* gbp-vls-completion-provider.c
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

#define G_LOG_DOMAIN "gbp-vls-completion-provider"

#include "config.h"

#include "gbp-vls-completion-provider.h"
#include "gbp-vls-service.h"

struct _GbpVlsCompletionProvider
{
  IdeLspCompletionProvider parent_instance;
};

static void
gbp_vls_completion_provider_load (IdeLspCompletionProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_VLS_COMPLETION_PROVIDER (provider));

  klass = g_type_class_ref (GBP_TYPE_VLS_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static int
gbp_vls_completion_provider_get_priority (GtkSourceCompletionProvider *provider,
                                          GtkSourceCompletionContext  *context)
{
  return -1000;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
  iface->get_priority = gbp_vls_completion_provider_get_priority;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVlsCompletionProvider, gbp_vls_completion_provider, IDE_TYPE_LSP_COMPLETION_PROVIDER,
                               G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

static void
gbp_vls_completion_provider_class_init (GbpVlsCompletionProviderClass *klass)
{
  IdeLspCompletionProviderClass *lsp_completion_provider_class = IDE_LSP_COMPLETION_PROVIDER_CLASS (klass);

  lsp_completion_provider_class->load = gbp_vls_completion_provider_load;
}

static void
gbp_vls_completion_provider_init (GbpVlsCompletionProvider *self)
{
}
