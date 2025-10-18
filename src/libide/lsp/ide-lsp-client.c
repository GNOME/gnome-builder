/* ide-lsp-client.c
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

#define G_LOG_DOMAIN "ide-lsp-client"

#include "config.h"

#include <unistd.h>

#include <glib/gi18n.h>
#include <jsonrpc-glib.h>

#include <libide-code.h>
#include <libide-projects.h>
#include <libide-threading.h>

#include "ide-marshal.h"

#include "ide-lsp-client.h"
#include "ide-lsp-diagnostic.h"
#include "ide-lsp-enums.h"
#include "ide-lsp-workspace-edit.h"

typedef struct
{
  JsonrpcClient *client;
  GVariant      *id;
} AsyncCall;

typedef struct
{
  GList         link;
  IdeTask      *task;
  gchar        *method;
  GVariant     *params;
  GCancellable *cancellable;
} PendingMessage;

typedef struct
{
  GWeakRef buffer_wr;
  GWeakRef client_wr;
  guint    commit_notify;
} NotifyBridge;

typedef struct
{
  GSignalGroup   *buffer_manager_signals;
  GSignalGroup   *project_signals;
  JsonrpcClient  *rpc_client;
  GIOStream      *io_stream;
  GHashTable     *diagnostics_by_file;
  GPtrArray      *languages;
  GVariant       *server_capabilities;
  GVariant       *initialization_options;
  char           *root_uri;
  char           *name;
  IdeLspTrace     trace;
  gboolean        initialized;
  GQueue          pending_messages;
  guint           use_markdown_in_diagnostics : 1;
  guint           text_document_sync : 2;
} IdeLspClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLspClient, ide_lsp_client, IDE_TYPE_OBJECT)

enum {
  FILE_CHANGE_TYPE_CREATED = 1,
  FILE_CHANGE_TYPE_CHANGED = 2,
  FILE_CHANGE_TYPE_DELETED = 3,
};

enum {
  SEVERITY_ERROR       = 1,
  SEVERITY_WARNING     = 2,
  SEVERITY_INFORMATION = 3,
  SEVERITY_HINT        = 4,
};

enum {
  TAG_UNNECESSARY      = 1,
  TAG_DEPRECATED       = 2,
};

enum {
  TEXT_DOCUMENT_SYNC_NONE,
  TEXT_DOCUMENT_SYNC_FULL,
  TEXT_DOCUMENT_SYNC_INCREMENTAL,
};

enum {
  PROP_0,
  PROP_INITIALIZATION_OPTIONS,
  PROP_IO_STREAM,
  PROP_NAME,
  PROP_ROOT_URI,
  PROP_SERVER_CAPABILITIES,
  PROP_TRACE,
  PROP_USE_MARKDOWN_IN_DIAGNOSTICS,
  N_PROPS
};

enum {
  INITIALIZED,
  LOAD_CONFIGURATION,
  NOTIFICATION,
  PUBLISHED_DIAGNOSTICS,
  SUPPORTS_LANGUAGE,
  N_SIGNALS
};

static void ide_lsp_client_call_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data);

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
notify_bridge_commit_notify (GtkTextBuffer            *buffer,
                             GtkTextBufferNotifyFlags  flags,
                             guint                     position,
                             guint                     length,
                             gpointer                  user_data)
{
  IdeLspClientPrivate *priv;
  NotifyBridge *bridge = user_data;
  g_autoptr(IdeLspClient) client = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));
  g_assert (bridge != NULL);

  IDE_TRACE_MSG ("Flags = 0x%x", flags);

  if (!(client = g_weak_ref_get (&bridge->client_wr)))
    IDE_EXIT;

  priv = ide_lsp_client_get_instance_private (client);

  if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_INSERT)
    {
      g_assert_not_reached ();
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT)
    {
      if (priv->text_document_sync == TEXT_DOCUMENT_SYNC_INCREMENTAL)
        {
          g_autoptr(GVariant) params = NULL;
          g_autofree gchar *uri = NULL;
          g_autofree gchar *copy = NULL;
          GtkTextIter begin;
          GtkTextIter end;
          gint64 version;
          guint line;
          guint column;

          gtk_text_buffer_get_iter_at_offset (buffer, &begin, position);
          gtk_text_buffer_get_iter_at_offset (buffer, &end, position + length);

          uri = ide_buffer_dup_uri (IDE_BUFFER (buffer));
          copy = gtk_text_iter_get_slice (&begin, &end);

          version = (gint64)ide_buffer_get_change_count (IDE_BUFFER (buffer)) + 1;

          line = gtk_text_iter_get_line (&begin);
          column = gtk_text_iter_get_line_offset (&begin);

          params = JSONRPC_MESSAGE_NEW (
            "textDocument", "{",
              "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
              "version", JSONRPC_MESSAGE_PUT_INT64 (version),
            "}",
            "contentChanges", "[",
              "{",
                "range", "{",
                  "start", "{",
                    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
                    "character", JSONRPC_MESSAGE_PUT_INT64 (column),
                  "}",
                  "end", "{",
                    "line", JSONRPC_MESSAGE_PUT_INT64 (line),
                    "character", JSONRPC_MESSAGE_PUT_INT64 (column),
                  "}",
                "}",
                "rangeLength", JSONRPC_MESSAGE_PUT_INT64 (0),
                "text", JSONRPC_MESSAGE_PUT_STRING (copy),
              "}",
            "]");

          ide_lsp_client_send_notification_async (client,
                                                  "textDocument/didChange",
                                                  params,
                                                  NULL, NULL, NULL);
        }
      else if (priv->text_document_sync == TEXT_DOCUMENT_SYNC_FULL)
        {
          g_autoptr(GBytes) content = NULL;
          g_autoptr(GVariant) params = NULL;
          g_autofree gchar *uri = NULL;
          const char *text;
          gint64 version;

          uri = ide_buffer_dup_uri (IDE_BUFFER (buffer));
          version = (gint64)ide_buffer_get_change_count (IDE_BUFFER (buffer));

          content = ide_buffer_dup_content (IDE_BUFFER (buffer));
          text = (const char *)g_bytes_get_data (content, NULL);

          params = JSONRPC_MESSAGE_NEW (
            "textDocument", "{",
              "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
              "version", JSONRPC_MESSAGE_PUT_INT64 (version),
            "}",
            "contentChanges", "[",
              "{",
                "text", JSONRPC_MESSAGE_PUT_STRING (text),
              "}",
            "]");

          ide_lsp_client_send_notification_async (client,
                                                  "textDocument/didChange",
                                                  params,
                                                  NULL, NULL, NULL);
        }
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE)
    {
      if (priv->text_document_sync == TEXT_DOCUMENT_SYNC_INCREMENTAL)
        {
          g_autoptr(GVariant) params = NULL;
          g_autofree gchar *uri = NULL;
          GtkTextIter copy_begin;
          GtkTextIter copy_end;
          struct {
            int line;
            int column;
          } begin, end;
          gint64 version;

          uri = ide_buffer_dup_uri (IDE_BUFFER (buffer));

          /* We get called before this change is registered */
          version = (gint)ide_buffer_get_change_count (IDE_BUFFER (buffer)) + 1;

          gtk_text_buffer_get_iter_at_offset (buffer, &copy_begin, position);
          gtk_text_buffer_get_iter_at_offset (buffer, &copy_end, position + length);

          begin.line = gtk_text_iter_get_line (&copy_begin);
          begin.column = gtk_text_iter_get_line_offset (&copy_begin);

          end.line = gtk_text_iter_get_line (&copy_end);
          end.column = gtk_text_iter_get_line_offset (&copy_end);

          params = JSONRPC_MESSAGE_NEW (
            "textDocument", "{",
              "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
              "version", JSONRPC_MESSAGE_PUT_INT64 (version),
            "}",
            "contentChanges", "[",
              "{",
                "range", "{",
                  "start", "{",
                    "line", JSONRPC_MESSAGE_PUT_INT64 (begin.line),
                    "character", JSONRPC_MESSAGE_PUT_INT64 (begin.column),
                  "}",
                  "end", "{",
                    "line", JSONRPC_MESSAGE_PUT_INT64 (end.line),
                    "character", JSONRPC_MESSAGE_PUT_INT64 (end.column),
                  "}",
                "}",
                "rangeLength", JSONRPC_MESSAGE_PUT_INT64 (length),
                "text", JSONRPC_MESSAGE_PUT_STRING (""),
              "}",
            "]");

          ide_lsp_client_send_notification_async (client,
                                                  "textDocument/didChange",
                                                  params,
                                                  NULL, NULL, NULL);
        }
    }
  else if (flags == GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE)
    {
      if (priv->text_document_sync == TEXT_DOCUMENT_SYNC_FULL)
        {
          g_autofree char *uri = NULL;
          g_autoptr(GBytes) content = NULL;
          g_autoptr(GVariant) params = NULL;
          const char *text;
          gint64 version;

          uri = ide_buffer_dup_uri (IDE_BUFFER (buffer));
          version = (gint)ide_buffer_get_change_count (IDE_BUFFER (buffer));

          content = ide_buffer_dup_content (IDE_BUFFER (buffer));
          text = (const gchar *)g_bytes_get_data (content, NULL);

          params = JSONRPC_MESSAGE_NEW (
            "textDocument", "{",
              "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
              "version", JSONRPC_MESSAGE_PUT_INT64 (version),
            "}",
            "contentChanges", "[",
              "{",
                "text", JSONRPC_MESSAGE_PUT_STRING (text),
              "}",
            "]");

          ide_lsp_client_send_notification_async (client,
                                                  "textDocument/didChange",
                                                  params,
                                                  NULL, NULL, NULL);
        }
    }

  IDE_EXIT;
}

