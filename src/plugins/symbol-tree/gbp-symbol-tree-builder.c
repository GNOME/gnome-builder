/* gbp-symbol-tree-builder.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-symbol-tree-builder"

#include <glib/gi18n.h>
#include <ide.h>

#include "gbp-symbol-tree-builder.h"

struct _GbpSymbolTreeBuilder
{
  DzlTreeBuilder parent_instance;
  gchar *filter;
};

G_DEFINE_TYPE (GbpSymbolTreeBuilder, gbp_symbol_tree_builder, DZL_TYPE_TREE_BUILDER)

static void
gbp_symbol_tree_builder_build_children (DzlTreeBuilder *builder,
                                        DzlTreeNode    *node)
{
  IdeSymbolNode *parent = NULL;
  IdeSymbolTree *symbol_tree;
  DzlTree *tree;
  DzlTreeNode *root;
  GObject *item;
  guint n_children;

  g_assert (DZL_IS_TREE_BUILDER (builder));
  g_assert (DZL_IS_TREE_NODE (node));

  if (!(tree = dzl_tree_builder_get_tree (builder)) ||
      !(root = dzl_tree_get_root (tree)) ||
      !(symbol_tree = IDE_SYMBOL_TREE (dzl_tree_node_get_item (root))))
    return;

  item = dzl_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    parent = IDE_SYMBOL_NODE (item);

  n_children = ide_symbol_tree_get_n_children (symbol_tree, parent);

  for (guint i = 0; i < n_children; i++)
    {
      g_autoptr(IdeSymbolNode) symbol = NULL;
      const gchar *name;
      const gchar *icon_name;
      DzlTreeNode *child;
      IdeSymbolKind kind;
      gboolean has_children;
      gboolean use_markup;

      symbol = ide_symbol_tree_get_nth_child (symbol_tree, parent, i);
      name = ide_symbol_node_get_name (symbol);
      kind = ide_symbol_node_get_kind (symbol);
      use_markup = ide_symbol_node_get_use_markup (symbol);
      icon_name = ide_symbol_kind_get_icon_name (kind);

      has_children = !!ide_symbol_tree_get_n_children (symbol_tree, symbol);

      child = g_object_new (DZL_TYPE_TREE_NODE,
                            "children-possible", has_children,
                            "text", name,
                            "use-markup", use_markup,
                            "icon-name", icon_name,
                            "item", symbol,
                            NULL);
      dzl_tree_node_append (node, child);
    }
}

static void
gbp_symbol_tree_builder_get_location_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(GbpSymbolTreeBuilder) self = user_data;
  g_autoptr(IdeSourceLocation) location = NULL;
  g_autoptr(GError) error = NULL;
  IdePerspective *editor;
  IdeWorkbench *workbench;
  DzlTree *tree;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_NODE (node));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_SYMBOL_TREE_BUILDER (self));

  location = ide_symbol_node_get_location_finish (node, result, &error);

  if (location == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      IDE_EXIT;
    }

  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  workbench = ide_widget_get_workbench (GTK_WIDGET (tree));
  editor = ide_workbench_get_perspective_by_name (workbench, "editor");

  ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (editor), location);

  IDE_EXIT;
}

static gboolean
gbp_symbol_tree_builder_node_activated (DzlTreeBuilder *builder,
                                        DzlTreeNode    *node)
{
  GbpSymbolTreeBuilder *self = (GbpSymbolTreeBuilder *)builder;
  GObject *item;

  IDE_ENTRY;

  g_assert (GBP_IS_SYMBOL_TREE_BUILDER (self));
  g_assert (!node || DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);

  if (IDE_IS_SYMBOL_NODE (item))
    {
      ide_symbol_node_get_location_async (IDE_SYMBOL_NODE (item),
                                          NULL,
                                          gbp_symbol_tree_builder_get_location_cb,
                                          g_object_ref (self));

      IDE_RETURN (TRUE);
    }

  g_warning ("Not a symbol node, ignoring request");

  IDE_RETURN (FALSE);
}

static void
gbp_symbol_tree_builder_cell_data_func (DzlTreeBuilder  *builder,
                                        DzlTreeNode     *node,
                                        GtkCellRenderer *cell)
{
  GbpSymbolTreeBuilder *self = (GbpSymbolTreeBuilder *)builder;
  g_autofree gchar *markup = NULL;
  const gchar *text = NULL;

  g_assert (GBP_IS_SYMBOL_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));
  g_assert (GTK_IS_CELL_RENDERER (cell));

  if (self->filter == NULL || !GTK_IS_CELL_RENDERER_TEXT (cell))
    return;

  text = dzl_tree_node_get_text (node);
  markup = ide_completion_fuzzy_highlight (text, self->filter);
  g_object_set (cell, "markup", markup, NULL);
}

static void
gbp_symbol_tree_builder_finalize (GObject *object)
{
  GbpSymbolTreeBuilder *self = (GbpSymbolTreeBuilder *)object;

  dzl_clear_pointer (&self->filter, g_free);

  G_OBJECT_CLASS (gbp_symbol_tree_builder_parent_class)->finalize (object);
}

static void
gbp_symbol_tree_builder_class_init (GbpSymbolTreeBuilderClass *klass)
{
  DzlTreeBuilderClass *builder_class = DZL_TREE_BUILDER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_symbol_tree_builder_finalize;

  builder_class->build_children = gbp_symbol_tree_builder_build_children;
  builder_class->node_activated = gbp_symbol_tree_builder_node_activated;
  builder_class->cell_data_func = gbp_symbol_tree_builder_cell_data_func;
}

static void
gbp_symbol_tree_builder_init (GbpSymbolTreeBuilder *self)
{
}

void
gbp_symbol_tree_builder_set_filter (GbpSymbolTreeBuilder *self,
                                    const gchar          *filter)
{
  g_return_if_fail (GBP_IS_SYMBOL_TREE_BUILDER (self));

  if (!dzl_str_equal0 (self->filter, filter))
    {
      g_free (self->filter);
      self->filter = g_strdup (filter);
    }
}
