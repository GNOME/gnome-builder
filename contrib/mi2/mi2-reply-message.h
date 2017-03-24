/* mi2-reply-mesage.h
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

#ifndef MI2_REPLY_MESAGE_H
#define MI2_REPLY_MESAGE_H

#include "mi2-message.h"

G_BEGIN_DECLS

#define MI2_TYPE_REPLY_MESSAGE (mi2_reply_mesage_get_type())

G_DECLARE_FINAL_TYPE (Mi2ReplyMessage, mi2_reply_mesage, MI2, REPLY_MESSAGE, Mi2Message)

Mi2Message   *mi2_reply_message_new_from_string (const gchar      *line);
const gchar  *mi2_reply_message_get_name        (Mi2ReplyMessage  *self);
void          mi2_reply_message_set_name        (Mi2ReplyMessage  *self,
                                                 const gchar      *name);
gboolean      mi2_reply_message_check_error     (Mi2ReplyMessage  *self,
                                                 GError          **error);

G_END_DECLS

#endif /* MI2_REPLY_MESAGE_H */
