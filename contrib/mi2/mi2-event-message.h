/* mi2-event-mesage.h
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

#ifndef MI2_EVENT_MESAGE_H
#define MI2_EVENT_MESAGE_H

#include "mi2-message.h"

G_BEGIN_DECLS

#define MI2_TYPE_EVENT_MESSAGE (mi2_event_mesage_get_type())

G_DECLARE_FINAL_TYPE (Mi2EventMessage, mi2_event_mesage, MI2, EVENT_MESSAGE, Mi2Message)

Mi2Message  *mi2_event_message_new_from_string (const gchar     *line);
const gchar *mi2_event_message_get_name        (Mi2EventMessage *self);
void         mi2_event_message_set_name        (Mi2EventMessage *self,
                                                const gchar     *name);

G_END_DECLS

#endif /* MI2_EVENT_MESAGE_H */
