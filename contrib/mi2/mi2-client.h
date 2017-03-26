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

#include "mi2-breakpoint.h"
#include "mi2-message.h"
#include "mi2-event-message.h"
#include "mi2-reply-message.h"

#define MI2_TYPE_CLIENT      (mi2_client_get_type())
#define MI2_TYPE_STOP_REASON (mi2_stop_reason_get_type())

G_DECLARE_DERIVABLE_TYPE (Mi2Client, mi2_client, MI2, CLIENT, GObject)

typedef enum
{
  MI2_STOP_UNKNOWN,
  MI2_STOP_EXITED_NORMALLY,
  MI2_STOP_BREAKPOINT_HIT,
} Mi2StopReason;

struct _Mi2ClientClass
{
  GObjectClass parent_instance;

  void (*log)                 (Mi2Client       *self,
                               const gchar     *log);
  void (*event)               (Mi2Client       *self,
                               Mi2EventMessage *message);
  void (*breakpoint_inserted) (Mi2Client       *client,
                               Mi2Breakpoint   *breakpoint);
  void (*breakpoint_removed)  (Mi2Client       *client,
                               gint             breakpoint_id);
  void (*stopped)             (Mi2Client       *self,
                               Mi2StopReason    reason,
                               Mi2Message      *message);

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

GType          mi2_stop_reason_get_type                   (void);
Mi2StopReason  mi2_stop_reason_parse                      (const gchar          *reason);
Mi2Client     *mi2_client_new                             (GIOStream            *stream);
void           mi2_client_exec_async                      (Mi2Client            *self,
                                                           const gchar          *command,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_exec_finish                     (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           Mi2ReplyMessage     **reply,
                                                           GError              **error);
void           mi2_client_listen_async                    (Mi2Client            *self,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_listen_finish                   (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
void           mi2_client_shutdown_async                  (Mi2Client            *self,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_shutdown_finish                 (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
void           mi2_client_continue_async                  (Mi2Client            *self,
                                                           gboolean              reverse,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_continue_finish                 (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
void           mi2_client_run_async                       (Mi2Client            *self,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_run_finish                      (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
void           mi2_client_insert_breakpoint_async         (Mi2Client            *self,
                                                           Mi2Breakpoint        *breakpoint,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gint           mi2_client_insert_breakpoint_finish        (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);
void           mi2_client_remove_breakpoint_async         (Mi2Client            *self,
                                                           gint                  breakpoint_id,
                                                           GCancellable         *cancellable,
                                                           GAsyncReadyCallback   callback,
                                                           gpointer              user_data);
gboolean       mi2_client_remove_breakpoint_finish        (Mi2Client            *self,
                                                           GAsyncResult         *result,
                                                           GError              **error);


G_END_DECLS

#endif /* MI2_CLIENT_H */
