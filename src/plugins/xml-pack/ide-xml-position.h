/* ide-xml-position.h
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

#include <glib-object.h>

#include "ide-xml-types.h"
#include "ide-xml-analysis.h"
#include "ide-xml-symbol-node.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_POSITION (ide_xml_position_get_type())

typedef struct _IdeXmlPosition IdeXmlPosition;

struct _IdeXmlPosition
{
  IdeXmlAnalysis       *analysis;
  IdeXmlSymbolNode     *node;
  IdeXmlSymbolNode     *child_node;
  IdeXmlSymbolNode     *previous_sibling_node;
  IdeXmlSymbolNode     *next_sibling_node;
  gchar                *prefix;
  gchar                *detail_name;
  gchar                *detail_value;
  IdeXmlPositionKind    kind;
  IdeXmlPositionDetail  detail;
  gint                  child_pos;
  gchar                 quote;

  volatile gint         ref_count;
};

GType                     ide_xml_position_get_type             (void);
IdeXmlPosition           *ide_xml_position_new                  (IdeXmlSymbolNode      *node,
                                                                 const gchar           *prefix,
                                                                 IdeXmlPositionKind     kind,
                                                                 IdeXmlPositionDetail   detail,
                                                                 const gchar           *detail_name,
                                                                 const gchar           *detail_value,
                                                                 gchar                  quote);
IdeXmlPosition           *ide_xml_position_copy                 (IdeXmlPosition        *self);
IdeXmlPosition           *ide_xml_position_ref                  (IdeXmlPosition        *self);
void                      ide_xml_position_unref                (IdeXmlPosition        *self);
IdeXmlAnalysis           *ide_xml_position_get_analysis         (IdeXmlPosition        *self);
gint                      ide_xml_position_get_child_pos        (IdeXmlPosition        *self);
IdeXmlPositionDetail      ide_xml_position_get_detail           (IdeXmlPosition        *self);
const gchar              *ide_xml_position_get_detail_name      (IdeXmlPosition        *self);
const gchar              *ide_xml_position_get_detail_value     (IdeXmlPosition        *self);
IdeXmlPositionKind        ide_xml_position_get_kind             (IdeXmlPosition        *self);
IdeXmlSymbolNode         *ide_xml_position_get_next_sibling     (IdeXmlPosition        *self);
IdeXmlSymbolNode         *ide_xml_position_get_child_node       (IdeXmlPosition        *self);
IdeXmlSymbolNode         *ide_xml_position_get_node             (IdeXmlPosition        *self);
IdeXmlSymbolNode         *ide_xml_position_get_parent_node      (IdeXmlPosition        *self);
const gchar              *ide_xml_position_get_prefix           (IdeXmlPosition        *self);
IdeXmlSymbolNode         *ide_xml_position_get_previous_sibling (IdeXmlPosition        *self);
void                      ide_xml_position_set_analysis         (IdeXmlPosition        *self,
                                                                 IdeXmlAnalysis        *analysis);
void                      ide_xml_position_set_child_node       (IdeXmlPosition        *self,
                                                                 IdeXmlSymbolNode      *child_node);
void                      ide_xml_position_set_child_pos        (IdeXmlPosition        *self,
                                                                 gint                   child_pos);
void                      ide_xml_position_set_siblings         (IdeXmlPosition        *self,
                                                                 IdeXmlSymbolNode      *previous_sibling_node,
                                                                 IdeXmlSymbolNode      *next_sibling_node);

void                      ide_xml_position_print                (IdeXmlPosition        *self);
const gchar              *ide_xml_position_kind_get_str         (IdeXmlPositionKind     kind);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlPosition, ide_xml_position_unref)

G_END_DECLS
