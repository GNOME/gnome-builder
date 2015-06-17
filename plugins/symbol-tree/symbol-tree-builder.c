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
  GObject *item;

  g_assert (GB_IS_TREE_BUILDER (builder));
  g_assert (GB_IS_TREE_NODE (node));

  item = gb_tree_node_get_item (node);

  if (GB_IS_EDITOR_DOCUMENT (item))
    {
#if 0
      GbTreeNode *child;

      child = g_object_new (GB_TYPE_TREE_NODE,
                            "text", _("Symbols"),
                            "icon-name", "lang-class-symbolic",
                            NULL);
      gb_tree_node_append (node, child);
#endif
    }
}

static void
symbol_tree_builder_class_init (SymbolTreeBuilderClass *klass)
{
  GbTreeBuilderClass *builder_class = GB_TREE_BUILDER_CLASS (klass);

  builder_class->build_node = symbol_tree_builder_build_node;
}

static void
symbol_tree_builder_init (SymbolTreeBuilder *self)
{
}
