/* ide-langserv-client.c
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

#define G_LOG_DOMAIN "ide-langserv-client"

#include <egg-counter.h>
#include <egg-signal-group.h>
#include <jsonrpc-glib.h>
#include <unistd.h>

#include "ide-context.h"
#include "ide-debug.h"

#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "langserv/ide-langserv-client.h"
#include "projects/ide-project.h"
#include "vcs/ide-vcs.h"

typedef struct
{
  EggSignalGroup *buffer_manager_signals;
  EggSignalGroup *project_signals;
  JsonrpcClient  *rpc_client;
  GIOStream      *io_stream;
  GHashTable     *diagnostics_by_file;
  GPtrArray      *languages;
} IdeLangservClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLangservClient, ide_langserv_client, IDE_TYPE_OBJECT)

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
  NOTIFICATION,
  SUPPORTS_LANGUAGE,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static gboolean
ide_langserv_client_supports_buffer (IdeLangservClient *self,
                                     IdeBuffer         *buffer)
{
  GtkSourceLanguage *language;
  const gchar *language_id = "text/plain";
  gboolean ret = FALSE;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  if (language != NULL)
    language_id = gtk_source_language_get_id (language);

  g_signal_emit (self, signals [SUPPORTS_LANGUAGE], 0, language_id, &ret);

  return ret;
}

static void
ide_langserv_client_clear_diagnostics (IdeLangservClient *self,
                                       const gchar       *uri)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (uri != NULL);

  IDE_TRACE_MSG ("Clearing diagnostics for %s", uri);

  file = g_file_new_for_uri (uri);
  g_hash_table_remove (priv->diagnostics_by_file, file);

  IDE_EXIT;
}

static void
ide_langserv_client_buffer_saved (IdeLangservClient *self,
                                  IdeBuffer         *buffer,
                                  IdeBufferManager  *buffer_manager)
{
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_langserv_client_supports_buffer (self, buffer))
    IDE_EXIT;

  uri = ide_buffer_get_uri (buffer);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}"
  );

  ide_langserv_client_notification_async (self, "textDocument/didSave",
                                          g_steal_pointer (&params),
                                          NULL, NULL, NULL);

  IDE_EXIT;
}

/*
 * TODO: This should all be delayed and buffered so we coalesce multiple
 *       events into a single dispatch.
 */

static void
ide_langserv_client_buffer_insert_text (IdeLangservClient *self,
                                        GtkTextIter       *location,
                                        const gchar       *new_text,
                                        gint               len,
                                        IdeBuffer         *buffer)
{
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *copy = NULL;
  gint line;
  gint column;
  gint version;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  copy = g_strndup (new_text, len);

  uri = ide_buffer_get_uri (buffer);
  version = (gint)ide_buffer_get_change_count (buffer);

  line = gtk_text_iter_get_line (location);
  column = gtk_text_iter_get_line_offset (location);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
      "version", JCON_INT (version),
    "}",
    "contentChanges", "[",
      "{",
        "range", "{",
          "start", "{",
            "line", JCON_INT (line),
            "character", JCON_INT (column),
          "}",
          "end", "{",
            "line", JCON_INT (line),
            "character", JCON_INT (column),
          "}",
        "}",
        "rangeLength", JCON_INT (0),
        "text", JCON_STRING (copy),
      "}",
    "]");

  ide_langserv_client_notification_async (self, "textDocument/didChange",
                                          g_steal_pointer (&params),
                                          NULL, NULL, NULL);
}

