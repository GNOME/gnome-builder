/* gbp-jedi-hover-provider.c
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

#define G_LOG_DOMAIN "gbp-jedi-hover-provider"

#include "config.h"

#include "gbp-jedi-hover-provider.h"
#include "gbp-jedi-service.h"

struct _GbpJediHoverProvider
{
  IdeLspHoverProvider parent_instance;
};

static void
gbp_jedi_hover_provider_prepare (IdeLspHoverProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_JEDI_HOVER_PROVIDER (provider));

  g_object_set (provider,
                "category", "Python",
                "priority", 200,
                NULL);

  klass = g_type_class_ref (GBP_TYPE_JEDI_SERVICE);
  ide_lsp_service_class_bind_client (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

G_DEFINE_FINAL_TYPE (GbpJediHoverProvider, gbp_jedi_hover_provider, IDE_TYPE_LSP_HOVER_PROVIDER)

static void
gbp_jedi_hover_provider_class_init (GbpJediHoverProviderClass *klass)
{
  IdeLspHoverProviderClass *lsp_hover_provider_class = IDE_LSP_HOVER_PROVIDER_CLASS (klass);

  lsp_hover_provider_class->prepare = gbp_jedi_hover_provider_prepare;
}

static void
gbp_jedi_hover_provider_init (GbpJediHoverProvider *self)
{
}
