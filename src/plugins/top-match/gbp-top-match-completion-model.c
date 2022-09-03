/* gbp-top-match-completion-model.c
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

#define G_LOG_DOMAIN "gbp-top-match-completion-model"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include <libide-core.h>

#include "gbp-top-match-completion-filter.h"
#include "gbp-top-match-completion-model.h"
#include "gbp-top-match-completion-provider.h"

struct _GbpTopMatchCompletionModel
{
  GObject              parent_instance;
  GListStore          *filters;
  GtkFlattenListModel *flatten;
};

static GType
gbp_top_match_completion_model_get_item_type (GListModel *model)
{
  return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gbp_top_match_completion_model_get_n_items (GListModel *model)
{
  GbpTopMatchCompletionModel *self = GBP_TOP_MATCH_COMPLETION_MODEL (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->flatten));
}

static gpointer
gbp_top_match_completion_model_get_item (GListModel *model,
                                         guint       position)
{
  GbpTopMatchCompletionModel *self = GBP_TOP_MATCH_COMPLETION_MODEL (model);

  return g_list_model_get_item (G_LIST_MODEL (self->flatten), position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = gbp_top_match_completion_model_get_n_items;
  iface->get_item_type = gbp_top_match_completion_model_get_item_type;
  iface->get_item = gbp_top_match_completion_model_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTopMatchCompletionModel, gbp_top_match_completion_model, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gbp_top_match_completion_model_provider_model_changed_cb (GbpTopMatchCompletionModel  *self,
                                                          GtkSourceCompletionProvider *provider,
                                                          GListModel                  *model,
                                                          GtkSourceCompletionContext  *context)
{
  GListModel *filters;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TOP_MATCH_COMPLETION_MODEL (self));
  g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
  g_assert (!model || G_IS_LIST_MODEL (model));
  g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

  filters = G_LIST_MODEL (self->filters);
  n_items = g_list_model_get_n_items (filters);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpTopMatchCompletionFilter) filter = g_list_model_get_item (filters, i);

      if (provider == gbp_top_match_completion_filter_get_provider (filter))
        {
          gbp_top_match_completion_filter_set_model (filter, model);
          break;
        }
    }
}

static void
gbp_top_match_completion_model_dispose (GObject *object)
{
  GbpTopMatchCompletionModel *self = (GbpTopMatchCompletionModel *)object;

  g_clear_object (&self->flatten);
  g_clear_object (&self->filters);

  G_OBJECT_CLASS (gbp_top_match_completion_model_parent_class)->dispose (object);
}

static void
gbp_top_match_completion_model_class_init (GbpTopMatchCompletionModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_top_match_completion_model_dispose;
}

static void
gbp_top_match_completion_model_init (GbpTopMatchCompletionModel *self)
{
  self->filters = g_list_store_new (G_TYPE_LIST_MODEL);
  self->flatten = gtk_flatten_list_model_new (g_object_ref (G_LIST_MODEL (self->filters)));
}

GbpTopMatchCompletionModel *
gbp_top_match_completion_model_new (GtkSourceCompletionContext *context)
{
  GbpTopMatchCompletionModel *self;
  g_autoptr(GListModel) providers = NULL;
  g_autofree char *word = NULL;
  guint n_items;

  g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);

  self = g_object_new (GBP_TYPE_TOP_MATCH_COMPLETION_MODEL, NULL);

  word = gtk_source_completion_context_get_word (context);
  providers = gtk_source_completion_context_list_providers (context);
  n_items = g_list_model_get_n_items (providers);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkSourceCompletionProvider) provider = g_list_model_get_item (providers, i);
      g_autoptr(GbpTopMatchCompletionFilter) filter = NULL;
      GListModel *model;

      if (GBP_IS_TOP_MATCH_COMPLETION_PROVIDER (provider))
        continue;

      model = gtk_source_completion_context_get_proposals_for_provider (context, provider);
      filter = gbp_top_match_completion_filter_new (provider, model);
      gbp_top_match_completion_filter_set_typed_text (filter, word);
      g_list_store_append (self->filters, filter);
    }

  g_signal_connect_object (context,
                           "provider-model-changed",
                           G_CALLBACK (gbp_top_match_completion_model_provider_model_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);


  return self;
}

void
gbp_top_match_completion_model_set_typed_text (GbpTopMatchCompletionModel *self,
                                               const char                 *typed_text)
{
  GListModel *filters;
  guint n_items;

  g_return_if_fail (GBP_IS_TOP_MATCH_COMPLETION_MODEL (self));

  filters = G_LIST_MODEL (self->filters);
  n_items = g_list_model_get_n_items (filters);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpTopMatchCompletionFilter) filter = g_list_model_get_item (filters, i);
      gbp_top_match_completion_filter_set_typed_text (filter, typed_text);
    }
}
