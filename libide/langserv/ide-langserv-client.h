/* ide-langserv-client.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_LANGSERV_CLIENT_H
#define IDE_LANGSERV_CLIENT_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_CLIENT (ide_langserv_client_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeLangservClient, ide_langserv_client, IDE, LANGSERV_CLIENT, IdeObject)

struct _IdeLangservClientClass
{
  IdeObjectClass parent_class;

  void     (*notification)          (IdeLangservClient *self,
                                     const gchar       *method,
                                     GVariant          *params);
  gboolean (*supports_language)     (IdeLangservClient *self,
                                     const gchar       *language_id);
  void     (*published_diagnostics) (IdeLangservClient *self,
                                     GFile             *file,
                                     IdeDiagnostics    *diagnostics);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeLangservClient *ide_langserv_client_new                      (IdeContext           *context,
                                                                 GIOStream            *io_stream);
void               ide_langserv_client_add_language             (IdeLangservClient    *self,
                                                                 const gchar          *language_id);
void               ide_langserv_client_start                    (IdeLangservClient    *self);
void               ide_langserv_client_stop                     (IdeLangservClient    *self);
void               ide_langserv_client_call_async               (IdeLangservClient    *self,
                                                                 const gchar          *method,
                                                                 GVariant             *params,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean           ide_langserv_client_call_finish              (IdeLangservClient    *self,
                                                                 GAsyncResult         *result,
                                                                 GVariant            **return_value,
                                                                 GError              **error);
void               ide_langserv_client_send_notification_async  (IdeLangservClient    *self,
                                                                 const gchar          *method,
                                                                 GVariant             *params,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   notificationback,
                                                                 gpointer              user_data);
gboolean           ide_langserv_client_send_notification_finish (IdeLangservClient    *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);
void               ide_langserv_client_get_diagnostics_async    (IdeLangservClient    *self,
                                                                 GFile                *file,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean           ide_langserv_client_get_diagnostics_finish   (IdeLangservClient    *self,
                                                                 GAsyncResult         *result,
                                                                 IdeDiagnostics      **diagnostics,
                                                                 GError              **error);

G_END_DECLS

#endif /* IDE_LANGSERV_CLIENT_H */
