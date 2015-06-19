/* symbol-tree-builder.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-tree.h"
#include "gb-view-grid.h"
#include "gb-workbench.h"

#include "symbol-tree-builder.h"

struct _SymbolTreeBuilder
{
  GbTreeBuilder parent_instance;
};

G_DEFINE_TYPE (SymbolTreeBuilder, symbol_tree_builder, GB_TYPE_TREE_BUILDER)

static void
symbol_tree_builder_build_node (GbTreeBuilder *builder,
                                GbTreeNode    *node)
{
  IdeSymbolNode *parent = NULL;
  IdeSymbolTree *symbol_tree;
  GbTree *tree;
  GbTreeNode *root;
  GObject *item;
  guint n_children;
  guint i;

  g_assert (GB_IS_TREE_BUILDER (builder));
  g_assert (GB_IS_TREE_NODE (node));

  if (!(tree = gb_tree_builder_get_tree (builder)) ||
      !(root = gb_tree_get_root (tree)) ||
      !(symbol_tree = IDE_SYMBOL_TREE (gb_tree_node_get_item (root))))
    return;

  item = gb_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    parent = IDE_SYMBOL_NODE (item);

  n_children = ide_symbol_tree_get_n_children (symbol_tree, parent);

  for (i = 0; i < n_children; i++)
    {
      g_autoptr(IdeSymbolNode) symbol = NULL;
      const gchar *name;
      const gchar *icon_name = NULL;
      GbTreeNode *child;
      IdeSymbolKind kind;

      symbol = ide_symbol_tree_get_nth_child (symbol_tree, parent, i);
      name = ide_symbol_node_get_name (symbol);
      kind = ide_symbol_node_get_kind (symbol);

      switch (kind)
        {
        case IDE_SYMBOL_FUNCTION:
          icon_name = "lang-function-symbolic";
          break;

        case IDE_SYMBOL_ENUM:
          icon_name = "lang-enum-symbolic";
          break;

        case IDE_SYMBOL_ENUM_VALUE:
          icon_name = "lang-enum-value-symbolic";
          break;

        case IDE_SYMBOL_STRUCT:
          icon_name = "lang-struct-symbolic";
          break;

        case IDE_SYMBOL_CLASS:
          icon_name = "lang-class-symbolic";
          break;

        case IDE_SYMBOL_METHOD:
          icon_name = "lang-method-symbolic";
          break;

        case IDE_SYMBOL_UNION:
          icon_name = "lang-union-symbolic";
          break;

        case IDE_SYMBOL_SCALAR:
        case IDE_SYMBOL_FIELD:
        case IDE_SYMBOL_VARIABLE:
          icon_name = "lang-variable-symbolic";
          break;

        case IDE_SYMBOL_NONE:
        default:
          icon_name = NULL;
          break;
        }

      child = g_object_new (GB_TYPE_TREE_NODE,
                            "text", name,
                            "icon-name", icon_name,
                            "item", symbol,
                            NULL);
      gb_tree_node_append (node, child);
    }
}

static gboolean
symbol_tree_builder_node_activated (GbTreeBuilder *builder,
                                    GbTreeNode    *node)
{
  SymbolTreeBuilder *self = (SymbolTreeBuilder *)builder;
  GtkWidget *workbench;
  GtkWidget *view_grid;
  GtkWidget *stack;
  GbTree *tree;
  GObject *item;

  g_assert (SYMBOL_IS_TREE_BUILDER (self));

  tree = gb_tree_builder_get_tree (builder);
  workbench = gtk_widget_get_ancestor (GTK_WIDGET (tree), GB_TYPE_WORKBENCH);
  view_grid = gb_workbench_get_view_grid (GB_WORKBENCH (workbench));
  stack = gb_view_grid_get_last_focus (GB_VIEW_GRID (view_grid));

  item = gb_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    {
      g_autoptr(IdeSourceLocation) location = NULL;

      location = ide_symbol_node_get_location (IDE_SYMBOL_NODE (item));
      if (location != NULL)
        {
          gb_view_stack_focus_location (GB_VIEW_STACK (stack), location);
          return TRUE;
        }
    }

  g_warning ("IdeSymbolNode did not create a source location");

  return FALSE;
}

static void
symbol_tree_builder_class_init (SymbolTreeBuilderClass *klass)
{
  GbTreeBuilderClass *builder_class = GB_TREE_BUILDER_CLASS (klass);

  builder_class->build_node = symbol_tree_builder_build_node;
  builder_class->node_activated = symbol_tree_builder_node_activated;
}

static void
symbol_tree_builder_init (SymbolTreeBuilder *self)
{
}
