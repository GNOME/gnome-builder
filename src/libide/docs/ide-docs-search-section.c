/* ide-docs-search-section.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-docs-search-section"

#include "config.h"

#include <dazzle.h>

#include "ide-docs-search-model.h"
#include "ide-docs-search-row.h"
#include "ide-docs-search-section.h"

#define MAX_ALLOWED_BY_GROUP 1000

struct _IdeDocsSearchSection
{
  GtkBin              parent_instance;

  DzlListBox         *groups;

  gchar              *title;

  gint                priority;

  guint               show_all_results : 1;
};

G_DEFINE_TYPE (IdeDocsSearchSection, ide_docs_search_section, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_PRIORITY,
  PROP_SHOW_ALL_RESULTS,
  PROP_TITLE,
  N_PROPS
};

enum {
  ITEM_ACTIVATED,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
on_row_activated_cb (IdeDocsSearchSection *self,
                     IdeDocsSearchRow     *row,
                     DzlListBox           *list_box)
{
  IdeDocsItem *item;

  g_assert (IDE_IS_DOCS_SEARCH_SECTION (self));
  g_assert (IDE_IS_DOCS_SEARCH_ROW (row));
  g_assert (DZL_IS_LIST_BOX (list_box));

  item = ide_docs_search_row_get_item (row);

  g_signal_emit (self, signals [ITEM_ACTIVATED], 0, item);
}

static void
ide_docs_search_section_finalize (GObject *object)
{
  IdeDocsSearchSection *self = (IdeDocsSearchSection *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (ide_docs_search_section_parent_class)->finalize (object);
}

static void
ide_docs_search_section_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeDocsSearchSection *self = IDE_DOCS_SEARCH_SECTION (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, ide_docs_search_section_get_priority (self));
      break;

    case PROP_SHOW_ALL_RESULTS:
      g_value_set_boolean (value, self->show_all_results);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_section_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeDocsSearchSection *self = IDE_DOCS_SEARCH_SECTION (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      ide_docs_search_section_set_priority (self, g_value_get_int (value));
      break;

    case PROP_SHOW_ALL_RESULTS:
      self->show_all_results = g_value_get_boolean (value);
      break;

    case PROP_TITLE:
      self->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_search_section_class_init (IdeDocsSearchSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = ide_docs_search_section_finalize;
  object_class->get_property = ide_docs_search_section_get_property;
  object_class->set_property = ide_docs_search_section_set_property;

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "THe priority of the section",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_ALL_RESULTS] =
    g_param_spec_boolean ("show-all-results",
                          "Show All Results",
                          "Show all of the results from groups",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the section",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "IdeDocsSearchSection");

  signals [ITEM_ACTIVATED] =
    g_signal_new ("item-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, IDE_TYPE_DOCS_ITEM);
}

static void
ide_docs_search_section_init (IdeDocsSearchSection *self)
{
  self->groups = g_object_new (DZL_TYPE_LIST_BOX,
                               "row-type", IDE_TYPE_DOCS_SEARCH_ROW,
                               "property-name", "item",
                               "selection-mode", GTK_SELECTION_NONE,
                               "visible", TRUE,
                               NULL);
  dzl_list_box_set_recycle_max (self->groups, 100);
  g_signal_connect_object (self->groups,
                           "row-activated",
                           G_CALLBACK (on_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->groups));
}

GtkWidget *
ide_docs_search_section_new (const gchar *title)
{
  return g_object_new (IDE_TYPE_DOCS_SEARCH_SECTION,
                       "title", title,
                       NULL);
}

const gchar *
ide_docs_search_section_get_title (IdeDocsSearchSection *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self), NULL);

  return self->title;
}

gint
ide_docs_search_section_get_priority (IdeDocsSearchSection *self)
{
  g_return_val_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self), 0);

  return self->priority;
}

void
ide_docs_search_section_set_priority (IdeDocsSearchSection *self,
                                      gint                  priority)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self));

  if (priority != self->priority)
    {
      self->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

void
ide_docs_search_section_add_groups (IdeDocsSearchSection *self,
                                    IdeDocsItem          *parent)
{
  g_return_if_fail (IDE_IS_DOCS_SEARCH_SECTION (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (parent));

  /* Clear state before we add new stuff, so we get a chance to
   * re-use cached listbox rows.
   */
  dzl_list_box_set_model (self->groups, NULL);
  gtk_widget_hide (GTK_WIDGET (self->groups));

  if (self->show_all_results)
    {
      g_autoptr(GListStore) model = g_list_store_new (IDE_TYPE_DOCS_ITEM);
      g_autoptr(IdeDocsItem) copy = NULL;

      /* Make a fake title with no children so we don't get
       * the +123 items in the header.
       */
      copy = ide_docs_item_new ();
      ide_docs_item_set_title (copy, ide_docs_item_get_title (parent));
      ide_docs_item_set_kind (copy, IDE_DOCS_ITEM_KIND_BOOK);
      g_list_store_append (model, copy);

      for (const GList *iter = ide_docs_item_get_children (parent);
           iter != NULL;
           iter = iter->next)
        {
          IdeDocsItem *child = iter->data;

          g_assert (IDE_IS_DOCS_ITEM (child));

          g_list_store_append (model, child);
        }

      dzl_list_box_set_model (self->groups, G_LIST_MODEL (model));
    }
  else
    {
      g_autoptr(IdeDocsSearchModel) model = ide_docs_search_model_new ();

      for (const GList *iter = ide_docs_item_get_children (parent);
           iter != NULL;
           iter = iter->next)
        {
          IdeDocsItem *child = iter->data;

          g_assert (IDE_IS_DOCS_ITEM (child));

          /* Truncate to a reasonable number to avoid very large lists */
          ide_docs_item_truncate (child, MAX_ALLOWED_BY_GROUP);

          ide_docs_search_model_add_group (model, child);

          dzl_list_box_set_model (self->groups, G_LIST_MODEL (model));
        }
    }

  gtk_widget_show (GTK_WIDGET (self->groups));
}
