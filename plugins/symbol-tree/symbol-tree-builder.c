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

#define G_LOG_DOMAIN "symbol-tree-builder"

#include <glib/gi18n.h>
#include <ide.h>

#include "symbol-tree-builder.h"

struct _SymbolTreeBuilder
{
  IdeTreeBuilder parent_instance;
};

G_DEFINE_TYPE (SymbolTreeBuilder, symbol_tree_builder, IDE_TYPE_TREE_BUILDER)

static void
symbol_tree_builder_build_node (IdeTreeBuilder *builder,
                                IdeTreeNode    *node)
{
  IdeSymbolNode *parent = NULL;
  IdeSymbolTree *symbol_tree;
  IdeTree *tree;
  IdeTreeNode *root;
  GObject *item;
  guint n_children;
  guint i;

  g_assert (IDE_IS_TREE_BUILDER (builder));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!(tree = ide_tree_builder_get_tree (builder)) ||
      !(root = ide_tree_get_root (tree)) ||
      !(symbol_tree = IDE_SYMBOL_TREE (ide_tree_node_get_item (root))))
    return;

  item = ide_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    parent = IDE_SYMBOL_NODE (item);

  n_children = ide_symbol_tree_get_n_children (symbol_tree, parent);

  for (i = 0; i < n_children; i++)
    {
      g_autoptr(IdeSymbolNode) symbol = NULL;
      const gchar *name;
      const gchar *icon_name = NULL;
      IdeTreeNode *child;
      IdeSymbolKind kind;
      gboolean has_children;

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

        case IDE_SYMBOL_HEADER:
        case IDE_SYMBOL_NONE:
        default:
          icon_name = NULL;
          break;
        }

      has_children = !!ide_symbol_tree_get_n_children (symbol_tree, symbol);

      child = g_object_new (IDE_TYPE_TREE_NODE,
                            "children-possible", has_children,
                            "text", name,
                            "icon-name", icon_name,
                            "item", symbol,
                            NULL);
      ide_tree_node_append (node, child);
    }
}

static void
symbol_tree_builder_get_location_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(SymbolTreeBuilder) self = user_data;
  g_autoptr(IdeSourceLocation) location = NULL;
  g_autoptr(GError) error = NULL;
  IdePerspective *editor;
  IdeWorkbench *workbench;
  IdeTree *tree;

  IDE_ENTRY;

  g_assert (SYMBOL_IS_TREE_BUILDER (self));

  location = ide_symbol_node_get_location_finish (node, result, &error);

  if (location == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      IDE_EXIT;
    }

  tree = ide_tree_builder_get_tree (IDE_TREE_BUILDER (self));
  workbench = ide_widget_get_workbench (GTK_WIDGET (tree));
  editor = ide_workbench_get_perspective_by_name (workbench, "editor");

  ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (editor), location);

  IDE_EXIT;
}

static gboolean
symbol_tree_builder_node_activated (IdeTreeBuilder *builder,
                                    IdeTreeNode    *node)
{
  SymbolTreeBuilder *self = (SymbolTreeBuilder *)builder;
  GObject *item;

  IDE_ENTRY;

  g_assert (SYMBOL_IS_TREE_BUILDER (self));

  item = ide_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    {
      g_autoptr(IdeSourceLocation) location = NULL;

      ide_symbol_node_get_location_async (IDE_SYMBOL_NODE (item),
                                          NULL,
                                          symbol_tree_builder_get_location_cb,
                                          g_object_ref (self));

      IDE_RETURN (TRUE);
    }

  g_warning ("IdeSymbolNode did not create a source location");

  IDE_RETURN (FALSE);
}

static void
symbol_tree_builder_class_init (SymbolTreeBuilderClass *klass)
{
  IdeTreeBuilderClass *builder_class = IDE_TREE_BUILDER_CLASS (klass);

  builder_class->build_node = symbol_tree_builder_build_node;
  builder_class->node_activated = symbol_tree_builder_node_activated;
}

static void
symbol_tree_builder_init (SymbolTreeBuilder *self)
{
}