static void
ide_langserv_client_buffer_delete_range (IdeLangservClient *self,
                                         GtkTextIter       *begin_iter,
                                         GtkTextIter       *end_iter,
                                         IdeBuffer         *buffer)
{

  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;
  struct {
    gint line;
    gint column;
  } begin, end;
  gint version;
  gint length;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (begin_iter != NULL);
  g_assert (end_iter != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  uri = ide_buffer_get_uri (buffer);
  version = (gint)ide_buffer_get_change_count (buffer);

  begin.line = gtk_text_iter_get_line (begin_iter);
  begin.column = gtk_text_iter_get_line_offset (begin_iter);

  end.line = gtk_text_iter_get_line (end_iter);
  end.column = gtk_text_iter_get_line_offset (end_iter);

  length = gtk_text_iter_get_offset (end_iter) - gtk_text_iter_get_offset (begin_iter);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
      "version", JCON_INT (version),
    "}",
    "contentChanges", "[",
      "{",
        "range", "{",
          "start", "{",
            "line", JCON_INT (begin.line),
            "character", JCON_INT (begin.column),
          "}",
          "end", "{",
            "line", JCON_INT (end.line),
            "character", JCON_INT (end.column),
          "}",
        "}",
        "rangeLength", JCON_INT (length),
        "text", "",
      "}",
    "]");

  ide_langserv_client_notification_async (self, "textDocument/didChange",
                                          g_steal_pointer (&params),
                                          NULL, NULL, NULL);
}

static void
ide_langserv_client_buffer_loaded (IdeLangservClient *self,
                                   IdeBuffer         *buffer,
                                   IdeBufferManager  *buffer_manager)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_langserv_client_supports_buffer (self, buffer))
    IDE_EXIT;

  g_signal_connect_object (buffer,
                           "insert-text",
                           G_CALLBACK (ide_langserv_client_buffer_insert_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "delete-range",
                           G_CALLBACK (ide_langserv_client_buffer_delete_range),
                           self,
                           G_CONNECT_SWAPPED);

  uri = ide_buffer_get_uri (buffer);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}"
  );

  jsonrpc_client_notification_async (priv->rpc_client,
                                     "textDocument/didOpen",
                                     g_steal_pointer (&params),
                                     NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_langserv_client_buffer_unloaded (IdeLangservClient *self,
                                     IdeBuffer         *buffer,
                                     IdeBufferManager  *buffer_manager)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  if (!ide_langserv_client_supports_buffer (self, buffer))
    IDE_EXIT;

  uri = ide_buffer_get_uri (buffer);

  params = JCON_NEW (
    "textDocument", "{",
      "uri", JCON_STRING (uri),
    "}"
  );

  jsonrpc_client_notification_async (priv->rpc_client,
                                     "textDocument/didClose",
                                     g_steal_pointer (&params),
                                     NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_langserv_client_buffer_manager_bind (IdeLangservClient *self,
                                         IdeBufferManager  *buffer_manager,
                                         EggSignalGroup    *signal_group)
{
  guint n_items;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (EGG_IS_SIGNAL_GROUP (signal_group));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (buffer_manager));

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeBuffer) buffer = NULL;

      buffer = g_list_model_get_item (G_LIST_MODEL (buffer_manager), i);
      ide_langserv_client_buffer_loaded (self, buffer, buffer_manager);
    }
}

static void
ide_langserv_client_buffer_manager_unbind (IdeLangservClient *self,
                                           EggSignalGroup    *signal_group)
{
  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (EGG_IS_SIGNAL_GROUP (signal_group));

  /* TODO: We need to track everything we've notified so that we
   *       can notify the peer to release its resources.
   */
}

static void
ide_langserv_client_project_file_trashed (IdeLangservClient *self,
                                          GFile             *file,
                                          IdeProject        *project)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PROJECT (project));

  uri = g_file_get_uri (file);

  params = JCON_NEW (
    "changes", "["
      "{",
        "uri", JCON_STRING (uri),
        "type", JCON_INT (FILE_CHANGE_TYPE_DELETED),
      "}",
    "]"
  );

  jsonrpc_client_notification_async (priv->rpc_client,
                                     "workspace/didChangeWatchedFiles",
                                     g_steal_pointer (&params),
                                     NULL, NULL, NULL);

  ide_langserv_client_clear_diagnostics (self, uri);

  IDE_EXIT;
}

