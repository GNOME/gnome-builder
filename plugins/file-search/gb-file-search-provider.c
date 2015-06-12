/* gb-file-search-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
 */

#include <glib/gi18n.h>

#include "gb-file-search-provider.h"

struct _GbFileSearchProvider
{
  IdeObject parent_instance;
};

static void search_provider_iface_init (IdeSearchProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbFileSearchProvider, gb_file_search_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SEARCH_PROVIDER,
                                                search_provider_iface_init))

static const gchar *
gb_file_search_provider_get_verb (IdeSearchProvider *provider)
{
  return _("Switch To");
}

static void
gb_file_search_provider_populate (IdeSearchProvider *provider,
                                  IdeSearchContext  *context,
                                  const gchar       *search_terms,
                                  gsize              max_results,
                                  GCancellable      *cancellable)
{
  g_assert (IDE_IS_SEARCH_PROVIDER (provider));
  g_assert (IDE_IS_SEARCH_CONTEXT (context));
  g_assert (search_terms != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_search_context_provider_completed (context, provider);
}

static void
gb_file_search_provider_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_file_search_provider_parent_class)->finalize (object);
}

static void
gb_file_search_provider_class_init (GbFileSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_file_search_provider_finalize;
}

static void
gb_file_search_provider_init (GbFileSearchProvider *self)
{
}

static void
search_provider_iface_init (IdeSearchProviderInterface *iface)
{
  iface->populate = gb_file_search_provider_populate;
  iface->get_verb = gb_file_search_provider_get_verb;
}
