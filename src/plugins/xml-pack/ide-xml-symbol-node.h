/* ide-xml-symbol-node.h
 *
 * Copyright 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#include "ide-xml-types.h"
#include "ide-xml-symbol-resolver.h"

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_SYMBOL_NODE (ide_xml_symbol_node_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlSymbolNode, ide_xml_symbol_node, IDE, XML_SYMBOL_NODE, IdeSymbolNode)

typedef enum
{
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_UNKNOW,
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_BEFORE,
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_AFTER,
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_START_TAG,
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_END_TAG,
  IDE_XML_SYMBOL_NODE_RELATIVE_POSITION_IN_CONTENT
} IdeXmlSymbolNodeRelativePosition;

typedef enum
{
  IDE_XML_SYMBOL_NODE_STATE_UNKNOW,
  IDE_XML_SYMBOL_NODE_STATE_OK,
  IDE_XML_SYMBOL_NODE_STATE_NOT_CLOSED
} IdeXmlSymbolNodeState;

IdeXmlSymbolNode                 *ide_xml_symbol_node_new                           (const gchar            *name,
                                                                                     const gchar            *value,
                                                                                     const gchar            *element_name,
                                                                                     IdeSymbolKind           kind);
IdeXmlSymbolNodeRelativePosition  ide_xml_symbol_node_compare_location              (IdeXmlSymbolNode       *ref_node,
                                                                                     gint                    line,
                                                                                     gint                    line_offset);
void                              ide_xml_symbol_node_take_child                    (IdeXmlSymbolNode       *self,
                                                                                     IdeXmlSymbolNode       *child);
void                              ide_xml_symbol_node_take_internal_child           (IdeXmlSymbolNode       *self,
                                                                                     IdeXmlSymbolNode       *child);
const gchar                      *ide_xml_symbol_node_get_element_name              (IdeXmlSymbolNode       *self);
GFile *                           ide_xml_symbol_node_get_location                  (IdeXmlSymbolNode       *self,
                                                                                     gint                   *start_line,
                                                                                     gint                   *start_line_offset,
                                                                                     gint                   *end_line,
                                                                                     gint                   *end_line_offset,
                                                                                     gsize                  *size);
void                              ide_xml_symbol_node_get_end_tag_location          (IdeXmlSymbolNode       *self,
                                                                                     gint                   *start_line,
                                                                                     gint                   *start_line_offset,
                                                                                     gint                   *end_line,
                                                                                     gint                   *end_line_offset,
                                                                                     gsize                  *size);
const gchar                      *ide_xml_symbol_node_get_namespace                 (IdeXmlSymbolNode       *self);
guint                             ide_xml_symbol_node_get_n_children                (IdeXmlSymbolNode       *self);
guint                             ide_xml_symbol_node_get_n_internal_children       (IdeXmlSymbolNode       *self);
IdeSymbolNode                    *ide_xml_symbol_node_get_nth_child                 (IdeXmlSymbolNode       *self,
                                                                                     guint                   nth_child);
IdeSymbolNode                    *ide_xml_symbol_node_get_nth_child_deep            (IdeXmlSymbolNode       *self,
                                                                                     guint                   nth_child,
                                                                                     guint                  *current_pos);
guint                             ide_xml_symbol_node_get_n_direct_children         (IdeXmlSymbolNode       *self);
IdeSymbolNode                    *ide_xml_symbol_node_get_nth_direct_child          (IdeXmlSymbolNode       *self,
                                                                                     guint                   nth_child);
IdeSymbolNode                    *ide_xml_symbol_node_get_nth_internal_child        (IdeXmlSymbolNode       *self,
                                                                                     guint                   nth_child);
IdeXmlSymbolNode                 *ide_xml_symbol_node_get_parent                    (IdeXmlSymbolNode       *self);
IdeXmlSymbolNodeState             ide_xml_symbol_node_get_state                     (IdeXmlSymbolNode       *self);
const gchar                      *ide_xml_symbol_node_get_value                     (IdeXmlSymbolNode       *self);
gboolean                          ide_xml_symbol_node_has_end_tag                   (IdeXmlSymbolNode       *self);
void                              ide_xml_symbol_node_print                         (IdeXmlSymbolNode       *self,
                                                                                     guint                   depth,
                                                                                     gboolean                recurse,
                                                                                     gboolean                show_value,
                                                                                     gboolean                show_attributes);
void                              ide_xml_symbol_node_set_location                  (IdeXmlSymbolNode       *self,
                                                                                     GFile                  *file,
                                                                                     gint                    start_line,
                                                                                     gint                    start_line_offset,
                                                                                     gint                    end_line,
                                                                                     gint                    end_line_offset,
                                                                                     gsize                   size);
void                              ide_xml_symbol_node_set_end_tag_location          (IdeXmlSymbolNode       *self,
                                                                                     gint                    start_line,
                                                                                     gint                    start_line_offset,
                                                                                     gint                    end_line,
                                                                                     gint                    end_line_offset,
                                                                                     gsize                   size);
void                              ide_xml_symbol_node_set_element_name              (IdeXmlSymbolNode       *self,
                                                                                     const gchar            *element_name);
void                              ide_xml_symbol_node_set_state                     (IdeXmlSymbolNode       *self,
                                                                                     IdeXmlSymbolNodeState   state);
void                              ide_xml_symbol_node_set_value                     (IdeXmlSymbolNode       *self,
                                                                                     const gchar            *value);
gchar                           **ide_xml_symbol_node_get_attributes_names          (IdeXmlSymbolNode       *self);
const gchar                      *ide_xml_symbol_node_get_attribute_value           (IdeXmlSymbolNode       *self,
                                                                                     const gchar            *name);
void                              ide_xml_symbol_node_set_attributes                (IdeXmlSymbolNode       *self,
                                                                                     const gchar           **attributes);

G_END_DECLS
