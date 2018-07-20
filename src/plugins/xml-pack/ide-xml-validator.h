/* ide-xml-validator.h
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
 */

#pragma once

#include <glib-object.h>
#include <libxml/parser.h>

#include <ide.h>
#include "ide-xml-schema-cache-entry.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_VALIDATOR (ide_xml_validator_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlValidator, ide_xml_validator, IDE, XML_VALIDATOR, IdeObject)

IdeXmlValidator  *ide_xml_validator_new        (IdeContext        *context);
IdeXmlSchemaKind  ide_xml_validator_get_kind   (IdeXmlValidator   *self);
gboolean          ide_xml_validator_set_schema (IdeXmlValidator   *self,
                                                IdeXmlSchemaKind   kind,
                                                const gchar       *data,
                                                gsize              size);
gboolean          ide_xml_validator_validate   (IdeXmlValidator   *self,
                                                xmlDoc            *doc,
                                                IdeDiagnostics   **diagnostics);

G_END_DECLS
