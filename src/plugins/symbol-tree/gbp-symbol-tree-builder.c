/* gbp-symbol-tree-builder.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gbp-symbol-tree-builder"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-gui.h>

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

static gboolean
page_contains_location (IdePage     *page,
                        IdeLocation *location)
{
  IdeBuffer *buffer;
  GFile *file;
  GFile *loc_file;

  if (location == NULL || !IDE_IS_EDITOR_PAGE (page))
    return FALSE;

  buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));
  file = ide_buffer_get_file (buffer);
  loc_file = ide_location_get_file (location);

  if (file == NULL || loc_file == NULL)
    return FALSE;

  return g_file_equal (file, loc_file);
}

static void
gbp_symbol_tree_builder_get_location_cb (GObject      *object,
                                         GAsyncResult *result,
                                         gpointer      user_data)
{
  IdeSymbolNode *node = (IdeSymbolNode *)object;
  g_autoptr(GbpSymbolTreeBuilder) self = user_data;
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GError) error = NULL;
  IdeWorkspace *workspace;
  IdeSurface *editor;
  IdeFrame *frame;
  IdePage *page;
  DzlTree *tree;
  gint line;
  gint line_offset;

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
  workspace = ide_widget_get_workspace (GTK_WIDGET (tree));
  editor = ide_workspace_get_surface_by_name (workspace, "editor");
  frame = IDE_FRAME (dzl_gtk_widget_get_relative (GTK_WIDGET (tree), IDE_TYPE_FRAME));
  page = ide_frame_get_visible_child (frame);
  line = ide_location_get_line (location);
  line_offset = ide_location_get_line_offset (location);

  /* Because we activated from within the document, we can ignore
   * using ide_editor_surface_focus_location() and instead just jump
   * to the resulting line and column.
   */
  if (page_contains_location (page, location))
    {
      if (line > 0 || line_offset > 0)
        ide_editor_page_scroll_to_line_offset (IDE_EDITOR_PAGE (page),
                                               MAX (0, line),
                                               MAX (0, line_offset));
      else
        gtk_widget_grab_focus (GTK_WIDGET (page));

      IDE_EXIT;
    }

  ide_editor_surface_focus_location (IDE_EDITOR_SURFACE (editor), location);

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

  g_clear_pointer (&self->filter, g_free);

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
