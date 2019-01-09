/* ide-xml-schema.h
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

#include <glib.h>
#include <glib-object.h>

#include "ide-xml-rng-grammar.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_SCHEMA (ide_xml_schema_get_type())

typedef struct _IdeXmlSchema IdeXmlSchema;

struct _IdeXmlSchema
{
  guint             ref_count;

  IdeXmlRngGrammar *top_grammar;
};

GType         ide_xml_schema_get_type (void);
IdeXmlSchema *ide_xml_schema_new      (void);
IdeXmlSchema *ide_xml_schema_copy     (IdeXmlSchema *self);
IdeXmlSchema *ide_xml_schema_ref      (IdeXmlSchema *self);
void          ide_xml_schema_unref    (IdeXmlSchema *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeXmlSchema, ide_xml_schema_unref)

G_END_DECLS
