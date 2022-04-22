/* gbp-symbol-popover.c
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

#define G_LOG_DOMAIN "gbp-symbol-popover"

#include "config.h"

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-symbol-list-model.h"
#include "gbp-symbol-popover.h"

struct _GbpSymbolPopover
{
  GtkPopover       parent_instance;

  IdeSymbolTree   *symbol_tree;
  GtkFilter       *filter;

  GtkSearchEntry  *search_entry;
  GtkListView     *list_view;

  char           **search_needle;
};

enum {
  PROP_0,
  PROP_SYMBOL_TREE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GbpSymbolPopover, gbp_symbol_popover, GTK_TYPE_POPOVER)

static GParamSpec *properties [N_PROPS];

static void
gbp_symbol_popover_get_location_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(GbpSymbolPopover) self = user_data;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_NODE (node));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYMBOL_POPOVER (self));

  if ((location = ide_symbol_node_get_location_finish (node, result, &error)))
    {
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (self));
      g_autoptr(IdePanelPosition) position = ide_panel_position_new ();

      ide_editor_focus_location (workspace, position, location);

      gtk_popover_popdown (GTK_POPOVER (self));
    }

  IDE_EXIT;
}

static void
gbp_symbol_popover_activate_cb (GbpSymbolPopover *self,
                                guint             position,
                                GtkListView      *list_view)
{
  GtkTreeListRow *row;
  IdeSymbolNode *node;
  GListModel *model;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_LIST_VIEW (list_view));

  g_debug ("Activating symbol row at position %u", position);

  model = G_LIST_MODEL (gtk_list_view_get_model (list_view));
  row = g_list_model_get_item (model, position);
  node = gtk_tree_list_row_get_item (row);

  g_assert (IDE_IS_SYMBOL_NODE (node));

  ide_symbol_node_get_location_async (node,
                                      NULL,
                                      gbp_symbol_popover_get_location_cb,
                                      g_object_ref (self));

  IDE_EXIT;
}

static void
gbp_symbol_popover_search_changed_cb (GbpSymbolPopover *self,
                                      GtkSearchEntry   *search_entry)
{
  const char *text;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  text = gtk_editable_get_text (GTK_EDITABLE (search_entry));
  g_clear_pointer (&self->search_needle, g_strfreev);

  if (text && *text)
    self->search_needle = g_str_tokenize_and_fold (text, NULL, NULL);

  if (self->filter != NULL)
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
gbp_symbol_popover_dispose (GObject *object)
{
  GbpSymbolPopover *self = (GbpSymbolPopover *)object;

  g_clear_object (&self->filter);
  g_clear_object (&self->symbol_tree);

  G_OBJECT_CLASS (gbp_symbol_popover_parent_class)->dispose (object);
}

static void
gbp_symbol_popover_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbpSymbolPopover *self = GBP_SYMBOL_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_TREE:
      g_value_set_object (value, gbp_symbol_popover_get_symbol_tree (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_popover_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbpSymbolPopover *self = GBP_SYMBOL_POPOVER (object);

  switch (prop_id)
    {
    case PROP_SYMBOL_TREE:
      gbp_symbol_popover_set_symbol_tree (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_symbol_popover_class_init (GbpSymbolPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_symbol_popover_dispose;
  object_class->get_property = gbp_symbol_popover_get_property;
  object_class->set_property = gbp_symbol_popover_set_property;

  properties [PROP_SYMBOL_TREE] =
    g_param_spec_object ("symbol-tree",
                         "Symbol Tree",
                         "The symbol tree to display",
                         IDE_TYPE_SYMBOL_TREE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/symbol-tree/gbp-symbol-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolPopover, list_view);
  gtk_widget_class_bind_template_child (widget_class, GbpSymbolPopover, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, gbp_symbol_popover_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_symbol_popover_search_changed_cb);
}

static void
gbp_symbol_popover_init (GbpSymbolPopover *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_symbol_popover_new (void)
{
  return g_object_new (GBP_TYPE_SYMBOL_POPOVER, NULL);
}

IdeSymbolTree *
gbp_symbol_popover_get_symbol_tree (GbpSymbolPopover *self)
{
  g_return_val_if_fail (GBP_IS_SYMBOL_POPOVER (self), NULL);
  g_return_val_if_fail (!self->symbol_tree ||
                        IDE_IS_SYMBOL_TREE (self->symbol_tree), NULL);

  return self->symbol_tree;
}

static GListModel *
get_child_model (gpointer item,
                 gpointer user_data)
{
  IdeSymbolNode *node = item;
  IdeSymbolTree *tree = user_data;

  g_assert (IDE_IS_SYMBOL_NODE (node));
  g_assert (IDE_IS_SYMBOL_TREE (tree));

  return G_LIST_MODEL (gbp_symbol_list_model_new (tree, node));
}

static gboolean
filter_node (IdeSymbolNode      *node,
             const char * const *search_needle)
{
  /* Show only if the name matches every needle */
  for (guint i = 0; search_needle[i]; i++)
    {
      const char *name = ide_symbol_node_get_name (node);

      if (!name)
        return FALSE;

      if (g_str_match_string (search_needle[i], name, TRUE))
        continue;

      return FALSE;
    }

  return TRUE;
}

