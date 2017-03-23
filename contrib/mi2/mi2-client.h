/* mi2-client.h
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

#ifndef MI2_CLIENT_H
#define MI2_CLIENT_H

#include <gio/gio.h>

G_BEGIN_DECLS

#include "mi2-message.h"
#include "mi2-event-message.h"

#define MI2_TYPE_CLIENT (mi2_client_get_type())

G_DECLARE_DERIVABLE_TYPE (Mi2Client, mi2_client, MI2, CLIENT, GObject)

struct _Mi2ClientClass
{
  GObjectClass parent_instance;

  void (*log)   (Mi2Client       *self,
                 const gchar     *log);
  void (*event) (Mi2Client       *self,
                 Mi2EventMessage *message);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
  gpointer _reserved13;
  gpointer _reserved14;
  gpointer _reserved15;
  gpointer _reserved16;
};

Mi2Client *mi2_client_new             (GIOStream            *stream);
void       mi2_client_exec_async      (Mi2Client            *self,
                                       const gchar          *command,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
gboolean   mi2_client_exec_finish     (Mi2Client            *self,
                                       GAsyncResult         *result,
                                       GError              **error);
void       mi2_client_start_listening (Mi2Client            *self);
void       mi2_client_stop_listening  (Mi2Client            *self);

G_END_DECLS

#endif /* MI2_CLIENT_H */
