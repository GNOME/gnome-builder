/* gbp-vls-search-provider.c
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

#define G_LOG_DOMAIN "gbp-vls-search-provider"

#include "config.h"

#include <libide-foundry.h>
#include <libide-search.h>

#include "gbp-vls-search-provider.h"
#include "gbp-vls-service.h"

struct _GbpVlsSearchProvider
{
  IdeLspSearchProvider parent_instance;
};

static void
gbp_vls_search_provider_load (IdeSearchProvider *provider)
{
  g_autoptr(IdeLspServiceClass) klass = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VLS_SEARCH_PROVIDER (provider));

  context = ide_object_get_context (IDE_OBJECT (provider));

  if (!ide_context_has_project (context))
    IDE_EXIT;

  build_system = ide_build_system_from_context (context);

  if (!ide_build_system_supports_language (build_system, "vala"))
    {
      g_debug ("%s does not advertise use of Vala in project. Searches will be ignored.",
               G_OBJECT_TYPE_NAME (build_system));
      IDE_EXIT;
    }

  klass = g_type_class_ref (GBP_TYPE_VLS_SERVICE);
  ide_lsp_service_class_bind_client_lazy (klass, IDE_OBJECT (provider));

  IDE_EXIT;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = gbp_vls_search_provider_load;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpVlsSearchProvider, gbp_vls_search_provider, IDE_TYPE_LSP_SEARCH_PROVIDER,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_vls_search_provider_class_init (GbpVlsSearchProviderClass *klass)
{
}

static void
gbp_vls_search_provider_init (GbpVlsSearchProvider *self)
{
}