static void
ide_langserv_client_project_file_renamed (IdeLangservClient *self,
                                          GFile             *src,
                                          GFile             *dst,
                                          IdeProject        *project)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *src_uri = NULL;
  g_autofree gchar *dst_uri = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (G_IS_FILE (src));
  g_assert (G_IS_FILE (dst));
  g_assert (IDE_IS_PROJECT (project));

  src_uri = g_file_get_uri (src);
  dst_uri = g_file_get_uri (dst);

  params = JCON_NEW (
    "changes", "["
      "{",
        "uri", JCON_STRING (src_uri),
        "type", JCON_INT (FILE_CHANGE_TYPE_DELETED),
      "}",
      "{",
        "uri", JCON_STRING (dst_uri),
        "type", JCON_INT (FILE_CHANGE_TYPE_CREATED),
      "}",
    "]"
  );

  jsonrpc_client_notification_async (priv->rpc_client,
                                     "workspace/didChangeWatchedFiles",
                                     g_steal_pointer (&params),
                                     NULL, NULL, NULL);

  ide_langserv_client_clear_diagnostics (self, src_uri);

  IDE_EXIT;
}

static IdeDiagnostics *
ide_langserv_client_translate_diagnostics (IdeLangservClient *self,
                                           IdeFile           *file,
                                           JsonArray         *diagnostics)
{
  g_autoptr(GPtrArray) ar = NULL;
  guint length;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (diagnostics != NULL);

  length = json_array_get_length (diagnostics);

  ar = g_ptr_array_sized_new (length);
  g_ptr_array_set_free_func (ar, (GDestroyNotify)ide_diagnostic_unref);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *node = json_array_get_element (diagnostics, i);
      g_autoptr(IdeSourceLocation) begin_loc = NULL;
      g_autoptr(IdeSourceLocation) end_loc = NULL;
      g_autoptr(IdeDiagnostic) diag = NULL;
      const gchar *message = NULL;
      const gchar *source = NULL;
      JsonNode *range = NULL;
      gint severity = 0;
      gboolean success;
      struct {
        gint line;
        gint column;
      } begin, end;

      /* Mandatory fields */
      if (!JCON_EXTRACT (node,
                         "range", JCONE_NODE (range),
                         "message", JCONE_STRING (message)))
        continue;

      /* Optional Fields */
      JCON_EXTRACT (node, "severity", JCONE_INT (severity));
      JCON_EXTRACT (node, "source", JCONE_STRING (source));

      /* Extract location information */
      success = JCON_EXTRACT (range,
        "start", "{",
          "line", JCONE_INT (begin.line),
          "character", JCONE_INT (begin.column),
        "}",
        "end", "{",
          "line", JCONE_INT (end.line),
          "character", JCONE_INT (end.column),
        "}"
      );

      if (!success)
        continue;

      begin_loc = ide_source_location_new (file, begin.line, begin.column, 0);
      end_loc = ide_source_location_new (file, end.line, end.column, 0);

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
      ide_diagnostic_take_range (diag, ide_source_range_new (begin_loc, end_loc));

      g_ptr_array_add (ar, g_steal_pointer (&diag));
    }

  return ide_diagnostics_new (g_steal_pointer (&ar));
}

static void
ide_langserv_client_text_document_publish_diagnostics (IdeLangservClient *self,
                                                       JsonNode          *params)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  JsonArray *json_diagnostics = NULL;
  const gchar *uri = NULL;
  gboolean success;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (params != NULL);

  success = JCON_EXTRACT (params,
    "uri", JCONE_STRING (uri),
    "diagnostics", JCONE_ARRAY (json_diagnostics)
  );

  if (success)
    {
      g_autoptr(IdeFile) ifile = NULL;
      g_autoptr(GFile) file = NULL;

      IDE_TRACE_MSG ("Diagnostics received for %s", uri);

      file = g_file_new_for_uri (uri);
      ifile = g_object_new (IDE_TYPE_FILE,
                            "file", file,
                            "context", ide_object_get_context (IDE_OBJECT (self)),
                            NULL);

      g_hash_table_insert (priv->diagnostics_by_file,
                           g_file_new_for_uri (uri),
                           ide_langserv_client_translate_diagnostics (self, ifile, json_diagnostics));
    }

  IDE_EXIT;
}

