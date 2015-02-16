/* ide-search-engine.c
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

#include <glib/gi18n.h>

#include "ide-internal.h"
#include "ide-search-context.h"
#include "ide-search-engine.h"
#include "ide-search-provider.h"
#include "ide-search-result.h"

struct _IdeSearchEngine
{
  IdeObject  parent_instance;
  GList     *providers;
};

G_DEFINE_TYPE (IdeSearchEngine, ide_search_engine, IDE_TYPE_OBJECT)

enum {
  PROVIDER_ADDED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

/**
 * ide_search_engine_search:
 * @providers: (allow-none) (element-type IdeSearchProvider*): Optional list
 *   of specific providers to use when searching.
 * @search_terms: The search terms.
 *
 * Begins a query against the requested search providers.
 *
 * If @providers is %NULL, all registered providers will be used.
 *
 * Returns: (transfer full) (nullable): An #IdeSearchContext or %NULL if no
 *   providers could be loaded.
 */
IdeSearchContext *
ide_search_engine_search (IdeSearchEngine *self,
                          const GList     *providers,
                          const gchar     *search_terms)
{
  IdeSearchContext *context;
  const GList *iter;

  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);
  g_return_val_if_fail (search_terms, NULL);

  if (!providers)
    providers = self->providers;

  if (!providers)
    return NULL;

  context = g_object_new (IDE_TYPE_SEARCH_CONTEXT,
                          "context", context,
                          NULL);

  for (iter = providers; iter; iter = iter->next)
    _ide_search_context_add_provider (context, iter->data, 0);

  return context;
}

/**
 * ide_search_engine_get_providers:
 *
 * Returns the list of registered search providers.
 *
 * Returns: (transfer none) (element-type IdeSearchProvider*): A #GList of
 *   #IdeSearchProvider.
 */
GList *
ide_search_engine_get_providers (IdeSearchEngine *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_ENGINE (self), NULL);

  return self->providers;
}

void
ide_search_engine_add_provider (IdeSearchEngine   *self,
                                IdeSearchProvider *provider)
{
  g_return_if_fail (IDE_IS_SEARCH_ENGINE (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  self->providers = g_list_append (self->providers, g_object_ref (provider));
  g_signal_emit (self, gSignals [PROVIDER_ADDED], 0, provider);
}

static void
ide_search_engine_dispose (GObject *object)
{
  IdeSearchEngine *self = (IdeSearchEngine *)object;
  GList *copy;

  copy = self->providers;
  self->providers = NULL;
  g_list_foreach (copy, (GFunc)g_object_unref, NULL);
  g_list_free (copy);

  G_OBJECT_CLASS (ide_search_engine_parent_class)->dispose (object);
}

static void
ide_search_engine_class_init (IdeSearchEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_search_engine_dispose;

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
                  IDE_TYPE_SEARCH_PROVIDER);
}

static void
ide_search_engine_init (IdeSearchEngine *self)
{
}
