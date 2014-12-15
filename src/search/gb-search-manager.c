/* gb-search-manager.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "search-manager"

#include <glib/gi18n.h>

#include "gb-search-context.h"
#include "gb-search-manager.h"
#include "gb-search-provider.h"

struct _GbSearchManagerPrivate
{
  GList *providers;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSearchManager, gb_search_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  LAST_PROP
};

//static GParamSpec *gParamSpecs [LAST_PROP];

GbSearchManager *
gb_search_manager_new (void)
{
  return g_object_new (GB_TYPE_SEARCH_MANAGER, NULL);
}

GbSearchManager *
gb_search_manager_get_default (void)
{
  static GbSearchManager *instance;

  if (!instance)
    instance = gb_search_manager_new ();

  return instance;
}

static gint
sort_provider (gconstpointer a,
               gconstpointer b)
{
  gint prio1;
  gint prio2;

  prio1 = gb_search_provider_get_priority ((GbSearchProvider *)a);
  prio2 = gb_search_provider_get_priority ((GbSearchProvider *)b);

  return prio1 - prio2;
}

void
gb_search_manager_add_provider (GbSearchManager  *manager,
                                GbSearchProvider *provider)
{
  g_return_if_fail (GB_IS_SEARCH_MANAGER (manager));
  g_return_if_fail (GB_IS_SEARCH_PROVIDER (provider));

  manager->priv->providers =
    g_list_sort (g_list_prepend (manager->priv->providers,
                                 g_object_ref (provider)),
                 sort_provider);
}

GbSearchContext *
gb_search_manager_search (GbSearchManager *manager,
                          const gchar     *search_text)
{
  GbSearchContext *context;

  g_return_val_if_fail (GB_IS_SEARCH_MANAGER (manager), NULL);
  g_return_val_if_fail (search_text, NULL);

  context = gb_search_context_new (manager->priv->providers, search_text);

  gb_search_context_execute (context);

  return context;
}

static void
gb_search_manager_finalize (GObject *object)
{
  GbSearchManagerPrivate *priv = GB_SEARCH_MANAGER (object)->priv;

  g_list_foreach (priv->providers, (GFunc)g_object_unref, NULL);
  g_clear_pointer (&priv->providers, g_list_free);

  G_OBJECT_CLASS (gb_search_manager_parent_class)->finalize (object);
}

static void
gb_search_manager_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  //GbSearchManager *self = GB_SEARCH_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_manager_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  //GbSearchManager *self = GB_SEARCH_MANAGER (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_search_manager_class_init (GbSearchManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_search_manager_finalize;
  object_class->get_property = gb_search_manager_get_property;
  object_class->set_property = gb_search_manager_set_property;
}

static void
gb_search_manager_init (GbSearchManager *self)
{
  self->priv = gb_search_manager_get_instance_private (self);
}
