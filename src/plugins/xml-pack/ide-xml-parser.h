/* ide-xml-parser.h
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

#include "ide-xml-analysis.h"
G_BEGIN_DECLS

#define IDE_TYPE_XML_PARSER (ide_xml_parser_get_type())

G_DECLARE_FINAL_TYPE (IdeXmlParser, ide_xml_parser, IDE, XML_PARSER, IdeObject)

IdeXmlParser      *ide_xml_parser_new                   (void);
void               ide_xml_parser_get_analysis_async    (IdeXmlParser         *self,
                                                         GFile                *file,
                                                         GBytes               *content,
                                                         gint64                sequence,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IdeXmlAnalysis    *ide_xml_parser_get_analysis_finish   (IdeXmlParser         *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

G_END_DECLS