static void
ide_langserv_client_real_notification (IdeLangservClient *self,
                                       const gchar       *method,
                                       JsonNode          *params)
{
  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (method != NULL);
  g_assert (params != NULL);

  if (g_str_equal (method, "textDocument/publishDiagnostics"))
    ide_langserv_client_text_document_publish_diagnostics (self, params);

  IDE_EXIT;
}

static void
ide_langserv_client_notification (IdeLangservClient *self,
                                  const gchar       *method,
                                  JsonNode          *params,
                                  JsonrpcClient     *rpc_client)
{
  GQuark detail;

  IDE_ENTRY;

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
  g_assert (method != NULL);
  g_assert (params != NULL);
  g_assert (rpc_client != NULL);

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
ide_langserv_client_finalize (GObject *object)
{
  IdeLangservClient *self = (IdeLangservClient *)object;
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  g_clear_pointer (&priv->diagnostics_by_file, g_hash_table_unref);
  g_clear_pointer (&priv->languages, g_ptr_array_unref);
  g_clear_object (&priv->rpc_client);
  g_clear_object (&priv->buffer_manager_signals);
  g_clear_object (&priv->project_signals);

  G_OBJECT_CLASS (ide_langserv_client_parent_class)->finalize (object);
}

static gboolean
ide_langserv_client_real_supports_language (IdeLangservClient *self,
                                            const gchar       *language_id)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  g_assert (IDE_IS_LANGSERV_CLIENT (self));
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
ide_langserv_client_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeLangservClient *self = IDE_LANGSERV_CLIENT (object);
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

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
ide_langserv_client_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeLangservClient *self = IDE_LANGSERV_CLIENT (object);
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

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
ide_langserv_client_class_init (IdeLangservClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_langserv_client_finalize;
  object_class->get_property = ide_langserv_client_get_property;
  object_class->set_property = ide_langserv_client_set_property;

  klass->notification = ide_langserv_client_real_notification;
  klass->supports_language = ide_langserv_client_real_supports_language;

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
                  G_STRUCT_OFFSET (IdeLangservClientClass, notification),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  JSON_TYPE_NODE);

  signals [SUPPORTS_LANGUAGE] =
    g_signal_new ("supports-language",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeLangservClientClass, supports_language),
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
ide_langserv_client_init (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  priv->languages = g_ptr_array_new_with_free_func (g_free);

  priv->diagnostics_by_file = g_hash_table_new_full ((GHashFunc)g_file_hash,
                                                     (GEqualFunc)g_file_equal,
                                                     g_object_unref,
                                                     (GDestroyNotify)ide_diagnostics_unref);

  priv->buffer_manager_signals = egg_signal_group_new (IDE_TYPE_BUFFER_MANAGER);

  egg_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-loaded",
                                   G_CALLBACK (ide_langserv_client_buffer_loaded),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-saved",
                                   G_CALLBACK (ide_langserv_client_buffer_saved),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (priv->buffer_manager_signals,
                                   "buffer-unloaded",
                                   G_CALLBACK (ide_langserv_client_buffer_unloaded),
                                   self,
                                   G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->buffer_manager_signals,
                           "bind",
                           G_CALLBACK (ide_langserv_client_buffer_manager_bind),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->buffer_manager_signals,
                           "unbind",
                           G_CALLBACK (ide_langserv_client_buffer_manager_unbind),
                           self,
                           G_CONNECT_SWAPPED);

  priv->project_signals = egg_signal_group_new (IDE_TYPE_PROJECT);

  egg_signal_group_connect_object (priv->project_signals,
                                   "file-trashed",
                                   G_CALLBACK (ide_langserv_client_project_file_trashed),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (priv->project_signals,
                                   "file-renamed",
                                   G_CALLBACK (ide_langserv_client_project_file_renamed),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
ide_langserv_client_initialize_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  JsonrpcClient *rpc_client = (JsonrpcClient *)object;
  g_autoptr(IdeLangservClient) self = user_data;
  g_autoptr(JsonNode) reply = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (JSONRPC_IS_CLIENT (rpc_client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LANGSERV_CLIENT (self));

  if (!jsonrpc_client_call_finish (rpc_client, result, &reply, &error))
    {
      g_warning ("Failed to initialize language server: %s", error->message);
      ide_langserv_client_stop (self);
      return;
    }

  /* TODO: Check for server capabilities */
}

IdeLangservClient *
ide_langserv_client_new (IdeContext *context,
                         GIOStream  *io_stream)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_LANGSERV_CLIENT,
                       "context", context,
                       "io-stream", io_stream,
                       NULL);
}

void
ide_langserv_client_start (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(JsonNode) params = NULL;
  g_autofree gchar *root_path = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;
  GFile *workdir;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));

  context = ide_object_get_context (IDE_OBJECT (self));

  if (!G_IS_IO_STREAM (priv->io_stream) || !IDE_IS_CONTEXT (context))
    {
      g_warning ("Cannot start %s due to misconfiguration.",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  priv->rpc_client = jsonrpc_client_new (priv->io_stream);

  g_signal_connect_object (priv->rpc_client,
                           "notification",
                           G_CALLBACK (ide_langserv_client_notification),
                           self,
                           G_CONNECT_SWAPPED);


  buffer_manager = ide_context_get_buffer_manager (context);
  egg_signal_group_set_target (priv->buffer_manager_signals, buffer_manager);

  project = ide_context_get_project (context);
  egg_signal_group_set_target (priv->project_signals, project);

  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  root_path = g_file_get_path (workdir);

  /*
   * The first thing we need to do is initialize the client with information
   * about our project. So that we will perform asynchronously here. It will
   * also start our read loop.
   */

  params = JCON_NEW (
    "processId", JCON_INT (getpid ()),
    "rootPath", JCON_STRING (root_path),
    "capabilities", "{", "}"
  );

  jsonrpc_client_call_async (priv->rpc_client,
                             "initialize",
                             g_steal_pointer (&params),
                             NULL,
                             ide_langserv_client_initialize_cb,
                             g_object_ref (self));

  IDE_EXIT;
}

static void
ide_langserv_client_shutdown_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  if (!jsonrpc_client_call_finish (client, result, NULL, &error))
    g_warning ("%s", error->message);

  jsonrpc_client_close_async (client, NULL, NULL, NULL);

  IDE_EXIT;
}

void
ide_langserv_client_stop (IdeLangservClient *self)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));

  if (priv->rpc_client != NULL)
    {
      jsonrpc_client_call_async (priv->rpc_client,
                                 "shutdown",
                                 NULL,
                                 NULL,
                                 ide_langserv_client_shutdown_cb,
                                 NULL);
      g_clear_object (&priv->rpc_client);
    }

  IDE_EXIT;
}