static void
notify_bridge_client_released (gpointer  data,
                               GObject  *where_object_was)
{
  NotifyBridge *bridge = data;
  g_autoptr(GtkTextBuffer) buffer = NULL;

  if ((buffer = g_weak_ref_get (&bridge->buffer_wr)))
    {
      guint notify = bridge->commit_notify;

      bridge->commit_notify = 0;

      if (notify)
        {
          /* This can invalidate/free @bridge */
          gtk_text_buffer_remove_commit_notify (buffer, notify);
        }
    }
}

static void
notify_bridge_destroy (gpointer data)
{
  NotifyBridge *bridge = data;

  /* Ignore registration, this can only be called when it becomes invalid
   * from disposal of the buffer or notify_bridge_client_released().
   */
  bridge->commit_notify = 0;

  g_weak_ref_clear (&bridge->buffer_wr);
  g_weak_ref_clear (&bridge->client_wr);

  g_free (bridge);
}

static void
notify_bridge (IdeLspClient *self,
               IdeBuffer    *buffer)
{
  NotifyBridge *bridge;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  bridge = g_new0 (NotifyBridge, 1);
  g_weak_ref_init (&bridge->client_wr, self);
  g_weak_ref_init (&bridge->buffer_wr, buffer);
  bridge->commit_notify = gtk_text_buffer_add_commit_notify (GTK_TEXT_BUFFER (buffer),
                                                             (GTK_TEXT_BUFFER_NOTIFY_AFTER_INSERT |
                                                              GTK_TEXT_BUFFER_NOTIFY_BEFORE_DELETE |
                                                              GTK_TEXT_BUFFER_NOTIFY_AFTER_DELETE),
                                                             notify_bridge_commit_notify,
                                                             bridge, notify_bridge_destroy);
  g_object_weak_ref (G_OBJECT (self),
                     notify_bridge_client_released,
                     bridge);
}

static AsyncCall *
async_call_new (JsonrpcClient *client,
                GVariant      *id)
{
  AsyncCall *ac = g_atomic_rc_box_new0 (AsyncCall);
  ac->client = g_object_ref (client);
  ac->id = g_variant_ref (id);
  return ac;
}

static void
async_call_finalize (gpointer data)
{
  AsyncCall *ac = data;
  g_clear_object (&ac->client);
  g_clear_pointer (&ac->id, g_variant_unref);
}

static void
async_call_unref (gpointer data)
{
  g_atomic_rc_box_release_full (data, async_call_finalize);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (AsyncCall, async_call_unref);

static void
pending_message_fail (PendingMessage *message)
{
  g_assert (message != NULL);
  g_assert (message->link.prev == NULL);
  g_assert (message->link.next == NULL);
  g_assert (message->link.data == message);
  g_assert (IDE_IS_TASK (message->task));
  g_assert (!message->cancellable || G_IS_CANCELLABLE (message->cancellable));
  g_assert (message->method != NULL);

  ide_task_return_new_error (message->task,
                             G_IO_ERROR,
                             G_IO_ERROR_CANCELLED,
                             _("The operation has been cancelled"));

  g_clear_object (&message->task);
  g_clear_object (&message->cancellable);
  g_clear_pointer (&message->method, g_free);
  g_clear_pointer (&message->params, g_variant_unref);
  g_slice_free (PendingMessage, message);
}

static void
pending_message_submit (PendingMessage *message,
                        JsonrpcClient  *rpc_client)
{
  g_assert (JSONRPC_IS_CLIENT (rpc_client));
  g_assert (message != NULL);
  g_assert (message->link.prev == NULL);
  g_assert (message->link.next == NULL);
  g_assert (message->link.data == message);
  g_assert (IDE_IS_TASK (message->task));
  g_assert (!message->cancellable || G_IS_CANCELLABLE (message->cancellable));
  g_assert (message->method != NULL);

  jsonrpc_client_call_async (rpc_client,
                             message->method,
                             message->params,
                             message->cancellable,
                             ide_lsp_client_call_cb,
                             g_steal_pointer (&message->task));

  g_clear_object (&message->cancellable);
  g_clear_pointer (&message->method, g_free);
  g_clear_pointer (&message->params, g_variant_unref);
  g_slice_free (PendingMessage, message);
}

static gboolean
ide_lsp_client_supports_buffer (IdeLspClient *self,
                                IdeBuffer    *buffer)
{
  GtkSourceLanguage *language;
  const gchar *language_id = "text/plain";
  gboolean ret = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (language != NULL)
    language_id = gtk_source_language_get_id (language);

  g_signal_emit (self, signals [SUPPORTS_LANGUAGE], 0, language_id, &ret);

  return ret;
}

static void
ide_lsp_client_clear_diagnostics (IdeLspClient *self,
                                  const gchar  *uri)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (uri != NULL);

  IDE_TRACE_MSG ("Clearing diagnostics for %s", uri);

  file = g_file_new_for_uri (uri);
  g_hash_table_remove (priv->diagnostics_by_file, file);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_saved (IdeLspClient     *self,
                             IdeBuffer        *buffer,
                             IdeBufferManager *buffer_manager)
{
  g_autoptr(GVariant) params = NULL;
  g_autoptr(GBytes) content = NULL;
  g_autofree gchar *uri = NULL;
  const gchar *text;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_lsp_client_supports_buffer (self, buffer))
    IDE_EXIT;

  uri = ide_buffer_dup_uri (buffer);
  content = ide_buffer_dup_content (buffer);
  text = (const gchar *)g_bytes_get_data (content, NULL);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "text", JSONRPC_MESSAGE_PUT_STRING (text),
    "}"
  );

  ide_lsp_client_send_notification_async (self,
                                          "textDocument/didSave",
                                          params,
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_loaded (IdeLspClient     *self,
                              IdeBuffer        *buffer,
                              IdeBufferManager *buffer_manager)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *text = NULL;
  GtkSourceLanguage *language;
  const gchar *language_id;
  GtkTextIter begin;
  GtkTextIter end;
  gint64 version;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_lsp_client_supports_buffer (self, buffer))
    IDE_EXIT;

  notify_bridge (self, buffer);

  uri = ide_buffer_dup_uri (buffer);
  version = (gint64)ide_buffer_get_change_count (buffer);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (language != NULL)
    language_id = gtk_source_language_get_id (language);
  else
    language_id = "text/plain";

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "languageId", JSONRPC_MESSAGE_PUT_STRING (language_id),
      "text", JSONRPC_MESSAGE_PUT_STRING (text),
      "version", JSONRPC_MESSAGE_PUT_INT64 (version),
    "}"
  );

  ide_lsp_client_send_notification_async (self,
                                          "textDocument/didOpen",
                                          params,
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_unloaded (IdeLspClient     *self,
                                IdeBuffer        *buffer,
                                IdeBufferManager *buffer_manager)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_lsp_client_supports_buffer (self, buffer))
    IDE_EXIT;

  uri = ide_buffer_dup_uri (buffer);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
    "}"
  );

  ide_lsp_client_send_notification_async (self,
                                          "textDocument/didClose",
                                          params,
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_manager_bind (IdeLspClient     *self,
                                    IdeBufferManager *buffer_manager,
                                    GSignalGroup     *signal_group)
{
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_lsp_client_buffer_loaded (self, buffer, buffer_manager);
    }
}

