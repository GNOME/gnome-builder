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

#include <dazzle.h>
#include <dazzle.h>
#include <glib/gi18n.h>
#include <jsonrpc-glib.h>
#include <libide-code.h>
#include <libide-projects.h>
#include <libide-sourceview.h>
#include <libide-threading.h>
#include <unistd.h>

#include "ide-lsp-client.h"

typedef struct
{
  DzlSignalGroup *buffer_manager_signals;
  DzlSignalGroup *project_signals;
  JsonrpcClient  *rpc_client;
  GIOStream      *io_stream;
  GHashTable     *diagnostics_by_file;
  GPtrArray      *languages;
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
  PROP_0,
  PROP_IO_STREAM,
  N_PROPS
};

enum {
  PUBLISHED_DIAGNOSTICS,
  NOTIFICATION,
  SUPPORTS_LANGUAGE,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

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

  ide_lsp_client_send_notification_async (self, "textDocument/didSave", params, NULL, NULL, NULL);

  IDE_EXIT;
}

/*
 * TODO: This should all be delayed and buffered so we coalesce multiple
 *       events into a single dispatch.
 */

static void
ide_lsp_client_buffer_insert_text (IdeLspClient *self,
                                   GtkTextIter  *location,
                                   const gchar  *new_text,
                                   gint          len,
                                   IdeBuffer    *buffer)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *copy = NULL;
  gint64 version;
  gint line;
  gint column;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  copy = g_strndup (new_text, len);

  uri = ide_buffer_dup_uri (buffer);
  version = (gint64)ide_buffer_get_change_count (buffer);

  line = gtk_text_iter_get_line (location);
  column = gtk_text_iter_get_line_offset (location);

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

  ide_lsp_client_send_notification_async (self, "textDocument/didChange",
                                               params, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_delete_range (IdeLspClient *self,
                                    GtkTextIter  *begin_iter,
                                    GtkTextIter  *end_iter,
                                    IdeBuffer    *buffer)
{

  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *uri = NULL;
  GtkTextIter copy_begin;
  GtkTextIter copy_end;
  struct {
    gint line;
    gint column;
  } begin, end;
  gint version;
  gint length;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (begin_iter != NULL);
  g_assert (end_iter != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  uri = ide_buffer_dup_uri (buffer);
  version = (gint)ide_buffer_get_change_count (buffer);

  copy_begin = *begin_iter;
  copy_end = *end_iter;
  gtk_text_iter_order (&copy_begin, &copy_end);

  begin.line = gtk_text_iter_get_line (&copy_begin);
  begin.column = gtk_text_iter_get_line_offset (&copy_begin);

  end.line = gtk_text_iter_get_line (&copy_end);
  end.column = gtk_text_iter_get_line_offset (&copy_end);

  length = gtk_text_iter_get_offset (&copy_end) - gtk_text_iter_get_offset (&copy_begin);

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
        "text", "",
      "}",
    "]");

  ide_lsp_client_send_notification_async (self, "textDocument/didChange",
                                               params, NULL, NULL, NULL);

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

  g_signal_connect_object (buffer,
                           "insert-text",
                           G_CALLBACK (ide_lsp_client_buffer_insert_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "delete-range",
                           G_CALLBACK (ide_lsp_client_buffer_delete_range),
                           self,
                           G_CONNECT_SWAPPED);

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

  ide_lsp_client_send_notification_async (self, "textDocument/didOpen",
                                               params, NULL, NULL, NULL);

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

  ide_lsp_client_send_notification_async (self, "textDocument/didClose",
                                               params, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_client_buffer_manager_bind (IdeLspClient     *self,
                                    IdeBufferManager *buffer_manager,
                                    DzlSignalGroup   *signal_group)
{
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (DZL_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_lsp_client_buffer_loaded (self, buffer, buffer_manager);
    }
}

static void
ide_lsp_client_buffer_manager_unbind (IdeLspClient *self,
                                           DzlSignalGroup    *signal_group)
{
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (DZL_IS_SIGNAL_GROUP (signal_group));

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
      g_autoptr(IdeDiagnostic) diag = NULL;
      g_autoptr(GVariant) range = NULL;
      const gchar *message = NULL;
      const gchar *source = NULL;
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

      diag = ide_diagnostic_new (severity, message, begin_loc);
      ide_diagnostic_take_range (diag, ide_range_new (begin_loc, end_loc));

      g_ptr_array_add (ar, g_steal_pointer (&diag));
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
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_LSP_CLIENT (self));
  g_assert (method != NULL);

  if (params != NULL)
    {
      if (g_str_equal (method, "textDocument/publishDiagnostics"))
        ide_lsp_client_text_document_publish_diagnostics (self, params);
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
ide_lsp_client_finalize (GObject *object)
{
  IdeLspClient *self = (IdeLspClient *)object;
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());

  g_clear_pointer (&priv->diagnostics_by_file, g_hash_table_unref);
  g_clear_pointer (&priv->languages, g_ptr_array_unref);
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
    case PROP_IO_STREAM:
      g_value_set_object (value, priv->io_stream);
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
    case PROP_IO_STREAM:
      priv->io_stream = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_client_class_init (IdeLspClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_client_finalize;
  object_class->get_property = ide_lsp_client_get_property;
  object_class->set_property = ide_lsp_client_set_property;

  klass->notification = ide_lsp_client_real_notification;
  klass->supports_language = ide_lsp_client_real_supports_language;

  properties [PROP_IO_STREAM] =
    g_param_spec_object ("io-stream",
                         "IO Stream",
                         "The GIOStream to communicate over",
                         G_TYPE_IO_STREAM,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (IdeLspClientClass, notification),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_VARIANT);

  signals [SUPPORTS_LANGUAGE] =
    g_signal_new ("supports-language",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, supports_language),
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  signals [PUBLISHED_DIAGNOSTICS] =
    g_signal_new ("published-diagnostics",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLspClientClass, published_diagnostics),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_FILE,
                  IDE_TYPE_DIAGNOSTICS);

}

static void
ide_lsp_client_init (IdeLspClient *self)
{
  IdeLspClientPrivate *priv = ide_lsp_client_get_instance_private (self);

  g_assert (IDE_IS_MAIN_THREAD ());

  priv->languages = g_ptr_array_new_with_free_func (g_free);

  priv->diagnostics_by_file = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                                     (GEqualFunc)g_file_equal,
                                                     g_object_unref,
                                                     (GDestroyNotify)g_object_unref);

  priv->buffer_manager_signals = dzl_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  dzl_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-loaded",
                                   G_CALLBACK (ide_lsp_client_buffer_loaded),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-saved",
                                   G_CALLBACK (ide_lsp_client_buffer_saved),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_manager_signals,
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

  priv->project_signals = dzl_signal_group_new (IDE_TYPE_PROJECT);

  dzl_signal_group_connect_object (priv->project_signals,
                                   "file-trashed",
                                   G_CALLBACK (ide_lsp_client_project_file_trashed),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->project_signals,
                                   "file-renamed",
                                   G_CALLBACK (ide_lsp_client_project_file_renamed),
                                   self,
                                   G_CONNECT_SWAPPED);
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
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeProject *project;
  IdeContext *context;

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

  /* TODO: Check for server capabilities */

  /*
   * Now that we are connected and have initialized the peer, setup our
   * buffer_manager and project signals so that we can notify the peer
   * of open documents and such.
   */

  context = ide_object_get_context (IDE_OBJECT (self));

  buffer_manager = ide_buffer_manager_from_context (context);
  dzl_signal_group_set_target (priv->buffer_manager_signals, buffer_manager);

  project = ide_project_from_context (context);
  dzl_signal_group_set_target (priv->project_signals, project);

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

  g_signal_connect_object (priv->rpc_client,
                           "notification",
                           G_CALLBACK (ide_lsp_client_send_notification),
                           self,
                           G_CONNECT_SWAPPED);

  workdir = ide_context_ref_workdir (context);
  root_path = g_file_get_path (workdir);
  root_uri = g_file_get_uri (workdir);

  /*
   * The first thing we need to do is initialize the client with information
   * about our project. So that we will perform asynchronously here. It will
   * also start our read loop.
   */

  params = JSONRPC_MESSAGE_NEW (
    "processId", JSONRPC_MESSAGE_PUT_INT64 (getpid ()),
    "rootUri", JSONRPC_MESSAGE_PUT_STRING (root_uri),
    "rootPath", JSONRPC_MESSAGE_PUT_STRING (root_path),
    "capabilities", "{", "}"
  );

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
                             (GDestroyNotify)g_variant_unref);

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
 *
 * Since: 3.26
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
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "No connection to language server");
  else
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
 *
 * Since: 3.26
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
                               "No connection to language server");
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
