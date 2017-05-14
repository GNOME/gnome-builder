/* ide-xml-rng-parser.h
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

#ifndef IDE_XML_RNG_PARSER_H
#define IDE_XML_RNG_PARSER_H

#include <glib.h>
#include <ide.h>

#include "ide-xml-schema.h"

G_BEGIN_DECLS

#define IDE_TYPE_XML_RNG_PARSER (ide_xml_rng_parser_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlRngParser, ide_xml_rng_parser, IDE, XML_RNG_PARSER, GObject)

IdeXmlRngParser       *ide_xml_rng_parser_new            (void);
IdeXmlSchema          *ide_xml_rng_parser_parse          (IdeXmlRngParser *self,
                                                          const gchar     *schema_data,
                                                          gsize            schema_size,
                                                          GFile           *file);

G_END_DECLS

#endif /* IDE_XML_RNG_PARSER_H */