static void
ide_lsp_client_buffer_manager_unbind (IdeLspClient   *self,
                                      GSignalGroup   *signal_group)
{
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (G_IS_SIGNAL_GROUP (signal_group));

  /* TODO: We need to track everything we've notified so that we
   *       can notify the peer to release its resources.
   */
}

static void
ide_lsp_client_project_file_trashed (IdeLspClient *self,
                                     GFile        *file,
                                     IdeProject   *project)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PROJECT (project));

  uri = g_file_get_uri (file);

  params = JSONRPC_MESSAGE_NEW (
    "changes", "[",
      "{",
        "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
        "type", JSONRPC_MESSAGE_PUT_INT64 (FILE_CHANGE_TYPE_DELETED),
      "}",
    "]"
  );

  ide_lsp_client_send_notification_async (self, "workspace/didChangeWatchedFiles",
                                               params, NULL, NULL, NULL);

  ide_lsp_client_clear_diagnostics (self, uri);

  IDE_EXIT;
}

static void
ide_lsp_client_project_file_renamed (IdeLspClient *self,
                                     GFile        *src,
                                     GFile        *dst,
                                     IdeProject   *project)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *src_uri = NULL;
  g_autofree gchar *dst_uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (G_IS_FILE (src));
  g_assert (G_IS_FILE (dst));
  g_assert (IDE_IS_PROJECT (project));

  src_uri = g_file_get_uri (src);
  dst_uri = g_file_get_uri (dst);

  params = JSONRPC_MESSAGE_NEW (
    "changes", "["
      "{",
        "uri", JSONRPC_MESSAGE_PUT_STRING (src_uri),
        "type", JSONRPC_MESSAGE_PUT_INT64 (FILE_CHANGE_TYPE_DELETED),
      "}",
      "{",
        "uri", JSONRPC_MESSAGE_PUT_STRING (dst_uri),
        "type", JSONRPC_MESSAGE_PUT_INT64 (FILE_CHANGE_TYPE_CREATED),
      "}",
    "]"
  );

  ide_lsp_client_send_notification_async (self, "workspace/didChangeWatchedFiles",
                                               params, NULL, NULL, NULL);

  ide_lsp_client_clear_diagnostics (self, src_uri);

  IDE_EXIT;
}

static IdeDiagnostics *
ide_lsp_client_translate_diagnostics (IdeLspClient *self,
                                      GFile        *file,
                                      GVariantIter *diagnostics)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GPtrArray) ar = NULL;
  g_autoptr(IdeDiagnostics) ret = NULL;
  GVariant *value;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (diagnostics != NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);

  while (g_variant_iter_loop (diagnostics, "v", &value))
    {
      g_autoptr(IdeLocation) begin_loc = NULL;
      g_autoptr(IdeLocation) end_loc = NULL;
      g_autoptr(IdeLspDiagnostic) diag = NULL;
      g_autoptr(GVariant) range = NULL;
      const gchar *message = NULL;
      const gchar *source = NULL;
      g_autoptr(GVariantIter) tags = NULL;
      GVariant *current_tag;
      g_autoptr(GVariantIter) related_information = NULL;
      GVariant *current_related;
      gint64 severity = 0;
      gboolean success;
      struct {
        gint64 line;
        gint64 column;
      } begin, end;

      /* Mandatory fields */
      if (!JSONRPC_MESSAGE_PARSE (value,
                                  "range", JSONRPC_MESSAGE_GET_VARIANT (&range),
                                  "message", JSONRPC_MESSAGE_GET_STRING (&message)))
        continue;

      /* Optional Fields */
      JSONRPC_MESSAGE_PARSE (value, "severity", JSONRPC_MESSAGE_GET_INT64 (&severity));
      JSONRPC_MESSAGE_PARSE (value, "source", JSONRPC_MESSAGE_GET_STRING (&source));
      JSONRPC_MESSAGE_PARSE (value, "tags", JSONRPC_MESSAGE_GET_ITER (&tags));
      JSONRPC_MESSAGE_PARSE (value, "relatedInformation", JSONRPC_MESSAGE_GET_ITER (&related_information));

      /* Extract location information */
      success = JSONRPC_MESSAGE_PARSE (range,
        "start", "{",
          "line", JSONRPC_MESSAGE_GET_INT64 (&begin.line),
          "character", JSONRPC_MESSAGE_GET_INT64 (&begin.column),
        "}",
        "end", "{",
          "line", JSONRPC_MESSAGE_GET_INT64 (&end.line),
          "character", JSONRPC_MESSAGE_GET_INT64 (&end.column),
        "}"
      );

      if (!success)
        continue;

      begin_loc = ide_location_new (file, begin.line, begin.column);
      end_loc = ide_location_new (file, end.line, end.column);

      switch (severity)
        {
        case SEVERITY_ERROR:
          severity = IDE_DIAGNOSTIC_ERROR;
          break;

        case SEVERITY_WARNING:
          severity = IDE_DIAGNOSTIC_WARNING;
          break;

        case SEVERITY_INFORMATION:
        case SEVERITY_HINT:
        default:
          severity = IDE_DIAGNOSTIC_NOTE;
          break;
        }

      while (tags != NULL && g_variant_iter_loop (tags, "v", &current_tag))
        {
          if (!g_variant_is_of_type (current_tag, G_VARIANT_TYPE_INT64))
            continue;

          switch (g_variant_get_int64 (current_tag))
            {
            case TAG_DEPRECATED:
              severity = IDE_DIAGNOSTIC_DEPRECATED;
              break;
            case TAG_UNNECESSARY:
              severity = IDE_DIAGNOSTIC_UNUSED;
              break;
            default:
              break;
            }
        }

      diag = ide_lsp_diagnostic_new (severity, message, begin_loc, value);
      if (priv->use_markdown_in_diagnostics)
        ide_diagnostic_set_marked_kind (IDE_DIAGNOSTIC (diag), IDE_MARKED_KIND_MARKDOWN);
      ide_diagnostic_take_range (IDE_DIAGNOSTIC (diag), ide_range_new (begin_loc, end_loc));

      g_ptr_array_add (ar, g_steal_pointer (&diag));

      while (related_information != NULL && g_variant_iter_loop (related_information, "v", &current_related))
        {
          g_autoptr(IdeLocation) related_begin_loc = NULL;
          g_autoptr(IdeLocation) related_end_loc = NULL;
          g_autoptr(GFile) related_file = NULL;
          const char *uri = NULL;

          success = JSONRPC_MESSAGE_PARSE (current_related, "message", JSONRPC_MESSAGE_GET_STRING (&message));
          if (success)
            {
              success = JSONRPC_MESSAGE_PARSE (current_related,
                "location", "{",
                  "uri", JSONRPC_MESSAGE_GET_STRING (&uri),
                  "range", "{",
                    "start", "{",
                      "line", JSONRPC_MESSAGE_GET_INT64 (&begin.line),
                      "character", JSONRPC_MESSAGE_GET_INT64 (&begin.column),
                    "}",
                    "end", "{",
                      "line", JSONRPC_MESSAGE_GET_INT64 (&end.line),
                      "character", JSONRPC_MESSAGE_GET_INT64 (&end.column),
                    "}",
                  "}",
                "}"
              );
            }

          if (!success)
            continue;

          related_file = g_file_new_for_uri (uri);
          related_begin_loc = ide_location_new (related_file, begin.line, begin.column);
          related_end_loc = ide_location_new (related_file, end.line, end.column);

          diag = ide_lsp_diagnostic_new (IDE_DIAGNOSTIC_NOTE, message, related_begin_loc, NULL);
          if (priv->use_markdown_in_diagnostics)
            ide_diagnostic_set_marked_kind (IDE_DIAGNOSTIC (diag), IDE_MARKED_KIND_MARKDOWN);
          ide_diagnostic_take_range (IDE_DIAGNOSTIC (diag), ide_range_new (related_begin_loc, related_end_loc));

          g_ptr_array_add (ar, g_steal_pointer (&diag));
        }
    }

  ret = ide_diagnostics_new ();

  if (ar != NULL)
    {
      for (guint i = 0; i < ar->len; i++)
        ide_diagnostics_add (ret, g_ptr_array_index (ar, i));
    }

  return g_steal_pointer (&ret);
}

