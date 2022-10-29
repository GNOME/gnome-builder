/* gbp-top-match-completion-filter.c
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

#define G_LOG_DOMAIN "gbp-top-match-completion-filter"

#include "config.h"

#include <libide-core.h>

#include "gbp-top-match-completion-filter.h"
#include "gbp-top-match-completion-proposal.h"

struct _GbpTopMatchCompletionFilter
{
  GObject parent_instance;
  GbpTopMatchCompletionProposal *proposal;
  GtkSourceCompletionProvider *provider;
  GListModel *model;
  char *typed_text;
  gulong items_changed_handler;
};

static GType
gbp_top_match_completion_filter_get_item_type (GListModel *model)
{
  return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gbp_top_match_completion_filter_get_n_items (GListModel *model)
{
  return GBP_TOP_MATCH_COMPLETION_FILTER (model)->proposal ? 1 : 0;
}

static gpointer
gbp_top_match_completion_filter_get_item (GListModel *model,
                                          guint       position)
{
  if (position == 0)
    {
      GbpTopMatchCompletionFilter *self = GBP_TOP_MATCH_COMPLETION_FILTER (model);

      return self->proposal ? g_object_ref (self->proposal) : NULL;
    }

  return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = gbp_top_match_completion_filter_get_item_type;
  iface->get_n_items = gbp_top_match_completion_filter_get_n_items;
  iface->get_item = gbp_top_match_completion_filter_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTopMatchCompletionFilter, gbp_top_match_completion_filter, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_top_match_completion_filter_dispose (GObject *object)
{
  GbpTopMatchCompletionFilter *self = (GbpTopMatchCompletionFilter *)object;

  if (self->model != NULL)
    g_clear_signal_handler (&self->items_changed_handler, self->model);

  g_clear_object (&self->proposal);
  g_clear_object (&self->provider);
  g_clear_object (&self->model);
  g_clear_pointer (&self->typed_text, g_free);

  G_OBJECT_CLASS (gbp_top_match_completion_filter_parent_class)->dispose (object);
}

static void
gbp_top_match_completion_filter_class_init (GbpTopMatchCompletionFilterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_top_match_completion_filter_dispose;
}

static void
gbp_top_match_completion_filter_init (GbpTopMatchCompletionFilter *self)
{
}

static void
gbp_top_match_completion_filter_update (GbpTopMatchCompletionFilter *self,
                                        gboolean                     definitely_has_items)
{
  guint removed = 0;
  guint added = 0;

  g_assert (GBP_IS_TOP_MATCH_COMPLETION_FILTER (self));

  if (self->proposal != NULL)
    {
      g_clear_object (&self->proposal);
      removed = 1;
    }

  if (!ide_str_empty0 (self->typed_text) &&
      self->provider != NULL &&
      self->model != NULL &&
      (definitely_has_items || g_list_model_get_n_items (self->model) > 0))
    {
      g_autoptr(GtkSourceCompletionProposal) first = g_list_model_get_item (self->model, 0);
      g_autofree char *typed_text = NULL;

      if (first != NULL &&
          (typed_text = gtk_source_completion_proposal_get_typed_text (first)) &&
          ide_str_equal0 (self->typed_text, typed_text))
        {
          self->proposal = gbp_top_match_completion_proposal_new (self->provider, first);
          added = 1;
        }
    }

  if (removed || added)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, added);
}


GbpTopMatchCompletionFilter *
gbp_top_match_completion_filter_new (GtkSourceCompletionProvider *provider,
                                     GListModel                  *model)
{
  GbpTopMatchCompletionFilter *self;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider), NULL);
  g_return_val_if_fail (!model || G_IS_LIST_MODEL (model), NULL);

  self = g_object_new (GBP_TYPE_TOP_MATCH_COMPLETION_FILTER, NULL);
  g_set_object (&self->provider, provider);
  g_set_object (&self->model, model);

  gbp_top_match_completion_filter_update (self, FALSE);

  return self;
}

static void
gbp_top_match_completion_filter_items_changed_cb (GbpTopMatchCompletionFilter *self,
                                                  guint                        position,
                                                  guint                        added,
                                                  guint                        removed,
                                                  GListModel                  *model)
{
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_FILTER (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (position == 0 && (added || removed))
    gbp_top_match_completion_filter_update (self, added > removed);
}

void
gbp_top_match_completion_filter_set_model (GbpTopMatchCompletionFilter *self,
                                           GListModel                  *model)
{
  g_return_if_fail (GBP_IS_TOP_MATCH_COMPLETION_FILTER (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));

  if (self->model == model)
    return;

  if (self->model != NULL)
    g_clear_signal_handler (&self->items_changed_handler, self->model);

  g_set_object (&self->model, model);

  if (self->model != NULL)
    self->items_changed_handler =
      g_signal_connect_object (self->model,
                               "items-changed",
                               G_CALLBACK (gbp_top_match_completion_filter_items_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  gbp_top_match_completion_filter_update (self, FALSE);
}

void
gbp_top_match_completion_filter_set_typed_text (GbpTopMatchCompletionFilter *self,
                                                const char                  *typed_text)
{
  g_return_if_fail (GBP_IS_TOP_MATCH_COMPLETION_FILTER (self));

  if (g_set_str (&self->typed_text, typed_text))
    gbp_top_match_completion_filter_update (self, FALSE);
}

GtkSourceCompletionProvider *
gbp_top_match_completion_filter_get_provider (GbpTopMatchCompletionFilter *self)
{
  g_return_val_if_fail (GBP_IS_TOP_MATCH_COMPLETION_FILTER (self), NULL);

  return self->provider;
}

