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

  IdePatternSpec  *search_needle;
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
      g_autoptr(PanelPosition) position = panel_position_new ();

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
gbp_symbol_popover_search_activate_cb (GbpSymbolPopover *self,
                                       GtkSearchEntry   *search_entry)
{
  guint position;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  if (ide_gtk_list_view_get_selected_row (self->list_view, &position))
    gbp_symbol_popover_activate_cb (self, position, self->list_view);
}

static void
gbp_symbol_popover_search_changed_cb (GbpSymbolPopover *self,
                                      GtkSearchEntry   *search_entry)
{
  const char *text;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_SEARCH_ENTRY (search_entry));

  text = gtk_editable_get_text (GTK_EDITABLE (search_entry));
  g_clear_pointer (&self->search_needle, ide_pattern_spec_unref);

  if (text && *text)
    self->search_needle = ide_pattern_spec_new (text);

  if (self->filter != NULL)
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
gbp_symbol_popover_grab_focus (GtkWidget *widget)
{
  GbpSymbolPopover *self = (GbpSymbolPopover *)widget;
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_POPOVER (self));

  ret = gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
  gtk_editable_select_region (GTK_EDITABLE (self->search_entry), 0, -1);

  IDE_RETURN (ret);
}

static gboolean
on_search_key_pressed_cb (GbpSymbolPopover      *self,
                          guint                  keyval,
                          guint                  keycode,
                          GdkModifierType        state,
                          GtkEventControllerKey *key)
{
  g_assert (GBP_IS_SYMBOL_POPOVER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  if ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) == 0)
    {
      switch (keyval)
        {
        case GDK_KEY_Escape:
          {
            IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (self));
            IdePage *page = ide_workspace_get_most_recent_page (workspace);

            gtk_popover_popdown (GTK_POPOVER (self));
            if (page)
              gtk_widget_grab_focus (GTK_WIDGET (page));

            return TRUE;
          }

        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          ide_gtk_list_view_move_previous (self->list_view);
          return TRUE;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          ide_gtk_list_view_move_next (self->list_view);
          return TRUE;

        default:
          break;
        }
    }

  return FALSE;
}

static void
gbp_symbol_popover_dispose (GObject *object)
{
  GbpSymbolPopover *self = (GbpSymbolPopover *)object;

  g_clear_object (&self->filter);
  g_clear_object (&self->symbol_tree);
  g_clear_pointer (&self->search_needle, ide_pattern_spec_unref);

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

  widget_class->grab_focus = gbp_symbol_popover_grab_focus;

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
  gtk_widget_class_bind_template_callback (widget_class, gbp_symbol_popover_search_activate_cb);
  gtk_widget_class_bind_template_callback (widget_class, gbp_symbol_popover_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_search_key_pressed_cb);
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

  if (ide_symbol_tree_get_n_children (tree, node) == 0)
    return NULL;

  return G_LIST_MODEL (gbp_symbol_list_model_new (tree, node));
}

static gboolean
filter_node (IdeSymbolNode  *node,
             IdePatternSpec *search_needle)
{
  const char *name = ide_symbol_node_get_name (node);
  if (name && ide_pattern_spec_match (search_needle, name))
    return TRUE;

  {
    g_autofree char *display_name = NULL;

    g_object_get (node, "display-name", &display_name, NULL);
    if (display_name && ide_pattern_spec_match (search_needle, display_name))
      return TRUE;
  }

  return FALSE;
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
  if (!self->search_needle)
    return TRUE;

  /* Show a row if itself or any parent matches */
  for (parent = row; parent; parent = gtk_tree_list_row_get_parent (parent))
    {
      node = gtk_tree_list_row_get_item (parent);
      g_assert (IDE_IS_SYMBOL_NODE (node));

      if (filter_node (node, self->search_needle))
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

          ret = filter_node (node, self->search_needle);
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

GListModel *
gbp_symbol_popover_get_model (GbpSymbolPopover *self)
{
  GtkSelectionModel *selection;
  GListModel *model;

  g_return_val_if_fail (GBP_IS_SYMBOL_POPOVER (self), NULL);

  if (self->list_view == NULL ||
      !(selection = gtk_list_view_get_model (self->list_view)) ||
      !GTK_IS_SINGLE_SELECTION (selection))
    return NULL;

  model = gtk_single_selection_get_model (GTK_SINGLE_SELECTION (selection));

  if (GTK_IS_FILTER_LIST_MODEL (model))
    {
      GListModel *filtered;

      if ((filtered = gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (model))))
        return filtered;
    }

  return model;
}