static void
ide_lsp_client_text_document_publish_diagnostics (IdeLspClient *self,
                                                  GVariant     *params)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GVariantIter) json_diagnostics = NULL;
  const gchar *uri = NULL;
  gboolean success;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (params != NULL);

  success = JSONRPC_MESSAGE_PARSE (params,
    "uri", JSONRPC_MESSAGE_GET_STRING (&uri),
    "diagnostics", JSONRPC_MESSAGE_GET_ITER (&json_diagnostics)
  );

  if (success)
    {
      g_autoptr(GFile) file = NULL;
      g_autoptr(IdeDiagnostics) diagnostics = NULL;

      file = g_file_new_for_uri (uri);

      diagnostics = ide_lsp_client_translate_diagnostics (self, file, json_diagnostics);

      IDE_TRACE_MSG ("%"G_GSIZE_FORMAT" diagnostics received for %s",
                     diagnostics ? ide_diagnostics_get_size (diagnostics) : 0,
                     uri);

      /*
       * Insert the diagnostics into our cache before emit any signals
       * so that we have up to date information incase the signal causes
       * a callback to query back.
       */
      g_hash_table_insert (priv->diagnostics_by_file,
                           g_object_ref (file),
                           g_object_ref (diagnostics));

      g_signal_emit (self, signals [PUBLISHED_DIAGNOSTICS], 0, file, diagnostics);
    }

  IDE_EXIT;
}

static void
ide_lsp_client_real_notification (IdeLspClient *self,
                                  const gchar  *method,
                                  GVariant     *params)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (method != NULL);

  if (params != NULL)
    {
      if (g_str_equal (method, "textDocument/publishDiagnostics"))
        {
          ide_lsp_client_text_document_publish_diagnostics (self, params);
        }
      else if (g_str_equal (method, "$/progress"))
        {
          gboolean notification_exists = FALSE;
          const gchar *token = NULL;
          const gchar *message = NULL;
          const gchar *title = NULL;
          const gchar *kind = NULL;
          gint64 percentage = -1;
          IdeContext *context;
          IdeNotifications *notifications;
          IdeNotification *notification = NULL;

          JSONRPC_MESSAGE_PARSE (params, "token", JSONRPC_MESSAGE_GET_STRING (&token),
                                         "value", "{",
                                           "kind", JSONRPC_MESSAGE_GET_STRING (&kind),
                                         "}");
          JSONRPC_MESSAGE_PARSE (params, "value", "{",
                                           "title", JSONRPC_MESSAGE_GET_STRING (&title),
                                         "}");
          JSONRPC_MESSAGE_PARSE (params, "value", "{",
                                           "message", JSONRPC_MESSAGE_GET_STRING (&message),
                                         "}");
          JSONRPC_MESSAGE_PARSE (params, "value", "{",
                                           "percentage", JSONRPC_MESSAGE_GET_INT64 (&percentage),
                                         "}");
          context = ide_object_get_context (IDE_OBJECT (self));
          notifications = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
          notification = ide_notifications_find_by_id (notifications, token);
          if (notification == NULL)
            notification_exists = FALSE;
          else
            notification_exists = TRUE;

          if (ide_str_equal0 (kind, "begin"))
            {
              if (!notification_exists)
                {
                  notification = ide_notification_new ();
                  ide_notification_set_id (notification, token);
                  ide_notification_set_has_progress (notification, TRUE);
                  ide_notification_set_progress_is_imprecise (notification, percentage == -1);
                }

              ide_notification_set_title (notification, title);
              ide_notification_set_body (notification, message != NULL ? message : title);
              if (percentage != -1)
                ide_notification_set_progress (notification, percentage / 100.0);
              if (!notification_exists)
                ide_notification_attach (notification, IDE_OBJECT (context));
            }
          else if (notification != NULL)
            {
              if (message != NULL)
                ide_notification_set_body (notification, message);
              if (percentage != -1)
                ide_notification_set_progress (notification, percentage / 100.0);
            }

          if (ide_str_equal0 (kind, "end") && notification != NULL)
            ide_notification_withdraw (notification);
        }
      else if (g_str_equal (method, "window/showMessage"))
        {
          const gchar *message = NULL;

          JSONRPC_MESSAGE_PARSE (params, "message", JSONRPC_MESSAGE_GET_STRING (&message));

          if (!ide_str_empty0 (message))
            ide_object_log (self,
                            G_LOG_LEVEL_MESSAGE,
                            priv->name ? priv->name : G_OBJECT_TYPE_NAME (self),
                            "%s", message);
        }
      else if (g_str_equal (method, "$/logTrace"))
        {
          const char *message = NULL;

          JSONRPC_MESSAGE_PARSE (params, "message", JSONRPC_MESSAGE_GET_STRING (&message));

          if (!ide_str_empty0 (message))
            g_log (priv->name ? priv->name : G_OBJECT_TYPE_NAME (self),
                   IDE_LOG_LEVEL_TRACE,
                   "%s", message);
        }
      else if (g_str_equal (method, "window/logMessage"))
        {
          const char *message = NULL;
          GLogLevelFlags level = G_LOG_LEVEL_MESSAGE;
          gint64 type;

          if (JSONRPC_MESSAGE_PARSE (params, "type", JSONRPC_MESSAGE_GET_INT64 (&type)))
            {
              /* Ignore error/etc because we don't want g_error() fatal errors
               * to be logged. This is just from the LSP.
               */
              switch (type)
                {
                case 1:
                case 2:
                case 3:
                  level = G_LOG_LEVEL_MESSAGE;
                  break;

                case 4:
                  level = G_LOG_LEVEL_INFO;
                  break;

                default:
                  break;
                }
            }

          JSONRPC_MESSAGE_PARSE (params, "message", JSONRPC_MESSAGE_GET_STRING (&message));

          if (!ide_str_empty0 (message))
            ide_object_log (self,
                            level,
                            priv->name ? priv->name : G_OBJECT_TYPE_NAME (self),
                            "%s", message);
        }
    }

  IDE_EXIT;
}

static void
ide_lsp_client_send_notification (IdeLspClient  *self,
                                  const gchar   *method,
                                  GVariant      *params,
                                  JsonrpcClient *rpc_client)
{
  GQuark detail;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (method != NULL);
  g_assert (JSONRPC_IS_CLIENT (rpc_client));

  IDE_TRACE_MSG ("Notification: %s", method);

  /*
   * To avoid leaking quarks we do not create a quark for the string unless
   * it already exists. This should be fine in practice because we only need
   * the quark if there is a caller that has registered for it. And the callers
   * registering for it will necessarily create the quark.
   */
  detail = g_quark_try_string (method);

  g_signal_emit (self, signals [NOTIFICATION], detail, method, params);

  IDE_EXIT;
}

static void
ide_lsp_client_apply_edit_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(AsyncCall) call = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (call != NULL);
  g_assert (JSONRPC_IS_CLIENT (call->client));
  g_assert (call->id != NULL);

  if (ide_buffer_manager_apply_edits_finish (bufmgr, result, &error))
    reply = JSONRPC_MESSAGE_NEW ("applied", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE));
  else
    reply = JSONRPC_MESSAGE_NEW ("applied", JSONRPC_MESSAGE_PUT_BOOLEAN (FALSE),
                                 "failureReason", JSONRPC_MESSAGE_PUT_STRING (error->message));

  jsonrpc_client_reply_async (call->client,
                              call->id,
                              reply,
                              NULL, NULL, NULL);

  IDE_EXIT;
}

