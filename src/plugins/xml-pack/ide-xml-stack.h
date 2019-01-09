/* ide-xml-stack.h
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

#include "ide-xml-symbol-node.h"

#include <glib.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_STACK (ide_xml_stack_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlStack, ide_xml_stack, IDE, XML_STACK, GObject)

IdeXmlStack           *ide_xml_stack_new           (void);

gsize                  ide_xml_stack_get_size      (IdeXmlStack       *self);
gboolean               ide_xml_stack_is_empty      (IdeXmlStack       *self);
IdeXmlSymbolNode      *ide_xml_stack_peek          (IdeXmlStack       *self,
                                                    const gchar      **name,
                                                    IdeXmlSymbolNode **parent,
                                                    gint              *depth);
IdeXmlSymbolNode      *ide_xml_stack_pop           (IdeXmlStack       *self,
                                                    gchar            **name,
                                                    IdeXmlSymbolNode **parent,
                                                    gint              *depth);
void                   ide_xml_stack_push          (IdeXmlStack       *self,
                                                    const gchar       *name,
                                                    IdeXmlSymbolNode  *node,
                                                    IdeXmlSymbolNode  *parent,
                                                    gint               depth);

G_END_DECLS
