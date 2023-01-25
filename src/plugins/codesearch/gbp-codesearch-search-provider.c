/* gbp-codesearch-search-provider.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-codesearch-search-provider"

#include "config.h"

#include <libide-search.h>

#include "gbp-codesearch-search-provider.h"

struct _GbpCodesearchSearchProvider
{
  IdeObject parent_instance;
};

static void
gbp_codesearch_search_provider_load (IdeSearchProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));

  IDE_EXIT;
}

static void
gbp_codesearch_search_provider_unload (IdeSearchProvider *provider)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));

  IDE_EXIT;
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->load = gbp_codesearch_search_provider_load;
  iface->unload = gbp_codesearch_search_provider_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCodesearchSearchProvider, gbp_codesearch_search_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER, search_provider_iface_init))

static void
gbp_codesearch_search_provider_class_init (GbpCodesearchSearchProviderClass *klass)
{
}

static void
gbp_codesearch_search_provider_init (GbpCodesearchSearchProvider *self)
{
}
