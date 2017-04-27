/* ide-cursor.h
 *
 * Copyright (C) 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#ifndef __IDE_CURSOR_

#define __IDE_CURSOR_

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_CURSOR (ide_cursor_get_type())

G_DECLARE_FINAL_TYPE (IdeCursor, ide_cursor, IDE, CURSOR, GObject)

typedef enum
{
  IDE_CURSOR_COLUMN,
  IDE_CURSOR_SELECT,
  IDE_CURSOR_MATCH
} IdeCursorType;

void         ide_cursor_add_cursor            (IdeCursor *self,
                                               guint      type);
void         ide_cursor_remove_cursors        (IdeCursor *self);
void         ide_cursor_insert_text           (IdeCursor *self,
                                               gchar     *text,
                                               gint       len);
gboolean     ide_cursor_is_enabled            (IdeCursor *self);

G_END_DECLS

#endif
