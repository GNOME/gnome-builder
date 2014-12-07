/* gb-document-split.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "gb-document-split.h"

GType
gb_document_split_get_type (void)
{
  static GType type_id;

  const GEnumValue values[] = {
    { GB_DOCUMENT_SPLIT_NONE, "GB_DOCUMENT_SPLIT_NONE", "NONE" },
    { GB_DOCUMENT_SPLIT_LEFT, "GB_DOCUMENT_SPLIT_LEFT", "LEFT" },
    { GB_DOCUMENT_SPLIT_LEFT, "GB_DOCUMENT_SPLIT_RIGHT", "RIGHT" },
    { 0 }
  };

  if (!type_id)
    type_id = g_enum_register_static ("GbDocumentSplit", values);

  return type_id;
}
