/* gb-document-split.h
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

#ifndef GB_DOCUMENT_SPLIT_H
#define GB_DOCUMENT_SPLIT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GB_TYPE_DOCUMENT_SPLIT (gb_document_split_get_type())

typedef enum {
  GB_DOCUMENT_SPLIT_NONE  = 0,
  GB_DOCUMENT_SPLIT_RIGHT = 1,
  GB_DOCUMENT_SPLIT_LEFT  = 2,
} GbDocumentSplit;

GType gb_document_split_get_type (void);

G_END_DECLS

#endif /* GB_DOCUMENT_SPLIT_H */
