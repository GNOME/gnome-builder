/* ide-gi-helper.h
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

#include <glib.h>
#include <string.h>

#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-parser-result.h"

G_BEGIN_DECLS

typedef enum {
  IDE_GI_MARKUP_COLLECT_INVALID,
  IDE_GI_MARKUP_COLLECT_BOOLEAN,
  IDE_GI_MARKUP_COLLECT_STABILITY,
  IDE_GI_MARKUP_COLLECT_SCOPE,
  IDE_GI_MARKUP_COLLECT_DIRECTION,
  IDE_GI_MARKUP_COLLECT_TRANSFER_OWNERSHIP,
  IDE_GI_MARKUP_COLLECT_SIGNAL_WHEN,
  IDE_GI_MARKUP_COLLECT_UINT64,
  IDE_GI_MARKUP_COLLECT_INT64,
  IDE_GI_MARKUP_COLLECT_STRING,
  IDE_GI_MARKUP_COLLECT_OFFSET32_STRING,
  IDE_GI_MARKUP_COLLECT_OFFSET32_DOC_STRING,
  IDE_GI_MARKUP_COLLECT_TRISTATE,

  /* keep it as the MSB for bitfield operations */
  IDE_GI_MARKUP_COLLECT_OPTIONAL = (1 << 16)
} IdeGiMarkupCollectType;

gboolean      ide_gi_helper_markup_collect_attributes (IdeGiParserResult        *result,
                                                       GMarkupParseContext      *context,
                                                       const gchar              *element_name,
                                                       const gchar             **attribute_names,
                                                       const gchar             **attribute_values,
                                                       GError                  **error,
                                                       IdeGiMarkupCollectType    first_type,
                                                       const gchar              *default_value,
                                                       const gchar              *first_attr,
                                                       ...);
void          ide_gi_helper_parsing_error_custom      (IdeGiParserObject        *parser_object,
                                                       GMarkupParseContext      *context,
                                                       GFile                    *file,
                                                       const gchar              *message);
void          ide_gi_helper_parsing_error             (IdeGiParserObject        *parser_object,
                                                       GMarkupParseContext      *context,
                                                       GFile                    *file);
void          ide_gi_helper_update_doc_blob           (IdeGiParserResult        *result,
                                                       IdeGiDocBlob             *blob,
                                                       IdeGiElementType          element_type,
                                                       const gchar              *str);

G_END_DECLS
