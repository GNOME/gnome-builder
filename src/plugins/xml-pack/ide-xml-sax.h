/* ide-xml-sax.h
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

#include <libxml/parser.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_SAX (ide_xml_sax_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlSax, ide_xml_sax, IDE, XML_SAX, GObject)

typedef enum _IdeXmlSaxCallbackType IdeXmlSaxCallbackType;

enum _IdeXmlSaxCallbackType {
  IDE_XML_SAX_CALLBACK_TYPE_ATTRIBUTE,
  IDE_XML_SAX_CALLBACK_TYPE_CDATA,
  IDE_XML_SAX_CALLBACK_TYPE_CHAR,
  IDE_XML_SAX_CALLBACK_TYPE_COMMENT,
  IDE_XML_SAX_CALLBACK_TYPE_START_DOCUMENT,
  IDE_XML_SAX_CALLBACK_TYPE_START_ELEMENT,
  IDE_XML_SAX_CALLBACK_TYPE_END_DOCUMENT,
  IDE_XML_SAX_CALLBACK_TYPE_END_ELEMENT,
  IDE_XML_SAX_CALLBACK_TYPE_ENTITY,
  IDE_XML_SAX_CALLBACK_TYPE_INTERNAL_SUBSET,
  IDE_XML_SAX_CALLBACK_TYPE_EXTERNAL_SUBSET,
  IDE_XML_SAX_CALLBACK_TYPE_PROCESSING_INSTRUCTION,
  IDE_XML_SAX_CALLBACK_TYPE_WARNING,
  IDE_XML_SAX_CALLBACK_TYPE_ERROR,
  IDE_XML_SAX_CALLBACK_TYPE_FATAL_ERROR,
};

void            ide_xml_sax_clear               (IdeXmlSax              *self);
xmlParserCtxt  *ide_xml_sax_get_context         (IdeXmlSax              *self);
gint            ide_xml_sax_get_depth           (IdeXmlSax              *self);
gboolean        ide_xml_sax_get_location        (IdeXmlSax              *self,
                                                 gint                   *start_line,
                                                 gint                   *start_line_offset,
                                                 gint                   *end_line,
                                                 gint                   *end_line_offset,
                                                 const gchar           **content,
                                                 gsize                  *size);
IdeXmlSax      *ide_xml_sax_new                 (void);
gboolean        ide_xml_sax_parse               (IdeXmlSax              *self,
                                                 const gchar            *data,
                                                 gsize                   length,
                                                 const gchar            *uri,
                                                 gpointer                user_data);
void            ide_xml_sax_set_callback        (IdeXmlSax              *self,
                                                 IdeXmlSaxCallbackType   callback_type,
                                                 gpointer                callback);

G_END_DECLS
