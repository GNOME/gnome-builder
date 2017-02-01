/* ide-xml-tree-builder-generic.h
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

#ifndef IDE_XML_TREE_BUILDER_GENERIC_H
#define IDE_XML_TREE_BUILDER_GENERIC_H

#include <glib.h>

#include "ide-xml-analysis.h"
#include "ide-xml-sax.h"
#include "ide-xml-symbol-node.h"
#include "ide-xml-tree-builder.h"
#include "xml-reader.h"

G_BEGIN_DECLS

IdeXmlAnalysis *ide_xml_tree_builder_generic_create (IdeXmlTreeBuilder *self,
                                                     IdeXmlSax         *parser,
                                                     GFile             *file,
                                                     const gchar       *data,
                                                     gsize              size);

G_END_DECLS

#endif /* IDE_XML_TREE_BUILDER_GENERIC_H */
