/* ide-search-context.c
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
#include "ide-search-result.h"

struct _IdeSearchContext
{
  IdeObject     parent_instance;

  GCancellable *cancellable;
  GList        *providers;
  guint         executed : 1;
};

G_DEFINE_TYPE (IdeSearchContext, ide_search_context, IDE_TYPE_OBJECT)

enum {
  COUNT_SET,
  RESULT_ADDED,
  RESULT_REMOVED,
  LAST_SIGNAL
};

static guint gSignals [LAST_SIGNAL];

/**
 * ide_search_context_get_providers:
 *
 * Retrieve the list of providers for the search context.
 *
 * Returns: (transfer none) (element-type IdeSearchProvider*): A #GList of
 *   #IdeSearchProvider.
 */
const GList *
ide_search_context_get_providers (IdeSearchContext *self)
{
  g_return_val_if_fail (IDE_IS_SEARCH_CONTEXT (self), NULL);

  return self->providers;
}

void
ide_search_context_add_result (IdeSearchContext  *self,
                               IdeSearchProvider *provider,
                               IdeSearchResult   *result)
{
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  g_signal_emit (self, gSignals [RESULT_ADDED], 0, provider, result);
}

void
ide_search_context_remove_result (IdeSearchContext  *self,
                                  IdeSearchProvider *provider,
                                  IdeSearchResult   *result)
{
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (IDE_IS_SEARCH_RESULT (result));

  g_signal_emit (self, gSignals [RESULT_REMOVED], 0, provider, result);
}

void
ide_search_context_set_provider_count (IdeSearchContext  *self,
                                       IdeSearchProvider *provider,
                                       guint64            count)
{
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));

  g_signal_emit (self, gSignals [COUNT_SET], 0, provider, count);
}

void
ide_search_context_execute (IdeSearchContext *self,
                            const gchar      *search_terms)
{
  GList *iter;

  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));
  g_return_if_fail (!self->executed);
  g_return_if_fail (search_terms);

  self->executed = TRUE;

  for (iter = self->providers; iter; iter = iter->next)
    {
      gsize max_results = 0;

      /* TODO: Get the max results for this provider */

      ide_search_provider_populate (iter->data,
                                    self,
                                    search_terms,
                                    max_results,
                                    self->cancellable);
    }
}

void
ide_search_context_cancel (IdeSearchContext *self)
{
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);
}

void
_ide_search_context_add_provider (IdeSearchContext  *self,
                                  IdeSearchProvider *provider,
                                  gsize              max_results)
{
  g_return_if_fail (IDE_IS_SEARCH_CONTEXT (self));
  g_return_if_fail (IDE_IS_SEARCH_PROVIDER (provider));
  g_return_if_fail (!self->executed);

  self->providers = g_list_append (self->providers, g_object_ref (provider));
}

static void
ide_search_context_finalize (GObject *object)
{
  IdeSearchContext *self = (IdeSearchContext *)object;
  GList *copy;

  copy = self->providers, self->providers = NULL;
  g_list_foreach (copy, (GFunc)g_object_unref, NULL);
  g_list_free (copy);

  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (ide_search_context_parent_class)->finalize (object);
}

static void
ide_search_context_class_init (IdeSearchContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_search_context_finalize;

  gSignals [COUNT_SET] =
    g_signal_new ("count-set",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_SEARCH_PROVIDER,
                  G_TYPE_UINT64);

  gSignals [RESULT_ADDED] =
    g_signal_new ("result-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_SEARCH_PROVIDER,
                  IDE_TYPE_SEARCH_RESULT);

  gSignals [RESULT_REMOVED] =
    g_signal_new ("result-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_SEARCH_PROVIDER,
                  IDE_TYPE_SEARCH_RESULT);
}

static void
ide_search_context_init (IdeSearchContext *self)
{
  self->cancellable = g_cancellable_new ();
}
