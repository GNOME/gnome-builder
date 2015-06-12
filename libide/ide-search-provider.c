/* ide-search-provider.c
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

#include "ide-context.h"
#include "ide-search-context.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"

G_DEFINE_INTERFACE (IdeSearchProvider, ide_search_provider, IDE_TYPE_OBJECT)

static const gchar *
ide_search_provider_real_get_verb (IdeSearchProvider *self)
{
  return "";
}

static gint
ide_search_provider_real_get_priority (IdeSearchProvider *self)
{
  return -1;
}

static gunichar
ide_search_provider_real_get_prefix (IdeSearchProvider *self)
{
  return '\0';
}

static GtkWidget *
ide_search_provider_real_create_row (IdeSearchProvider *self,
                                     IdeSearchResult   *result)
{
  return NULL;
}

static void
ide_search_provider_real_activate (IdeSearchProvider *self,
                                   GtkWidget         *row,
                                   IdeSearchResult   *result)
{
}

static void
ide_search_provider_default_init (IdeSearchProviderInterface *iface)
{
  iface->get_verb = ide_search_provider_real_get_verb;
  iface->get_priority = ide_search_provider_real_get_priority;
  iface->get_prefix = ide_search_provider_real_get_prefix;
  iface->create_row = ide_search_provider_real_create_row;
  iface->activate = ide_search_provider_real_activate;

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            _("Context"),
                                                            _("Context"),
                                                            IDE_TYPE_CONTEXT,
                                                            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

const gchar *
ide_search_provider_get_verb (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), NULL);

  return IDE_SEARCH_PROVIDER_GET_IFACE (provider)->get_verb (provider);
}

gint
ide_search_provider_get_priority (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), -1);

  return IDE_SEARCH_PROVIDER_GET_IFACE (provider)->get_priority (provider);
}

gunichar
ide_search_provider_get_prefix (IdeSearchProvider *provider)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (provider), -1);

  return IDE_SEARCH_PROVIDER_GET_IFACE (provider)->get_prefix (provider);
}

void
ide_search_provider_populate (IdeSearchProvider *provider,
                              IdeSearchContext  *context,
                              const gchar       *search_terms,
                              gsize              max_results,
                              GCancellable      *cancellable)
{
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (search_terms != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  return IDE_SEARCH_PROVIDER_GET_IFACE (provider)->populate (provider,
                                                             context,
                                                             search_terms,
                                                             max_results,
                                                             cancellable);
}

/**
 * ide_search_provider_create_row:
 * @provider: A #IdeSearchProvider.
 * @result: A #IdeSearchResult.
 *
 * Create a row to display the search result.
 *
 * Returns: (transfer full): A #GtkWidget.
 */
GtkWidget *
ide_search_provider_create_row (IdeSearchProvider *self,
                                IdeSearchResult   *result)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (self), NULL);
  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (result), NULL);

  return IDE_SEARCH_PROVIDER_GET_IFACE (self)->create_row (self, result);
}

void
ide_search_provider_activate (IdeSearchProvider *self,
                              GtkWidget         *row,
                              IdeSearchResult   *result)
{
  g_return_val_if_fail (IDE_IS_SEARCH_PROVIDER (self), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (row), NULL);
  g_return_val_if_fail (IDE_IS_SEARCH_RESULT (result), NULL);

  return IDE_SEARCH_PROVIDER_GET_IFACE (self)->activate (self, row, result);
}
