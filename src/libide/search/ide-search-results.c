/* ide-search-results.c
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

#define G_LOG_DOMAIN "ide-search-results"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-search-result.h"
#include "ide-search-results-private.h"

struct _IdeSearchResults
{
  GObject              parent_instance;

  /* The model doing our filtering, we proxy through to it */
  GtkFilterListModel  *filter_model;
  GtkCustomFilter     *filter;

  /* The current refiltered word, >= @query with matching prefix */
  char                *refilter;

  /* The original query and it's length */
  char                *query;
  gsize                query_len;

  /* If the original search set was truncated */
  guint                truncated : 1;
};

static GType
ide_search_results_get_item_type (GListModel *model)
{
  return IDE_TYPE_SEARCH_RESULT;
}

static guint
ide_search_results_get_n_items (GListModel *model)
{
  return g_list_model_get_n_items (G_LIST_MODEL (IDE_SEARCH_RESULTS (model)->filter_model));
}

static gpointer
ide_search_results_get_item (GListModel *model,
                             guint       position)
{
  return g_list_model_get_item (G_LIST_MODEL (IDE_SEARCH_RESULTS (model)->filter_model), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_search_results_get_item_type;
  iface->get_n_items = ide_search_results_get_n_items;
  iface->get_item = ide_search_results_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeSearchResults, ide_search_results, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_search_results_dispose (GObject *object)
{
  IdeSearchResults *self = (IdeSearchResults *)object;

  if (self->filter_model != NULL)
    {
      gtk_filter_list_model_set_filter (self->filter_model, NULL);
      g_clear_object (&self->filter_model);
    }

  g_clear_object (&self->filter);

  g_clear_pointer (&self->query, g_free);
  g_clear_pointer (&self->refilter, g_free);

  G_OBJECT_CLASS (ide_search_results_parent_class)->dispose (object);
}

static void
ide_search_results_class_init (IdeSearchResultsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_search_results_dispose;
}

static void
ide_search_results_init (IdeSearchResults *self)
{
}

static gboolean
ide_search_results_filter (gpointer item,
                           gpointer user_data)
{
  IdeSearchResult *result = item;
  IdeSearchResults *self = user_data;

  return IDE_SEARCH_RESULT_GET_CLASS (result)->matches (result, self->refilter);
}

IdeSearchResults *
_ide_search_results_new (GListModel *model,
                         const char *query,
                         gboolean    truncated)
{
  IdeSearchResults *self;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);
  g_return_val_if_fail (query != NULL, NULL);

  self = g_object_new (IDE_TYPE_SEARCH_RESULTS, NULL);
  self->truncated = !!truncated;
  self->query = g_strdup (query);
  self->query_len = strlen (query);
  self->refilter = g_strdup (query);
  self->filter = gtk_custom_filter_new (ide_search_results_filter, self, NULL);
  self->filter_model = gtk_filter_list_model_new (g_object_ref (model), NULL);

  gtk_filter_list_model_set_incremental (self->filter_model, TRUE);

  g_signal_connect_object (self->filter_model,
                           "items-changed",
                           G_CALLBACK (g_list_model_items_changed),
                           self,
                           G_CONNECT_SWAPPED);

  return self;
}

gboolean
ide_search_results_refilter (IdeSearchResults *self,
                             const char       *query)
{
  g_autofree char *old_query = NULL;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SEARCH_RESULTS (self), FALSE);
  g_return_val_if_fail (query != NULL, FALSE);

  /* If this is an empty query or our original was an empty
   * query (ie: no results), then nothing to refilter.
   */
  if (query[0] == 0 || self->query_len == 0)
    IDE_RETURN (FALSE);

  /* Can't refilter truncated sets, we want a new result set
   * instead so that we get possibly missing results.
   */
  if (self->truncated)
    IDE_RETURN (FALSE);

  /* Make sure we have the prefix of the original search */
  if (memcmp (self->query, query, self->query_len) != 0)
    IDE_RETURN (FALSE);

  /* If they are exactly the same, ignore the request but
   * pretend we refiltered.
   */
  if (g_strcmp0 (self->refilter, query) == 0)
    IDE_RETURN (TRUE);

  /* Swap the filtering query */
  old_query = g_steal_pointer (&self->refilter);
  self->refilter = g_strdup (query);

  /* Notify of changes, and only update the portion not matched */
  if (g_str_has_prefix (query, old_query))
    gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_MORE_STRICT);
  else if (g_str_has_prefix (old_query, query))
    gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_LESS_STRICT);
  else
    gtk_filter_changed (GTK_FILTER (self->filter), GTK_FILTER_CHANGE_DIFFERENT);

  /* Attach filter if we haven't yet */
  if (gtk_filter_list_model_get_filter (self->filter_model) == NULL)
    gtk_filter_list_model_set_filter (self->filter_model, GTK_FILTER (self->filter));

  IDE_RETURN (TRUE);
}