static gboolean
filter_by_name (gpointer item,
                gpointer user_data)
{
  GbpSymbolPopover *self = user_data;
  GtkTreeListRow *row = item;
  GtkTreeListRow *parent;
  IdeSymbolNode *node;
  GListModel *children;
  guint i, n;

  g_assert (GTK_IS_TREE_LIST_ROW (row));
  g_assert (GBP_IS_SYMBOL_POPOVER (self));

  /* Show all items if search is empty */
  if (!self->search_needle || !self->search_needle[0] || !*self->search_needle[0])
    return TRUE;

  /* Show a row if itself of any parent matches */
  for (parent = row; parent; parent = gtk_tree_list_row_get_parent (parent))
    {
      node = gtk_tree_list_row_get_item (parent);
      g_assert (IDE_IS_SYMBOL_NODE (node));

      if (filter_node (node, (const char * const *)self->search_needle))
        return TRUE;
    }

  /* Show a row if any child matches */
  if ((children = gtk_tree_list_row_get_children (row)))
    {
      n = g_list_model_get_n_items (children);
      for (i = 0; i < n; i++)
        {
          gboolean ret;

          node = g_list_model_get_item (children, i);
          g_assert (IDE_IS_SYMBOL_NODE (node));

          ret = filter_node (node, (const char * const *)self->search_needle);
          g_object_unref (node);
          if (ret)
            return TRUE;
        }
    }

  return FALSE;
}

void
gbp_symbol_popover_set_symbol_tree (GbpSymbolPopover *self,
                                    IdeSymbolTree    *symbol_tree)
{
  g_return_if_fail (GBP_IS_SYMBOL_POPOVER (self));
  g_return_if_fail (!symbol_tree || IDE_IS_SYMBOL_TREE (symbol_tree));

  if (g_set_object (&self->symbol_tree, symbol_tree))
    {
      gtk_list_view_set_model (self->list_view, NULL);

      if (symbol_tree != NULL)
        {
          GbpSymbolListModel *model;
          GtkTreeListModel *tree_model;
          GtkFilterListModel *filter_model;
          GtkSingleSelection *selection;
          GtkFilter *filter;

          model = gbp_symbol_list_model_new (symbol_tree, NULL);
          tree_model = gtk_tree_list_model_new (G_LIST_MODEL (model),
                                                FALSE,
                                                TRUE,
                                                get_child_model,
                                                g_object_ref (symbol_tree),
                                                g_object_unref);

          filter_model = gtk_filter_list_model_new (G_LIST_MODEL (tree_model), NULL);
          filter = GTK_FILTER (gtk_custom_filter_new (filter_by_name, self, NULL));
          gtk_filter_list_model_set_filter (filter_model, filter);
          g_set_object (&self->filter, filter);
          g_object_unref (filter);

          selection = gtk_single_selection_new (G_LIST_MODEL (filter_model));
          gtk_list_view_set_model (self->list_view, GTK_SELECTION_MODEL (selection));
          g_object_unref (selection);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYMBOL_TREE]);
    }
}
