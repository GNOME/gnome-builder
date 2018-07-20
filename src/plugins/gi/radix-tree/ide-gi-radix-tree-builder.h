/* ide-gi-radix-tree-builder.h
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_GI_RADIX_TREE_BUILDER (ide_gi_radix_tree_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeGiRadixTreeBuilder, ide_gi_radix_tree_builder, IDE, GI_RADIX_TREE_BUILDER, GObject)

typedef struct _IdeGiRadixTreeNode IdeGiRadixTreeNode;

struct _IdeGiRadixTreeNode
{
  IdeGiRadixTreeNode *parent;
  gchar              *prefix;
  GPtrArray          *children;
  guint64            *payloads;
  guint               nb_payloads;
};

/* The payloads are malloc allocated so that you can free the original datas */

typedef struct
{
  gchar    *word;
  gpointer  payloads;
  guint     nb_payloads;
} IdeGiRadixTreeCompleteItem;

gboolean                    ide_gi_radix_tree_builder_add                    (IdeGiRadixTreeBuilder  *self,
                                                                              const gchar            *word,
                                                                              guint                   nb_payloads,
                                                                              gpointer                payloads);
GArray                     *ide_gi_radix_tree_builder_complete               (IdeGiRadixTreeBuilder  *self,
                                                                              const gchar            *word);
void                        ide_gi_radix_tree_builder_dump                   (IdeGiRadixTreeBuilder  *self);
void                        ide_gi_radix_tree_builder_dump_nodes             (IdeGiRadixTreeBuilder  *self);
gboolean                    ide_gi_radix_tree_builder_is_empty               (IdeGiRadixTreeBuilder  *self);
IdeGiRadixTreeNode         *ide_gi_radix_tree_builder_lookup                 (IdeGiRadixTreeBuilder  *self,
                                                                              const gchar            *word);
IdeGiRadixTreeBuilder      *ide_gi_radix_tree_builder_new                    (void);
gboolean                    ide_gi_radix_tree_builder_remove                 (IdeGiRadixTreeBuilder  *self,
                                                                              const gchar            *word);
GByteArray                 *ide_gi_radix_tree_builder_serialize              (IdeGiRadixTreeBuilder  *self);
gboolean                    _ide_gi_radix_tree_builder_set_root              (IdeGiRadixTreeBuilder  *self,
                                                                              IdeGiRadixTreeNode     *node);

IdeGiRadixTreeNode         *_ide_gi_radix_tree_builder_node_add              (IdeGiRadixTreeNode     *parent,
                                                                              const gchar            *prefix,
                                                                              gint                    prefix_size,
                                                                              guint                   nb_payloads,
                                                                              gpointer                payloads);
gboolean                    ide_gi_radix_tree_builder_node_append_payload    (IdeGiRadixTreeNode     *node,
                                                                              guint                   nb_payloads,
                                                                              gpointer                payloads);
gboolean                    ide_gi_radix_tree_builder_node_insert_payload    (IdeGiRadixTreeNode     *node,
                                                                              guint                   pos,
                                                                              guint                   nb_payloads,
                                                                              gpointer                payloads);
gboolean                    ide_gi_radix_tree_builder_node_prepend_payload   (IdeGiRadixTreeNode     *node,
                                                                              guint                   nb_payloads,
                                                                              gpointer                payloads);
gboolean                    ide_gi_radix_tree_builder_node_remove_payload    (IdeGiRadixTreeNode     *node,
                                                                              guint                   pos);

G_END_DECLS
