/* mi2-console-message.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef MI2_CONSOLE_MESSAGE_H
#define MI2_CONSOLE_MESSAGE_H

#include "mi2-message.h"

G_BEGIN_DECLS

#define MI2_TYPE_CONSOLE_MESSAGE (mi2_console_message_get_type())

G_DECLARE_FINAL_TYPE (Mi2ConsoleMessage, mi2_console_message, MI2, CONSOLE_MESSAGE, Mi2Message)

Mi2Message  *mi2_console_message_new_from_string (const gchar       *line);
const gchar *mi2_console_message_get_message     (Mi2ConsoleMessage *self);
void         mi2_console_message_set_message     (Mi2ConsoleMessage *self,
                                                  const gchar       *message);

G_END_DECLS

#endif /* MI2_CONSOLE_MESSAGE_H */
