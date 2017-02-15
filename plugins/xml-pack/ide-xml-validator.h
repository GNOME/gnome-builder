/* ide-xml-validator.h
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

#ifndef IDE_XML_VALIDATOR_H
#define IDE_XML_VALIDATOR_H

#include <glib-object.h>
#include <ide.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

#define IDE_TYPE_XML_VALIDATOR (ide_xml_validator_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlValidator, ide_xml_validator, IDE, XML_VALIDATOR, IdeObject)

typedef enum
{
  SCHEMA_KIND_NONE,
  SCHEMA_KIND_DTD,
  SCHEMA_KIND_RNG,
  SCHEMA_KIND_XML_SCHEMA,
} SchemaKind;

typedef struct _SchemaEntry
{
  GFile      *schema_file;
  GBytes     *schema_content;
  gchar      *error_message;
  SchemaKind  schema_kind;
  gint32      schema_line;
  gint32      schema_col;
} SchemaEntry;

IdeXmlValidator       *ide_xml_validator_new        (IdeContext       *context);
SchemaKind             ide_xml_validator_get_kind   (IdeXmlValidator  *self);
gboolean               ide_xml_validator_set_schema (IdeXmlValidator  *self,
                                                     SchemaKind        kind,
                                                     const gchar      *data,
                                                     gsize             size);

gboolean               ide_xml_validator_validate   (IdeXmlValidator  *self,
                                                     xmlDoc           *doc,
                                                     IdeDiagnostics  **diagnostics);

G_END_DECLS

#endif /* IDE_XML_VALIDATOR_H */

