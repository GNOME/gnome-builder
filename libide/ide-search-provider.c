/* ide-search-provider.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ide-search-context.h"
#include "ide-search-provider.h"

G_DEFINE_ABSTRACT_TYPE (IdeSearchProvider, ide_search_provider, IDE_TYPE_OBJECT)

static void
ide_search_provider_class_init (IdeSearchProviderClass *klass)
{
}

static void
ide_search_provider_init (IdeSearchProvider *self)
{
}

const gchar *
ide_search_provider_get_verb (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), NULL);

  if (IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_verb)
    return IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_verb (provider);

  return NULL;
}

gint
ide_search_provider_get_priority (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), -1);

  if (IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_priority)
    return IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_priority (provider);

  return G_MAXINT;
}

gunichar
ide_search_provider_get_prefix (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), -1);

  if (IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_prefix)
    return IDE_SEARCH_PROVIDER_GET_CLASS (provider)->get_prefix (provider);

  return '\0';
}

void
ide_search_provider_populate (IdeSearchProvider *provider,
                             IdeSearchContext  *context,
                             const gchar      *search_terms,
                             gsize             max_results,
                             GCancellable     *cancellable)
{
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (search_terms);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (IDE_SEARCH_PROVIDER_GET_CLASS (provider)->populate)
    {
      IDE_SEARCH_PROVIDER_GET_CLASS (provider)->populate (provider,
                                                         context,
                                                         search_terms,
                                                         max_results,
                                                         cancellable);
      return;
    }

  g_warning ("%s does not implement populate vfunc",
             g_type_name (G_TYPE_FROM_INSTANCE (provider)));
}