static gboolean
ide_lsp_client_handle_apply_edit (IdeLspClient  *self,
                                  JsonrpcClient *client,
                                  GVariant      *id,
                                  GVariant      *params)
{
  g_autoptr(GVariant) parent = NULL;
  g_autoptr(IdeLspWorkspaceEdit) workspace_edit = NULL;
  g_autoptr(GPtrArray) edits = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (id != NULL);
  g_assert (params != NULL);

  if (!(parent = g_variant_lookup_value (params, "edit", G_VARIANT_TYPE_VARDICT)))
    IDE_GOTO (invalid_params);

  edits = g_ptr_array_new_with_free_func (g_object_unref);

  workspace_edit = ide_lsp_workspace_edit_new(parent);
  edits = ide_lsp_workspace_edit_get_edits(workspace_edit);

  if (edits->len > 0)
    {
      g_autoptr(IdeContext) context = ide_object_ref_context (IDE_OBJECT (self));
      IdeBufferManager *bufmgr = ide_buffer_manager_from_context (context);

      ide_buffer_manager_apply_edits_async (bufmgr,
                                            IDE_PTR_ARRAY_STEAL_FULL (&edits),
                                            NULL,
                                            ide_lsp_client_apply_edit_cb,
                                            async_call_new (client, id));

      IDE_RETURN (TRUE);
    }

invalid_params:
  IDE_RETURN (FALSE);
}

static gboolean
ide_lsp_client_handle_call (IdeLspClient  *self,
                            const gchar   *method,
                            GVariant      *id,
                            GVariant      *params,
                            JsonrpcClient *client)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (method != NULL);
  g_assert (id != NULL);
  g_assert (JSONRPC_IS_CLIENT (client));

  IDE_TRACE_MSG ("Received remote call for method \"%s\"", method);

  if (strcmp (method, "workspace/configuration") == 0)
    {
      g_autoptr(GVariant) config = NULL;

      /* TODO: Subclasses will really need the params to be able to handle
       *       workspace/configuration correctly.
       */

      g_signal_emit (self, signals [LOAD_CONFIGURATION], 0, &config);

      if (config == NULL)
        {
          GVariantBuilder builder;

          g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));
          config = g_variant_builder_end (&builder);
        }

      /* Ensure we didn't get anything floating */
      g_variant_take_ref (config);

      jsonrpc_client_reply_async (client, id, config, NULL, NULL, NULL);

      IDE_RETURN (TRUE);
    }
  else if (strcmp (method, "workspace/applyEdit") == 0)
    {
      gboolean ret = FALSE;

      if (params != NULL)
        ret = ide_lsp_client_handle_apply_edit (self, client, id, params);

      IDE_RETURN (ret);
    }
  else if (strcmp (method, "window/workDoneProgress/create") == 0)
    {
      jsonrpc_client_reply_async (client, id, NULL, NULL, NULL, NULL);
      IDE_RETURN (TRUE);
    }

  IDE_RETURN (FALSE);
}

static void
ide_lsp_client_destroy (IdeObject *object)
{
  IdeLspClient *self = (IdeLspClient *)object;
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());

  if (priv->rpc_client != NULL)
    g_object_run_dispose (G_OBJECT (priv->rpc_client));

  while (priv->pending_messages.length > 0)
    {
      PendingMessage *message = priv->pending_messages.head->data;

      g_queue_unlink (&priv->pending_messages, &message->link);
      pending_message_fail (message);
    }

  IDE_OBJECT_CLASS (ide_lsp_client_parent_class)->destroy (object);
}

static void
ide_lsp_client_finalize (GObject *object)
{
  IdeLspClient *self = (IdeLspClient *)object;
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->diagnostics_by_file, g_hash_table_unref);
  g_clear_pointer (&priv->server_capabilities, g_variant_unref);
  g_clear_pointer (&priv->languages, g_ptr_array_unref);
  g_clear_pointer (&priv->root_uri, g_free);
  g_clear_object (&priv->rpc_client);
  g_clear_object (&priv->buffer_manager_signals);
  g_clear_object (&priv->project_signals);

  G_OBJECT_CLASS (ide_lsp_client_parent_class)->finalize (object);
}

static gboolean
ide_lsp_client_real_supports_language (IdeLspClient *self,
                                       const gchar  *language_id)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (language_id != NULL);

  if (g_str_equal (language_id, "python3"))
    language_id = "python";

  for (guint i = 0; i < priv->languages->len; i++)
    {
      const gchar *id = g_ptr_array_index (priv->languages, i);

      if (g_strcmp0 (language_id, id) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
ide_lsp_client_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeLspClient *self = IDE_LSP_CLIENT (object);
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;

    case PROP_SERVER_CAPABILITIES:
      g_value_set_variant (value, priv->server_capabilities);
      break;

    case PROP_IO_STREAM:
      g_value_set_object (value, priv->io_stream);
      break;

    case PROP_TRACE:
      g_value_set_enum (value, ide_lsp_client_get_trace (self));
      break;

    case PROP_ROOT_URI:
      g_value_set_string (value, priv->root_uri);
      break;

    case PROP_USE_MARKDOWN_IN_DIAGNOSTICS:
      g_value_set_boolean (value, priv->use_markdown_in_diagnostics);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_client_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeLspClient *self = IDE_LSP_CLIENT (object);
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_USE_MARKDOWN_IN_DIAGNOSTICS:
      priv->use_markdown_in_diagnostics = g_value_get_boolean (value);
      break;

    case PROP_NAME:
      ide_lsp_client_set_name (self, g_value_get_string (value));
      break;

    case PROP_IO_STREAM:
      priv->io_stream = g_value_dup_object (value);
      break;

    case PROP_TRACE:
      ide_lsp_client_set_trace (self, g_value_get_enum (value));
      break;

    case PROP_ROOT_URI:
      ide_lsp_client_set_root_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_client_class_init (IdeLspClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_client_finalize;
  object_class->get_property = ide_lsp_client_get_property;
  object_class->set_property = ide_lsp_client_set_property;

  i_object_class->destroy = ide_lsp_client_destroy;

  klass->notification = ide_lsp_client_real_notification;
  klass->supports_language = ide_lsp_client_real_supports_language;

  properties [PROP_INITIALIZATION_OPTIONS] =
    g_param_spec_variant ("initialization-options",
                          "Initialization Options",
                          "Initialization Options",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_MARKDOWN_IN_DIAGNOSTICS] =
    g_param_spec_boolean ("use-markdown-in-diagnostics",
                          "Use Markdown in Diagnostics",
                          "If Diagnostics can contain markdown",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SERVER_CAPABILITIES] =
    g_param_spec_variant ("server-capabilities",
                         "Server Capabilities",
                         "The server capabilities as provided by the server",
                         G_VARIANT_TYPE_VARDICT,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The GIOStream to communicate over",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TRACE] =
    g_param_spec_enum ("trace",
                       "Trace",
                       "If tracing should be enabled on the peer.",
                       IDE_TYPE_LSP_TRACE,
                       IDE_LSP_TRACE_OFF,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ROOT_URI] =
    g_param_spec_string ("root-uri",
                         "Root Uri",
                         "The root uri the LSP should work on",
                         "",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);


  signals [INITIALIZED] =
    g_signal_new ("initialized",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, initialized),
                  NULL, NULL,
                  ide_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [INITIALIZED],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__VOIDv);

  /**
   * IdeLspClient::load-configuration:
   * @self: a #IdeLspClient
   *
   * Loads the configuration object to reply to a workspace/configuration
   * request from the peer.
   *
   * Returns: (transfer full): a #GVariant containing the result or %NULL
   *   to proceed to the next signal handler.
   */
  signals [LOAD_CONFIGURATION] =
    g_signal_new ("load-configuration",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, load_configuration),
                  g_signal_accumulator_first_wins, NULL,
                  ide_marshal_BOXED__VOID,
                  G_TYPE_VARIANT, 0);
  g_signal_set_va_marshaller (signals [LOAD_CONFIGURATION],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOXED__VOIDv);

  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (IdeLspClientClass, notification),
                  NULL, NULL,
                  ide_marshal_VOID__STRING_VARIANT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT);
  g_signal_set_va_marshaller (signals [NOTIFICATION],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__STRING_VARIANTv);

  signals [SUPPORTS_LANGUAGE] =
    g_signal_new ("supports-language",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, supports_language),
                  g_signal_accumulator_true_handled, NULL,
                  ide_marshal_BOOLEAN__STRING,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [SUPPORTS_LANGUAGE],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOOLEAN__STRINGv);

  signals [PUBLISHED_DIAGNOSTICS] =
    g_signal_new ("published-diagnostics",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, published_diagnostics),
                  NULL, NULL,
                  ide_marshal_VOID__OBJECT_OBJECT,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_FILE | G_SIGNAL_TYPE_STATIC_SCOPE,
                  IDE_TYPE_DIAGNOSTICS | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals [PUBLISHED_DIAGNOSTICS],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_VOID__OBJECT_OBJECTv);

}

