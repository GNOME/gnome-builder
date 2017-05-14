/* ide-xml-position.h
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

#ifndef IDE_XML_POSITION_H
#define IDE_XML_POSITION_H

#include <glib-object.h>

#include "ide-xml-types.h"
#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_POSITION (ide_xml_position_get_type())

typedef struct _IdeXmlPosition IdeXmlPosition;

struct _IdeXmlPosition
{
  IdeXmlSymbolNode   *root_node;
  IdeXmlSymbolNode   *node;
  IdeXmlSymbolNode   *parent_node;
  IdeXmlPositionKind  kind;

  guint               ref_count;
};

IdeXmlPosition     *ide_xml_position_new   (void);
IdeXmlPosition     *ide_xml_position_copy  (IdeXmlPosition *self);
IdeXmlPosition     *ide_xml_position_ref   (IdeXmlPosition *self);
void                ide_xml_position_unref (IdeXmlPosition *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlPosition, ide_xml_position_unref)

G_END_DECLS

#endif /* IDE_XML_POSITION_H */
