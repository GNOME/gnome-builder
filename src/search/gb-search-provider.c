/* gb-search-provider.c
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

#include "gb-search-provider.h"

G_DEFINE_ABSTRACT_TYPE (GbSearchProvider, gb_search_provider, G_TYPE_OBJECT)

const gchar *
gb_search_provider_get_verb (GbSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_SEARCH_PROVIDER (provider), NULL);

  if (GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_verb)
    return GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_verb (provider);

  return NULL;
}

gint
gb_search_provider_get_priority (GbSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_SEARCH_PROVIDER (provider), NULL);

  if (GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_priority)
    return GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_priority (provider);

  return G_MAXINT;
}

gunichar
gb_search_provider_get_prefix (GbSearchProvider *provider)
{
  g_return_val_if_fail (GB_IS_SEARCH_PROVIDER (provider), NULL);

  if (GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_prefix)
    return GB_SEARCH_PROVIDER_GET_CLASS (provider)->get_prefix (provider);

  return '\0';
}

void
gb_search_provider_populate (GbSearchProvider *provider,
                             GbSearchContext  *context,
                             const gchar      *search_terms,
                             gsize             max_results,
                             GCancellable     *cancellable)
{
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (GB_IS_SEARCH_CONTEXT (context));
  g_return_if_fail (search_terms);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (GB_SEARCH_PROVIDER_GET_CLASS (provider)->populate)
    {
      GB_SEARCH_PROVIDER_GET_CLASS (provider)->populate (provider,
                                                         context,
                                                         search_terms,
                                                         max_results,
                                                         cancellable);
      return;
    }

  g_warning ("%s does not implement populate vfunc",
             g_type_name (G_TYPE_FROM_INSTANCE (provider)));
}

static void
gb_search_provider_finalize (GObject *object)
{
  GbSearchProviderPrivate *priv = GB_SEARCH_PROVIDER (object)->priv;

  G_OBJECT_CLASS (gb_search_provider_parent_class)->finalize (object);
}

static void
gb_search_provider_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbSearchProvider *self = GB_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_provider_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbSearchProvider *self = GB_SEARCH_PROVIDER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_provider_class_init (GbSearchProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_search_provider_finalize;
  object_class->get_property = gb_search_provider_get_property;
  object_class->set_property = gb_search_provider_set_property;
}

static void
gb_search_provider_init (GbSearchProvider *self)
{
  self->priv = gb_search_provider_get_instance_private (self);
}
