/* ide-xml-tree-builder.h
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#ifndef IDE_XML_TREE_BUILDER_H
#define IDE_XML_TREE_BUILDER_H

#include "ide-xml-analysis.h"
#include "ide-xml-symbol-node.h"

#include <glib-object.h>
#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_TREE_BUILDER (ide_xml_tree_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE, XML_TREE_BUILDER, IdeObject)

typedef enum _ColorTagId
{
  COLOR_TAG_LABEL,
  COLOR_TAG_ID,
  COLOR_TAG_STYLE_CLASS,
  COLOR_TAG_TYPE,
  COLOR_TAG_PARENT,
  COLOR_TAG_CLASS,
  COLOR_TAG_ATTRIBUTE,
} ColorTagId;

IdeXmlTreeBuilder   *ide_xml_tree_builder_new                    (void);
void                 ide_xml_tree_builder_build_tree_async       (IdeXmlTreeBuilder     *self,
                                                                  GFile                 *file,
                                                                  GCancellable          *cancellable,
                                                                  GAsyncReadyCallback    callback,
                                                                  gpointer               user_data);
IdeXmlAnalysis      *ide_xml_tree_builder_build_tree_finish      (IdeXmlTreeBuilder     *self,
                                                                  GAsyncResult          *result,
                                                                  GError               **error);
gchar               *ide_xml_tree_builder_get_color_tag          (IdeXmlTreeBuilder     *self,
                                                                  const gchar           *str,
                                                                  ColorTagId             id,
                                                                  gboolean               space_before,
                                                                  gboolean               space_after,
                                                                  gboolean               space_inside);

G_END_DECLS

#endif /* IDE_XML_TREE_BUILDER_H */