static void
ide_lsp_client_init (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());

  priv->trace = IDE_LSP_TRACE_OFF;
  priv->languages = g_ptr_array_new_with_free_func (g_free);
  priv->initialized = FALSE;

  priv->diagnostics_by_file = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                                     (GEqualFunc)g_file_equal,
                                                     g_object_unref,
                                                     (GDestroyNotify)g_object_unref);

  priv->buffer_manager_signals = g_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  g_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-loaded",
                                   G_CALLBACK (ide_lsp_client_buffer_loaded),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-saved",
                                   G_CALLBACK (ide_lsp_client_buffer_saved),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-unloaded",
                                   G_CALLBACK (ide_lsp_client_buffer_unloaded),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->buffer_manager_signals,
                           "bind",
                           G_CALLBACK (ide_lsp_client_buffer_manager_bind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->buffer_manager_signals,
                           "unbind",
                           G_CALLBACK (ide_lsp_client_buffer_manager_unbind),
                           self,
                           G_CONNECT_SWAPPED);

  priv->project_signals = g_signal_group_new (IDE_TYPE_PROJECT);

  g_signal_group_connect_object (priv->project_signals,
                                   "file-trashed",
                                   G_CALLBACK (ide_lsp_client_project_file_trashed),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_group_connect_object (priv->project_signals,
                                   "file-renamed",
                                   G_CALLBACK (ide_lsp_client_project_file_renamed),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
ide_lsp_client_flush_queue (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (!priv->rpc_client || JSONRPC_IS_CLIENT (priv->rpc_client));

  if (priv->pending_messages.length == 0 || priv->rpc_client == NULL)
    IDE_EXIT;

  IDE_TRACE_MSG ("Flushing pending queue of length %u", priv->pending_messages.length);

  while (priv->pending_messages.length > 0)
    {
      PendingMessage *message = priv->pending_messages.head->data;
      g_queue_unlink (&priv->pending_messages, &message->link);
      pending_message_submit (message, priv->rpc_client);
    }

  IDE_EXIT;
}

static void
ide_lsp_client_initialized_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  JsonrpcClient *rpc_client = (JsonrpcClient *)object;
  g_autoptr(IdeLspClient) self = user_data;
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeProject *project;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSONRPC_IS_CLIENT (rpc_client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LSP_CLIENT (self));

  if (!jsonrpc_client_send_notification_finish (rpc_client, result, &error))
    g_debug ("LSP initialized notification failed: %s",
             error->message);

  context = ide_object_get_context (IDE_OBJECT (self));
  buffer_manager = ide_buffer_manager_from_context (context);
  g_signal_group_set_target (priv->buffer_manager_signals, buffer_manager);

  project = ide_project_from_context (context);
  g_signal_group_set_target (priv->project_signals, project);

  priv->initialized = TRUE;

  g_signal_emit (self, signals[INITIALIZED], 0);

  ide_lsp_client_flush_queue (self);

  IDE_EXIT;
}

static void
ide_lsp_client_extract_server_capabilities (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  GVariant *capabilities;
  gint64 tds;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));

  priv->text_document_sync = TEXT_DOCUMENT_SYNC_INCREMENTAL;

  if (!(capabilities = priv->server_capabilities))
    return;

  if (JSONRPC_MESSAGE_PARSE (capabilities, "textDocumentSync", JSONRPC_MESSAGE_GET_INT64 (&tds)) ||
      JSONRPC_MESSAGE_PARSE (capabilities,
                             "textDocumentSync", "{",
                               "change", JSONRPC_MESSAGE_GET_INT64 (&tds),
                             "}"))
    priv->text_document_sync = tds & 0x3;

  IDE_EXIT;
}

static void
ide_lsp_client_initialize_cb (GObject      *object,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  JsonrpcClient *rpc_client = (JsonrpcClient *)object;
  g_autoptr(IdeLspClient) self = user_data;
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) initialized_param = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSONRPC_IS_CLIENT (rpc_client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LSP_CLIENT (self));

  if (!jsonrpc_client_call_finish (rpc_client, result, &reply, &error))
    {
      /* translators: %s is replaced with the error message */
      g_debug (_("Failed to initialize language server: %s"), error->message);
      ide_lsp_client_stop (self);
      IDE_EXIT;
    }

  /* Extract capabilities for future use */
  g_clear_pointer (&priv->server_capabilities, g_variant_unref);
  if (g_variant_is_of_type (reply, G_VARIANT_TYPE_VARDICT))
    priv->server_capabilities = g_variant_lookup_value (reply, "capabilities", G_VARIANT_TYPE_VARDICT);
  ide_lsp_client_extract_server_capabilities (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SERVER_CAPABILITIES]);

  initialized_param = JSONRPC_MESSAGE_NEW ("initializedParams", "{", "}");

  jsonrpc_client_send_notification_async (rpc_client,
                                          "initialized",
                                          initialized_param,
                                          NULL,
                                          ide_lsp_client_initialized_cb,
                                          g_object_ref (self));

  IDE_EXIT;
}

IdeLspClient *
ide_lsp_client_new (GIOStream *io_stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (io_stream), NULL);

  return g_object_new (IDE_TYPE_LSP_CLIENT,
                       "io-stream", io_stream,
                       NULL);
}

