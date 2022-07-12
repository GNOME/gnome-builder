/* ide-html-proposals.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-html-proposals"

#include "config.h"

#include "ide-html-proposal.h"
#include "ide-html-proposals.h"

#include "html-attributes.h"
#include "html-elements.h"
#include "css-properties.h"

struct _IdeHtmlProposals
{
  GObject  parent_instance;
  GArray  *items;
};

typedef struct
{
  const gchar *word;
  IdeHtmlProposalKind kind;
  guint priority;
} Item;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeHtmlProposals, ide_html_proposals, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
ide_html_proposals_finalize (GObject *object)
{
  IdeHtmlProposals *self = (IdeHtmlProposals *)object;

  g_clear_pointer (&self->items, g_array_unref);

  G_OBJECT_CLASS (ide_html_proposals_parent_class)->finalize (object);
}

static void
ide_html_proposals_class_init (IdeHtmlProposalsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_html_proposals_finalize;
}

static void
ide_html_proposals_init (IdeHtmlProposals *self)
{
  self->items = g_array_new (FALSE, FALSE, sizeof (Item));
}

IdeHtmlProposals *
ide_html_proposals_new (void)
{
  return g_object_new (IDE_TYPE_HTML_PROPOSALS, NULL);
}

static gint
compare_item (gconstpointer a,
              gconstpointer b)
{
  return (gint)((const Item *)a)->priority - (gint)((const Item *)b)->priority;
}

void
ide_html_proposals_refilter (IdeHtmlProposals    *self,
                             IdeHtmlProposalKind  kind,
                             const gchar         *element,
                             const gchar         *casefold)
{
  guint old_len;

  g_return_if_fail (IDE_IS_HTML_PROPOSALS (self));

  /* TODO: We can fast refilter if things match previous query */

	if ((old_len = self->items->len))
    g_array_remove_range (self->items, 0, old_len);

  /* TODO: None of these do binary search, and would benefit from that.
   *       However, the data set is small enough it's not a priority.
   */

  if (kind == IDE_HTML_PROPOSAL_ELEMENT_START ||
      kind == IDE_HTML_PROPOSAL_ELEMENT_END)
    {
      for (guint i = 0; i < G_N_ELEMENTS (html_elements); i++)
        {
          guint priority;

          if (gtk_source_completion_fuzzy_match (html_elements[i], casefold, &priority))
            {
              Item item = { html_elements[i], kind, priority };
              g_array_append_val (self->items, item);
            }
        }
    }
  else if (kind == IDE_HTML_PROPOSAL_ATTRIBUTE_NAME)
    {
      g_assert (element != NULL);

      for (guint i = 0; i < G_N_ELEMENTS (html_attributes_shared); i++)
        {
          guint priority;

          if (gtk_source_completion_fuzzy_match (html_attributes_shared[i], casefold, &priority))
            {
              Item item = { html_attributes_shared[i], kind, priority };
              g_array_append_val (self->items, item);
            }
        }

      for (guint i = 0; i < G_N_ELEMENTS (html_attributes); i++)
        {
          guint priority;

          if (strcmp (html_attributes[i].element, element) != 0)
            continue;

          if (gtk_source_completion_fuzzy_match (html_attributes[i].attr, casefold, &priority))
            {
              Item item = { html_attributes[i].attr, kind, priority };
              g_array_append_val (self->items, item);
            }
        }
    }
  else if (kind == IDE_HTML_PROPOSAL_CSS_PROPERTY)
    {
      for (guint i = 0; i < G_N_ELEMENTS (css_properties); i++)
        {
          guint priority;

          if (gtk_source_completion_fuzzy_match (css_properties[i], casefold, &priority))
            {
              Item item = { css_properties[i], kind, priority };
              g_array_append_val (self->items, item);
            }
        }
    }
  else if (kind == IDE_HTML_PROPOSAL_ATTRIBUTE_VALUE)
    {
      /* TODO: We can probably handle some of these which are "option" based */
    }

  g_array_sort (self->items, compare_item);

  g_list_model_items_changed (G_LIST_MODEL (self), 0, old_len, self->items->len);
}

static GType
ide_html_proposals_get_item_type (GListModel *model)
{
  return IDE_TYPE_HTML_PROPOSAL;
}

static guint
ide_html_proposals_get_n_items (GListModel *model)
{
  return IDE_HTML_PROPOSALS (model)->items->len;
}

static gpointer
ide_html_proposals_get_item (GListModel *model,
                             guint       position)
{
  IdeHtmlProposals *self = IDE_HTML_PROPOSALS (model);
  const Item *item = &g_array_index (self->items, Item, position);

  return ide_html_proposal_new (item->word, item->kind);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_html_proposals_get_item_type;
  iface->get_n_items = ide_html_proposals_get_n_items;
  iface->get_item = ide_html_proposals_get_item;
}
