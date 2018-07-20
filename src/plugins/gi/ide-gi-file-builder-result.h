/* ide-gi-file-builder-result.h
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

#include "radix-tree/ide-gi-radix-tree-builder.h"

G_BEGIN_DECLS

#define IDE_TYPE_GI_FILE_BUILDER_RESULT (ide_gi_file_builder_result_get_type())

typedef struct _IdeGiFileBuilderResult IdeGiFileBuilderResult;

struct _IdeGiFileBuilderResult
{
  volatile gint          ref_count;

  GByteArray            *ns_ba;
  IdeGiRadixTreeBuilder *ro_tree;
  GArray                *global_index;

  gchar                 *ns;
  gchar                 *symbol_prefixes;
  gchar                 *identifier_prefixes;

  gint                   major_version;
  gint                   minor_version;
};

IdeGiFileBuilderResult     *ide_gi_file_builder_result_new   (GByteArray              *ns_ba,
                                                              IdeGiRadixTreeBuilder   *ro_tree,
                                                              GArray                  *global_index,
                                                              const gchar             *ns,
                                                              const gchar             *symbol_prefixes,
                                                              const gchar             *identifier_prefixes);
IdeGiFileBuilderResult     *ide_gi_file_builder_result_ref   (IdeGiFileBuilderResult  *self);
void                        ide_gi_file_builder_result_unref (IdeGiFileBuilderResult  *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiFileBuilderResult, ide_gi_file_builder_result_unref)

G_END_DECLS
