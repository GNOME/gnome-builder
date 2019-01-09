/* ide-xml-tree-builder.h
 *
 * Copyright 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#pragma once

#include <libide-code.h>

#include "ide-xml-analysis.h"
#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_TREE_BUILDER (ide_xml_tree_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlTreeBuilder, ide_xml_tree_builder, IDE, XML_TREE_BUILDER, IdeObject)

void                 ide_xml_tree_builder_build_tree_async       (IdeXmlTreeBuilder     *self,
                                                                  GFile                 *file,
                                                                  GCancellable          *cancellable,
                                                                  GAsyncReadyCallback    callback,
                                                                  gpointer               user_data);
IdeXmlAnalysis      *ide_xml_tree_builder_build_tree_finish      (IdeXmlTreeBuilder     *self,
                                                                  GAsyncResult          *result,
                                                                  GError               **error);

G_END_DECLS
