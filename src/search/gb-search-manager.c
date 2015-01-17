/* gb-search-manager.c
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

#include "gb-search-manager.h"

struct _GbSearchManagerPrivate
{
  GList *providers;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchManager, gb_search_manager, G_TYPE_OBJECT)

enum {
  PROVIDER_ADDED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

GbSearchManager *
gb_search_manager_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_MANAGER, NULL);
}

GbSearchContext *
gb_search_manager_search (GbSearchManager *manager,
                          const GList     *providers,
                          const gchar     *search_terms)
{
  GbSearchContext *context;
  const GList *iter;

  g_return_val_if_fail (GB_IS_SEARCH_MANAGER (manager), NULL);
  g_return_val_if_fail (search_terms, NULL);

  if (!providers)
    providers = manager->priv->providers;

  if (!providers)
    return NULL;

  context = gb_search_context_new ();

  for (iter = providers; iter; iter = iter->next)
    gb_search_context_add_provider (context, iter->data, 0);

  return context;
}

/**
 * gb_search_manager_get_providers:
 *
 * Returns the providers attached to the search manager.
 *
 * Returns: (transfer container) (element-type GbSearchProvider*): A #GList of
 *   #GbSearchProvider.
 */
GList *
gb_search_manager_get_providers (GbSearchManager *manager)
{
  g_return_val_if_fail (GB_IS_SEARCH_MANAGER (manager), NULL);

  return g_list_copy (manager->priv->providers);
}

void
gb_search_manager_add_provider (GbSearchManager  *manager,
                                GbSearchProvider *provider)
{
  g_return_if_fail (GB_IS_SEARCH_MANAGER (manager));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  manager->priv->providers = g_list_append (manager->priv->providers,
                                            g_object_ref (provider));
  g_signal_emit (manager, gSignals [PROVIDER_ADDED], 0, provider);
}

static void
gb_search_manager_finalize (GObject *object)
{
  GbSearchManagerPrivate *priv = GB_SEARCH_MANAGER (object)->priv;

  g_list_foreach (priv->providers, (GFunc)g_object_unref, NULL);
  g_list_free (priv->providers);
  priv->providers = NULL;

  G_OBJECT_CLASS (gb_search_manager_parent_class)->finalize (object);
}

static void
gb_search_manager_class_init (GbSearchManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_search_manager_finalize;

  gSignals [PROVIDER_ADDED] =
    g_signal_new ("provider-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_SEARCH_PROVIDER);
}

static void
gb_search_manager_init (GbSearchManager *self)
{
  self->priv = gb_search_manager_get_instance_private (self);
}
