/* mi2-message.h
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

#ifndef MI2_MESSAGE_H
#define MI2_MESSAGE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MI2_TYPE_MESSAGE (mi2_message_get_type())

G_DECLARE_DERIVABLE_TYPE (Mi2Message, mi2_message, MI2, MESSAGE, GObject)

struct _Mi2MessageClass
{
  GObjectClass parent_class;

  GBytes *(*serialize) (Mi2Message *self);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

Mi2Message   *mi2_message_parse            (const gchar  *line,
                                            gsize         len,
                                            GError      **error);
GBytes       *mi2_message_serialize        (Mi2Message   *self);
const gchar **mi2_message_get_params       (Mi2Message   *self);
GVariant     *mi2_message_get_param        (Mi2Message   *self,
                                            const gchar  *param);
void          mi2_message_set_param        (Mi2Message   *self,
                                            const gchar  *param,
                                            GVariant     *variant);
const gchar  *mi2_message_get_param_string (Mi2Message   *self,
                                            const gchar  *name);
void          mi2_message_set_param_string (Mi2Message   *self,
                                            const gchar  *name,
                                            const gchar  *value);

G_END_DECLS

#endif /* MI2_MESSAGE_H */
