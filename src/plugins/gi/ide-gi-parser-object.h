
/* ide-gi-parser-object.h
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

#pragma once

#include <glib-object.h>

#include "ide-gi-types.h"

#include "ide-gi-parser.h"

G_BEGIN_DECLS

gpointer                ide_gi_parser_object_finish                     (IdeGiParserObject       *self);
void                    ide_gi_parser_object_index                      (IdeGiParserObject       *self,
                                                                         IdeGiParserResult       *result,
                                                                         gpointer                 user_data);
gboolean                ide_gi_parser_object_parse                      (IdeGiParserObject       *self,
                                                                         GMarkupParseContext     *context,
                                                                         IdeGiParserResult       *result,
                                                                         const gchar             *element_name,
                                                                         const gchar            **attribute_names,
                                                                         const gchar            **attribute_values,
                                                                         GError                 **error);
void                    ide_gi_parser_object_reset                      (IdeGiParserObject       *self);

IdeGiElementType        ide_gi_parser_object_get_element_type           (IdeGiParserObject       *self);
const gchar            *ide_gi_parser_object_get_element_type_string    (IdeGiParserObject       *self);
IdeGiParserResult      *ide_gi_parser_object_get_result                 (IdeGiParserObject       *self);
void                    ide_gi_parser_object_set_result                 (IdeGiParserObject       *self,
                                                                         IdeGiParserResult       *result);
void                    _ide_gi_parser_object_set_element_type          (IdeGiParserObject       *self,
                                                                         IdeGiElementType         type);

G_END_DECLS
