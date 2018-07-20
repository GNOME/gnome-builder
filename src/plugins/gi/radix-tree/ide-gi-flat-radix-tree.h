/* ide-gi-flat-radix-tree.h
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

#include "ide-gi-radix-tree-builder.h"

G_BEGIN_DECLS

#define IDE_TYPE_GI_FLAT_RADIX_TREE (ide_gi_flat_radix_tree_get_type())

typedef struct _IdeGiFlatRadixTree IdeGiFlatRadixTree;

struct _IdeGiFlatRadixTree
{
  volatile gint  ref_count;

  guint64       *data;
  gsize          len;
};

/* For speed reasons, the payloads use a pointer to the original datas */
typedef struct
{
  gchar    *word;
  guint64  *payloads;
  guint     nb_payloads;
} IdeGiFlatRadixTreeCompleteItem;

typedef void (IdeGiFlatRadixTreeFilterFunc) (const gchar *word,
                                             guint64     *payloads,
                                             guint        nb_payloads,
                                             gpointer     user_data);

void                    ide_gi_flat_radix_tree_clear               (IdeGiFlatRadixTree            *self);
GArray                 *ide_gi_flat_radix_tree_complete            (IdeGiFlatRadixTree            *self,
                                                                    const gchar                   *word,
                                                                    gboolean                       get_prefix,
                                                                    gboolean                       case_sensitive);
void                    ide_gi_flat_radix_tree_complete_custom     (IdeGiFlatRadixTree            *self,
                                                                    const gchar                   *word,
                                                                    gboolean                       get_prefix,
                                                                    gboolean                       case_sensitive,
                                                                    IdeGiFlatRadixTreeFilterFunc   filter_func,
                                                                    gpointer                       user_data);
IdeGiRadixTreeBuilder  *ide_gi_flat_radix_tree_deserialize         (IdeGiFlatRadixTree            *self);
void                    ide_gi_flat_radix_tree_dump                (IdeGiFlatRadixTree            *self,
                                                                    IdeGiFlatRadixTreeFilterFunc  *func,
                                                                    gpointer                       user_data);
void                    ide_gi_flat_radix_tree_foreach             (IdeGiFlatRadixTree            *self,
                                                                    IdeGiFlatRadixTreeFilterFunc   filter_func,
                                                                    gpointer                       user_data);
void                    ide_gi_flat_radix_tree_init                (IdeGiFlatRadixTree            *self,
                                                                    guint64                       *data,
                                                                    gsize                          len);
gboolean                ide_gi_flat_radix_tree_lookup              (IdeGiFlatRadixTree            *self,
                                                                    const gchar                   *word,
                                                                    guint64                      **payloads,
                                                                    guint                         *nb_payloads);

IdeGiFlatRadixTree     *ide_gi_flat_radix_tree_new                 (void);
IdeGiFlatRadixTree     *ide_gi_flat_radix_tree_ref                 (IdeGiFlatRadixTree            *self);
void                    ide_gi_flat_radix_tree_unref               (IdeGiFlatRadixTree            *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiFlatRadixTree, ide_gi_flat_radix_tree_unref)

G_END_DECLS