void
ide_lsp_client_start (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *root_path = NULL;
  g_autofree gchar *root_uri = NULL;
  g_autofree gchar *basename = NULL;
  const gchar *trace_string;
  IdeContext *context;
  GFile *workdir;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!G_IS_IO_STREAM (priv->io_stream) || !IDE_IS_CONTEXT (context))
    {
      ide_object_message (self,
                          "Cannot start %s due to misconfiguration.",
                          G_OBJECT_TYPE_NAME (self));
      return;
    }

  priv->rpc_client = jsonrpc_client_new (priv->io_stream);

  basename = ide_context_dup_title (context);
  workdir = ide_context_ref_workdir (context);
  root_path = g_file_get_path (workdir);
  root_uri = g_strdup (priv->root_uri);
  if (root_uri == NULL)
    root_uri = g_file_get_uri (workdir);

  switch (priv->trace)
    {
    case IDE_LSP_TRACE_VERBOSE:
      trace_string = "verbose";
      break;

    case IDE_LSP_TRACE_MESSAGES:
      trace_string = "messages";
      break;

    case IDE_LSP_TRACE_OFF:
    default:
      trace_string = "off";
      break;
    }

  /*
   * The first thing we need to do is initialize the client with information
   * about our project. So that we will perform asynchronously here. It will
   * also start our read loop.
   */

  params = JSONRPC_MESSAGE_NEW (
#if 0
    /* Some LSPs will monitor the PID of the editor and exit when they
     * detect the editor has exited. Since we are likely in a different
     * PID namespace than the LSP, there is a PID mismatch and it will
     * probably get PID 2 (from Flatpak) and not be of any use.
     *
     * Just ignore it as the easiest solution.
     *
     * If this causes problems elsewhere, we might need to try to setup
     * a quirk handler for some LSPs.
     *
     * https://gitlab.gnome.org/GNOME/gnome-builder/-/issues/2050
     */
    "processId", JSONRPC_MESSAGE_PUT_INT64 (getpid ()),
#endif
    "rootUri", JSONRPC_MESSAGE_PUT_STRING (root_uri),
    "clientInfo", "{",
      "name", JSONRPC_MESSAGE_PUT_STRING (PACKAGE_NAME),
      "version", JSONRPC_MESSAGE_PUT_STRING (PACKAGE_VERSION),
    "}",
    "rootPath", JSONRPC_MESSAGE_PUT_STRING (root_path),
    "workspaceFolders", "[",
      "{",
        "uri", JSONRPC_MESSAGE_PUT_STRING (root_uri),
        "name", JSONRPC_MESSAGE_PUT_STRING (basename),
      "}",
    "]",
    "trace", JSONRPC_MESSAGE_PUT_STRING (trace_string),
    "capabilities", "{",
      "workspace", "{",
        "applyEdit", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "configuration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "symbol", "{",
          "SymbolKind", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1), /* File */
              JSONRPC_MESSAGE_PUT_INT64 (2), /* Module */
              JSONRPC_MESSAGE_PUT_INT64 (3), /* Namespace */
              JSONRPC_MESSAGE_PUT_INT64 (4), /* Package */
              JSONRPC_MESSAGE_PUT_INT64 (5), /* Class */
              JSONRPC_MESSAGE_PUT_INT64 (6), /* Method */
              JSONRPC_MESSAGE_PUT_INT64 (7), /* Property */
              JSONRPC_MESSAGE_PUT_INT64 (8), /* Field */
              JSONRPC_MESSAGE_PUT_INT64 (9), /* Constructor */
              JSONRPC_MESSAGE_PUT_INT64 (10), /* Enum */
              JSONRPC_MESSAGE_PUT_INT64 (11), /* Interface */
              JSONRPC_MESSAGE_PUT_INT64 (12), /* Function */
              JSONRPC_MESSAGE_PUT_INT64 (13), /* Variable */
              JSONRPC_MESSAGE_PUT_INT64 (14), /* Constant */
              JSONRPC_MESSAGE_PUT_INT64 (15), /* String */
              JSONRPC_MESSAGE_PUT_INT64 (16), /* Number */
              JSONRPC_MESSAGE_PUT_INT64 (17), /* Boolean */
              JSONRPC_MESSAGE_PUT_INT64 (18), /* Array */
              JSONRPC_MESSAGE_PUT_INT64 (19), /* Object */
              JSONRPC_MESSAGE_PUT_INT64 (20), /* Key */
              JSONRPC_MESSAGE_PUT_INT64 (21), /* Null */
              JSONRPC_MESSAGE_PUT_INT64 (22), /* EnumMember */
              JSONRPC_MESSAGE_PUT_INT64 (23), /* Struct */
              JSONRPC_MESSAGE_PUT_INT64 (24), /* Event */
              JSONRPC_MESSAGE_PUT_INT64 (25), /* Operator */
              JSONRPC_MESSAGE_PUT_INT64 (26), /* TypeParameter */
            "]",
          "}",
        "}",
      "}",
      "textDocument", "{",
        "completion", "{",
          "contextSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "completionItem", "{",
            "snippetSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
            "documentationFormat", "[",
              "markdown",
              "plaintext",
            "]",
            "deprecatedSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "}",
          "completionItemKind", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1),
              JSONRPC_MESSAGE_PUT_INT64 (2),
              JSONRPC_MESSAGE_PUT_INT64 (3),
              JSONRPC_MESSAGE_PUT_INT64 (4),
              JSONRPC_MESSAGE_PUT_INT64 (5),
              JSONRPC_MESSAGE_PUT_INT64 (6),
              JSONRPC_MESSAGE_PUT_INT64 (7),
              JSONRPC_MESSAGE_PUT_INT64 (8),
              JSONRPC_MESSAGE_PUT_INT64 (9),
              JSONRPC_MESSAGE_PUT_INT64 (10),
              JSONRPC_MESSAGE_PUT_INT64 (11),
              JSONRPC_MESSAGE_PUT_INT64 (12),
              JSONRPC_MESSAGE_PUT_INT64 (13),
              JSONRPC_MESSAGE_PUT_INT64 (14),
              JSONRPC_MESSAGE_PUT_INT64 (15),
              JSONRPC_MESSAGE_PUT_INT64 (16),
              JSONRPC_MESSAGE_PUT_INT64 (17),
              JSONRPC_MESSAGE_PUT_INT64 (18),
              JSONRPC_MESSAGE_PUT_INT64 (19),
              JSONRPC_MESSAGE_PUT_INT64 (20),
              JSONRPC_MESSAGE_PUT_INT64 (21),
              JSONRPC_MESSAGE_PUT_INT64 (22),
              JSONRPC_MESSAGE_PUT_INT64 (23),
              JSONRPC_MESSAGE_PUT_INT64 (24),
              JSONRPC_MESSAGE_PUT_INT64 (25),
            "]",
          "}",
        "}",
        "hover", "{",
          "contentFormat", "[",
            "markdown",
            "plaintext",
          "]",
        "}",
        "publishDiagnostics", "{",
          "tagSupport", "{",
            "valueSet", "[",
              JSONRPC_MESSAGE_PUT_INT64 (1),
              JSONRPC_MESSAGE_PUT_INT64 (2),
            "]",
          "}",
        "}",
        "codeAction", "{",
          "dynamicRegistration", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "isPreferredSupport", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
          "codeActionLiteralSupport", "{",
            "codeActionKind", "{",
              "valueSet", "[",
                "",
                "quickfix",
                "refactor",
                "refactor.extract",
                "refactor.inline",
                "refactor.rewrite",
                "source",
                "source.organizeImports",
              "]",
            "}",
          "}",
        "}",
      "}",
      "window", "{",
        "workDoneProgress", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
      "}",
    "}",
    "initializationOptions", "{",
      JSONRPC_MESSAGE_PUT_VARIANT (priv->initialization_options),
    "}"
  );

  /*
   * We connect this before sending initialized because we don't want
   * to lose any possible messages in-between the async calls.
   */
  g_signal_connect_object (priv->rpc_client,
                           "notification",
                           G_CALLBACK (ide_lsp_client_send_notification),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->rpc_client,
                           "handle-call",
                           G_CALLBACK (ide_lsp_client_handle_call),
                           self,
                           G_CONNECT_SWAPPED);

  jsonrpc_client_call_async (priv->rpc_client,
                             "initialize",
                             params,
                             NULL,
                             ide_lsp_client_initialize_cb,
                             g_object_ref (self));

  IDE_EXIT;
}

static void
ide_lsp_client_close_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(IdeLspClient) self = user_data;
  JsonrpcClient *client = (JsonrpcClient *)object;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LSP_CLIENT (self));

  jsonrpc_client_close_finish (client, result, NULL);
}

static void
ide_lsp_client_shutdown_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr(IdeLspClient) self = user_data;
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LSP_CLIENT (self));

  if (!jsonrpc_client_call_finish (client, result, NULL, &error))
    g_debug ("%s", error->message);
  else
    jsonrpc_client_close_async (client,
                                NULL,
                                ide_lsp_client_close_cb,
                                g_steal_pointer (&self));

  IDE_EXIT;
}

void
ide_lsp_client_stop (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));

  if (priv->rpc_client != NULL)
    {
      jsonrpc_client_call_async (priv->rpc_client,
                                 "shutdown",
                                 NULL,
                                 NULL,
                                 ide_lsp_client_shutdown_cb,
                                 g_object_ref (self));
      g_clear_object (&priv->rpc_client);
    }

  IDE_EXIT;
}

static void
ide_lsp_client_call_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!jsonrpc_client_call_finish (client, result, &reply, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             g_steal_pointer (&reply),
                             g_variant_unref);

  IDE_EXIT;
}

static void
ide_lsp_client_queue_message (IdeLspClient *self,
                              const char   *method,
                              GVariant     *params,
                              GCancellable *cancellable,
                              IdeTask      *task)

