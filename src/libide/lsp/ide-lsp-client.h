/* ide-lsp-client.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_LSP_CLIENT (ide_lsp_client_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLspClient, ide_lsp_client, IDE, LSP_CLIENT, IdeObject)

typedef enum
{
  IDE_LSP_TRACE_OFF,
  IDE_LSP_TRACE_MESSAGES,
  IDE_LSP_TRACE_VERBOSE,
} IdeLspTrace;

struct _IdeLspClientClass
{
  IdeObjectClass parent_class;

  void      (*notification)          (IdeLspClient   *self,
                                      const gchar    *method,
                                      GVariant       *params);
  gboolean  (*supports_language)     (IdeLspClient   *self,
                                      const gchar    *language_id);
  void      (*published_diagnostics) (IdeLspClient   *self,
                                      GFile          *file,
                                      IdeDiagnostics *diagnostics);
  GVariant *(*load_configuration)    (IdeLspClient   *self);
  void      (*initialized)           (IdeLspClient   *self);

  /*< private >*/
  gpointer _reserved[15];
};

IDE_AVAILABLE_IN_ALL
IdeLspClient *ide_lsp_client_new                        (GIOStream            *io_stream);
IDE_AVAILABLE_IN_44
void          ide_lsp_client_set_name                   (IdeLspClient         *self,
                                                         const char           *name);
IDE_AVAILABLE_IN_ALL
IdeLspTrace   ide_lsp_client_get_trace                  (IdeLspClient         *self);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_set_trace                  (IdeLspClient         *self,
                                                         IdeLspTrace           trace);
IDE_AVAILABLE_IN_ALL
GVariant     *ide_lsp_client_get_server_capabilities    (IdeLspClient         *self);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_add_language               (IdeLspClient         *self,
                                                         const gchar          *language_id);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_set_root_uri               (IdeLspClient         *self,
                                                         const gchar          *root_uri);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_start                      (IdeLspClient         *self);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_stop                       (IdeLspClient         *self);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_call_async                 (IdeLspClient         *self,
                                                         const gchar          *method,
                                                         GVariant             *params,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_lsp_client_call_finish                (IdeLspClient         *self,
                                                         GAsyncResult         *result,
                                                         GVariant            **return_value,
                                                         GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_send_notification_async    (IdeLspClient         *self,
                                                         const gchar          *method,
                                                         GVariant             *params,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   notificationback,
                                                         gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_lsp_client_send_notification_finish   (IdeLspClient         *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_get_diagnostics_async      (IdeLspClient         *self,
                                                         GFile                *file,
                                                         GBytes               *content,
                                                         const gchar          *lang_id,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean      ide_lsp_client_get_diagnostics_finish     (IdeLspClient         *self,
                                                         GAsyncResult         *result,
                                                         IdeDiagnostics      **diagnostics,
                                                         GError              **error);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_client_set_initialization_options (IdeLspClient         *self,
                                                         GVariant             *options);
IDE_AVAILABLE_IN_ALL
GVariant     *ide_lsp_client_get_initialization_options (IdeLspClient         *self);

G_END_DECLS