static void
ide_langserv_client_call_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(JsonNode) return_value = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  IDE_ENTRY;

  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!jsonrpc_client_call_finish (client, result, &return_value, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&return_value), (GDestroyNotify)json_node_unref);

  IDE_EXIT;
}

/**
 * ide_langserv_client_call_async:
 * @self: An #IdeLangservClient
 * @method: the method to call
 * @params: (nullable) (transfer full): An #JsonNode or %NULL
 * @cancellable: (nullable): A cancellable or %NULL
 * @callback: the callback to receive the result, or %NULL
 * @user_data: user data for @callback
 *
 * Asynchronously queries the Language Server using the JSON-RPC protocol.
 */
void
ide_langserv_client_call_async (IdeLangservClient   *self,
                                const gchar         *method,
                                JsonNode            *params,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_client_call_async);

  if (priv->rpc_client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "No connection to language server");
      IDE_EXIT;
    }

  jsonrpc_client_call_async (priv->rpc_client,
                             method,
                             params,
                             cancellable,
                             ide_langserv_client_call_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_langserv_client_call_finish (IdeLangservClient  *self,
                                 GAsyncResult       *result,
                                 JsonNode          **return_value,
                                 GError            **error)
{
  g_autoptr(JsonNode) local_return_value = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LANGSERV_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_return_value = g_task_propagate_pointer (G_TASK (result), error);
  ret = local_return_value != NULL;

  if (return_value != NULL)
    *return_value = g_steal_pointer (&local_return_value);

  IDE_RETURN (ret);
}

static void
ide_langserv_client_notification_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  JsonrpcClient *client = (JsonrpcClient *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  if (!jsonrpc_client_notification_finish (client, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

/**
 * ide_langserv_client_notification_async:
 * @self: An #IdeLangservClient
 * @method: the method to notification
 * @params: (nullable) (transfer full): An #JsonNode or %NULL
 * @cancellable: (nullable): A cancellable or %NULL
 * @notificationback: the notificationback to receive the result, or %NULL
 * @user_data: user data for @notificationback
 *
 * Asynchronously sends a notification to the Language Server.
 */
void
ide_langserv_client_notification_async (IdeLangservClient   *self,
                                        const gchar         *method,
                                        JsonNode            *params,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  notificationback,
                                        gpointer             user_data)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(GTask) task = NULL;

  IDE_ENTRY;

  task = g_task_new (self, cancellable, notificationback, user_data);
  g_task_set_source_tag (task, ide_langserv_client_notification_async);

  if (priv->rpc_client == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_CONNECTED,
                               "No connection to language server");
      IDE_EXIT;
    }

  jsonrpc_client_notification_async (priv->rpc_client,
                                     method,
                                     params,
                                     cancellable,
                                     ide_langserv_client_notification_cb,
                                     g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
ide_langserv_client_notification_finish (IdeLangservClient  *self,
                                         GAsyncResult       *result,
                                         GError            **error)
{
  g_autoptr(JsonNode) local_return_value = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_LANGSERV_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

void
ide_langserv_client_get_diagnostics_async (IdeLangservClient   *self,
                                           GFile               *file,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);
  g_autoptr(GTask) task = NULL;
  IdeDiagnostics *diagnostics;

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_langserv_client_get_diagnostics_async);

  diagnostics = g_hash_table_lookup (priv->diagnostics_by_file, file);

  if (diagnostics != NULL)
    g_task_return_pointer (task,
                           ide_diagnostics_ref (diagnostics),
                           (GDestroyNotify)ide_diagnostics_unref);
  else
    g_task_return_pointer (task, NULL, NULL);
}

/**
 * ide_langserv_client_get_diagnostics_finish:
 * @self: A #IdeLangservClient
 * @result: A #GAsyncResult
 * @diagnostics: (nullable) (out): A location for a #IdeDiagnostics or %NULL
 * @error: A location for a #GError or %NULL
 *
 * Completes a request to ide_langserv_client_get_diagnostics_async().
 *
 * Returns: %TRUE if successful and @diagnostics is set, otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_langserv_client_get_diagnostics_finish (IdeLangservClient  *self,
                                            GAsyncResult       *result,
                                            IdeDiagnostics    **diagnostics,
                                            GError            **error)
{
  g_autoptr(IdeDiagnostics) local_diagnostics = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean ret;

  g_return_val_if_fail (IDE_IS_LANGSERV_CLIENT (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  local_diagnostics = g_task_propagate_pointer (G_TASK (result), &local_error);
  ret = local_error == NULL;

  if (ret == TRUE && local_diagnostics == NULL)
    local_diagnostics = ide_diagnostics_new (NULL);

  if (local_diagnostics != NULL && diagnostics != NULL)
    *diagnostics = g_steal_pointer (&local_diagnostics);

  if (local_error)
    g_propagate_error (error, local_error);

  return ret;
}

void
ide_langserv_client_add_language (IdeLangservClient *self,
                                  const gchar       *language_id)
{
  IdeLangservClientPrivate *priv = ide_langserv_client_get_instance_private (self);

  g_return_if_fail (IDE_IS_LANGSERV_CLIENT (self));
  g_return_if_fail (language_id != NULL);

  g_ptr_array_add (priv->languages, g_strdup (language_id));
}