{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  PendingMessage *pending;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (method != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_TASK (task));

  IDE_TRACE_MSG ("Queuing LSP call to method %s", method);

  pending = g_slice_new0 (PendingMessage);
  pending->link.data = pending;
  pending->task = g_steal_pointer (&task);
  pending->method = g_strdup (method);
  pending->params = params ? g_variant_ref (params) : NULL;
  g_set_object (&pending->cancellable, cancellable);

  g_queue_push_tail_link (&priv->pending_messages, &pending->link);

  IDE_EXIT;
}

/**
 * ide_lsp_client_call_async:
 * @self: An #IdeLspClient
 * @method: the method to call
 * @params: (nullable) (transfer none): An #GVariant or %NULL
 * @cancellable: (nullable): A cancellable or %NULL
 * @callback: the callback to receive the result, or %NULL
 * @user_data: user data for @callback
 *
 * Asynchronously queries the Language Server using the JSON-RPC protocol.
 *
 * If @params is floating, it's floating reference is consumed.
 */
void
ide_lsp_client_call_async (IdeLspClient        *self,
                           const gchar         *method,
                           GVariant            *params,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (!priv->rpc_client || JSONRPC_IS_CLIENT (priv->rpc_client));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_client_call_async);

  if (priv->rpc_client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 _("No connection to language server"));
      IDE_EXIT;
    }

  if (!priv->initialized &&
      !(g_str_equal (method, "initialize") || g_str_equal (method, "initialized")))
    {
      ide_lsp_client_queue_message (self,
                                    method,
                                    params,
                                    cancellable,
                                    g_steal_pointer (&task));
      IDE_EXIT;
    }

  jsonrpc_client_call_async (priv->rpc_client,
                             method,
                             params,
                             cancellable,
                             ide_lsp_client_call_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_lsp_client_call_finish (IdeLspClient  *self,
                            GAsyncResult  *result,
                            GVariant     **return_value,
                            GError       **error)
{
  g_autoptr(GVariant) local_return_value = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  local_return_value = ide_task_propagate_pointer (IDE_TASK (result), error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  IDE_RETURN (ret);
}

static void
ide_lsp_client_send_notification_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!jsonrpc_client_send_notification_finish (client, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_lsp_client_send_notification_async:
 * @self: An #IdeLspClient
 * @method: the method to notification
 * @params: (nullable) (transfer none): An #GVariant or %NULL
 * @cancellable: (nullable): A cancellable or %NULL
 * @notificationback: the notificationback to receive the result, or %NULL
 * @user_data: user data for @notificationback
 *
 * Asynchronously sends a notification to the Language Server.
 *
 * If @params is floating, it's reference is consumed.
 */
void
ide_lsp_client_send_notification_async (IdeLspClient        *self,
                                        const gchar         *method,
                                        GVariant            *params,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  notificationback,
                                        gpointer             user_data)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (method != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, notificationback, user_data);
  ide_task_set_source_tag (task, ide_lsp_client_send_notification_async);

  if (priv->rpc_client == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               _("No connection to language server"));
  else
    jsonrpc_client_send_notification_async (priv->rpc_client,
                                            method,
                                            params,
                                            cancellable,
                                            ide_lsp_client_send_notification_cb,
                                            g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_lsp_client_send_notification_finish (IdeLspClient  *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
ide_lsp_client_get_diagnostics_async (IdeLspClient        *self,
                                      GFile               *file,
                                      GBytes              *content,
                                      const gchar         *lang_id,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  IdeDiagnostics *diagnostics;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_client_get_diagnostics_async);

  diagnostics = g_hash_table_lookup (priv->diagnostics_by_file, file);

  if (diagnostics != NULL)
    ide_task_return_pointer (task,
                             g_object_ref (diagnostics),
                             (GDestroyNotify)g_object_unref);
  else
    ide_task_return_pointer (task,
                             ide_diagnostics_new (),
                             (GDestroyNotify)g_object_unref);
}

/**
 * ide_lsp_client_get_diagnostics_finish:
 * @self: an #IdeLspClient
 * @result: a #GAsyncResult
 * @diagnostics: (nullable) (out): A location for a #IdeDiagnostics or %NULL
 * @error: A location for a #GError or %NULL
 *
 * Completes a request to ide_lsp_client_get_diagnostics_async().
 *
 * Returns: %TRUE if successful and @diagnostics is set, otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_lsp_client_get_diagnostics_finish (IdeLspClient    *self,
                                       GAsyncResult    *result,
                                       IdeDiagnostics **diagnostics,
                                       GError         **error)
{
  g_autoptr(IdeDiagnostics) local_diagnostics = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  local_diagnostics = ide_task_propagate_pointer (IDE_TASK (result), &local_error);
  ret = local_diagnostics != NULL;

  if (local_diagnostics != NULL && diagnostics != NULL)
    *diagnostics = g_steal_pointer (&local_diagnostics);

  if (local_error)
    g_propagate_error (error, g_steal_pointer (&local_error));

  return ret;
}

void
ide_lsp_client_add_language (IdeLspClient *self,
                             const gchar  *language_id)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (language_id != NULL);

  g_ptr_array_add (priv->languages, g_strdup (language_id));
}

IdeLspTrace
ide_lsp_client_get_trace (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), IDE_LSP_TRACE_OFF);

  return priv->trace;
}

void
ide_lsp_client_set_trace (IdeLspClient *self,
                          IdeLspTrace   trace)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CLIENT (self));
  g_return_if_fail (trace == IDE_LSP_TRACE_OFF ||
                    trace == IDE_LSP_TRACE_MESSAGES ||
                    trace == IDE_LSP_TRACE_VERBOSE);

  if (trace != priv->trace)
    {
      priv->trace = trace;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TRACE]);
    }
}

void
ide_lsp_client_set_root_uri (IdeLspClient *self,
                             const gchar  *root_uri)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CLIENT (self));

  if (g_set_str (&priv->root_uri, root_uri))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT_URI]);
    }
}

/**
 * ide_lsp_client_get_server_capabilities:
 * @self: a #IdeLspClient
 *
 * Gets the capabilities provided to us by the server after initializing.
 *
 * This value is not available until after connecting and initializing
 * the connection.
 *
 * Returns: (transfer none) (nullable): a #GVariant that is a
 *   %G_VARIANT_TYPE_VARDICT or %NULL.
 */
GVariant *
ide_lsp_client_get_server_capabilities (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), NULL);

  return priv->server_capabilities;
}

/**
 * ide_lsp_client_set_initialization_options:
 * @self: a [class@LspClient]
 * @options: (nullable): a #GVariant or %NULL
 *
 * Sets the `initilizationOptions` to send to the language server
 * when the server is initialized.
 *
 * if @options is floating, the floating reference will be taken
 * when calling this function otherwise the reference count of
 * @options will be incremented by one.
 */
void
ide_lsp_client_set_initialization_options (IdeLspClient *self,
                                           GVariant     *options)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CLIENT (self));

  if (options == priv->initialization_options)
    return;

  g_clear_pointer (&priv->initialization_options, g_variant_unref);
  if (options)
    priv->initialization_options = g_variant_ref_sink (options);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INITIALIZATION_OPTIONS]);
}

/**
 * ide_lsp_client_get_initialization_options:
 * @self: a [class@LspClient]
 *
 * Gets the initialization options for the client.
 *
 * Returns: (transfer none) (nullable): a [struct@GLib.Variant] or %NULL
 */
GVariant *
ide_lsp_client_get_initialization_options (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_CLIENT (self), NULL);

  return priv->initialization_options;
}

/**
 * ide_lsp_client_set_name:
 * @self: a #IdeLspClient
 * @name: the name of the LSP like "gopls"
 *
 * Sets the name for the client.
 *
 * This is useful in situations where you want to be able to have better
 * logging messages which include the LSP name.
 *
 * Since: 44
 */
void
ide_lsp_client_set_name (IdeLspClient *self,
                         const char   *name)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_CLIENT (self));

  if (g_set_str (&priv->name, name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NAME]);
}
