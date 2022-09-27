/* ide-lsp-formatter.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-lsp-formatter"

#include "config.h"

#include <jsonrpc-glib.h>

#include <libide-code.h>
#include <libide-threading.h>

#include "ide-lsp-formatter.h"

typedef struct
{
  IdeLspClient *client;
} IdeLspFormatterPrivate;

enum {
  PROP_0,
  PROP_CLIENT,
  N_PROPS
};

static void formatter_iface_init (IdeFormatterInterface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeLspFormatter, ide_lsp_formatter, IDE_TYPE_OBJECT,
                         G_ADD_PRIVATE (IdeLspFormatter)
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, formatter_iface_init))

static GParamSpec *properties [N_PROPS];

/**
 * ide_lsp_formatter_get_client:
 * @self: a #IdeLspFormatter
 *
 * Gets the client to use for the formatter.
 *
 * Returns: (transfer none): An #IdeLspClient or %NULL.
 */
IdeLspClient *
ide_lsp_formatter_get_client (IdeLspFormatter *self)
{
  IdeLspFormatterPrivate *priv = ide_lsp_formatter_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LSP_FORMATTER (self), NULL);

  return priv->client;
}

void
ide_lsp_formatter_set_client (IdeLspFormatter *self,
                              IdeLspClient    *client)
{
  IdeLspFormatterPrivate *priv = ide_lsp_formatter_get_instance_private (self);

  g_return_if_fail (IDE_IS_LSP_FORMATTER (self));

  if (g_set_object (&priv->client, client))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CLIENT]);
}

static void
ide_lsp_formatter_finalize (GObject *object)
{
  IdeLspFormatter *self = (IdeLspFormatter *)object;
  IdeLspFormatterPrivate *priv = ide_lsp_formatter_get_instance_private (self);

  g_clear_object (&priv->client);

  G_OBJECT_CLASS (ide_lsp_formatter_parent_class)->finalize (object);
}

static void
ide_lsp_formatter_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeLspFormatter *self = IDE_LSP_FORMATTER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      g_value_set_object (value, ide_lsp_formatter_get_client (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_formatter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeLspFormatter *self = IDE_LSP_FORMATTER (object);

  switch (prop_id)
    {
    case PROP_CLIENT:
      ide_lsp_formatter_set_client (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_lsp_formatter_class_init (IdeLspFormatterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_lsp_formatter_finalize;
  object_class->get_property = ide_lsp_formatter_get_property;
  object_class->set_property = ide_lsp_formatter_set_property;

  properties [PROP_CLIENT] =
    g_param_spec_object ("client",
                         "Client",
                         "The client to communicate over",
                         IDE_TYPE_LSP_CLIENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_lsp_formatter_init (IdeLspFormatter *self)
{
}

static void
ide_lsp_formatter_apply_changes (IdeLspFormatter *self,
                                 IdeBuffer       *buffer,
                                 GVariant        *text_edits)
{
  g_autoptr(GPtrArray) edits = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeBufferManager *buffer_manager;
  GVariant *text_edit;
  GFile *file;
  GVariantIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_FORMATTER (self));
  g_assert (text_edits != NULL);

  /* We use "mv" to signify null/empty-set/nothing */
  if (g_variant_is_of_type (text_edits, G_VARIANT_TYPE ("mv")))
    IDE_EXIT;

  /* We use "av" which is really "a<a{sv}>" */
  if (!g_variant_is_of_type (text_edits, G_VARIANT_TYPE ("av")))
    {
      g_warning ("Unexpected result of type %s for text edits",
                 g_variant_get_type_string (text_edits));
      IDE_EXIT;
    }

  file = ide_buffer_get_file (buffer);
  edits = g_ptr_array_new_with_free_func (g_object_unref);

  g_variant_iter_init (&iter, text_edits);

  while (g_variant_iter_loop (&iter, "v", &text_edit))
    {
      g_autoptr(IdeLocation) begin_location = NULL;
      g_autoptr(IdeLocation) end_location = NULL;
      g_autoptr(IdeRange) range = NULL;
      const gchar *new_text = NULL;
      gboolean success;
      struct {
        gint64 line;
        gint64 column;
      } begin, end;

      success = JSONRPC_MESSAGE_PARSE (text_edit,
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
        "newText", JSONRPC_MESSAGE_GET_STRING (&new_text)
      );

      if (!success)
        {
          IDE_TRACE_MSG ("Failed to extract change from variant");
          continue;
        }

      begin_location = ide_location_new (file, begin.line, begin.column);
      end_location = ide_location_new (file, end.line, end.column);
      range = ide_range_new (begin_location, end_location);

      g_ptr_array_add (edits, ide_text_edit_new (range, new_text));
    }

  context = ide_buffer_ref_context (buffer);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_apply_edits_async (buffer_manager,
                                        IDE_PTR_ARRAY_STEAL_FULL (&edits),
                                        NULL, NULL, NULL);

  IDE_EXIT;
}

static void
ide_lsp_formatter_format_call_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeLspClient *client = (IdeLspClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  IdeLspFormatter *self;
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_LSP_CLIENT (client));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));

  if (!ide_lsp_client_call_finish (client, result, &reply, &error))
    {
      g_debug ("Failed to format selection: %s", error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  buffer = ide_task_get_task_data (task);

  g_assert (IDE_IS_LSP_FORMATTER (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_lsp_formatter_apply_changes (self, buffer, reply);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_lsp_formatter_format_async (IdeFormatter        *formatter,
                                IdeBuffer           *buffer,
                                IdeFormatterOptions *options,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IdeLspFormatter *self = (IdeLspFormatter *)formatter;
  IdeLspFormatterPrivate *priv = ide_lsp_formatter_get_instance_private (self);
  g_autoptr(GVariant) params = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *text = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  gint64 version;
  gint tab_size;
  gboolean insert_spaces;

  IDE_ENTRY;

  g_assert (IDE_IS_LSP_FORMATTER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_formatter_format_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  if (priv->client == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_CONNECTED,
                                 "No language server connected");
      IDE_EXIT;
    }

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_iter_order (&begin, &end);

  version = ide_buffer_get_change_count (buffer);
  uri = ide_buffer_dup_uri (buffer);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);

  tab_size = ide_formatter_options_get_tab_width (options);
  insert_spaces = ide_formatter_options_get_insert_spaces (options);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "text", JSONRPC_MESSAGE_PUT_STRING (text),
      "version", JSONRPC_MESSAGE_PUT_INT64 (version),
    "}",
    "options", "{",
      "tabSize", JSONRPC_MESSAGE_PUT_INT32 (tab_size),
      "insertSpaces", JSONRPC_MESSAGE_PUT_BOOLEAN (insert_spaces),
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/formatting",
                             params,
                             cancellable,
                             ide_lsp_formatter_format_call_cb,
                             g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_lsp_formatter_format_finish (IdeFormatter  *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_lsp_formatter_format_range_async (IdeFormatter        *formatter,
                                      IdeBuffer           *buffer,
                                      IdeFormatterOptions *options,
                                      const GtkTextIter   *begin,
                                      const GtkTextIter   *end,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  IdeLspFormatter *self = (IdeLspFormatter *)formatter;
  IdeLspFormatterPrivate *priv = ide_lsp_formatter_get_instance_private (self);
  g_autoptr(GVariant) params = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *text = NULL;
  gint64 version;
  gint tab_size;
  gboolean insert_spaces;
  struct {
    gint line;
    gint character;
  } b, e;

  g_assert (IDE_IS_LSP_FORMATTER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_lsp_formatter_format_range_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  if (gtk_text_iter_compare (begin, end) > 0)
    {
      const GtkTextIter *tmp = end;
      end = begin;
      begin = tmp;
    }

  version = ide_buffer_get_change_count (buffer);
  uri = ide_buffer_dup_uri (buffer);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), begin, end, TRUE);

  tab_size = ide_formatter_options_get_tab_width (options);
  insert_spaces = ide_formatter_options_get_insert_spaces (options);

  b.line = gtk_text_iter_get_line (begin);
  b.character = gtk_text_iter_get_line_offset (begin);

  e.line = gtk_text_iter_get_line (end);
  e.character = gtk_text_iter_get_line_offset (begin);

  params = JSONRPC_MESSAGE_NEW (
    "textDocument", "{",
      "uri", JSONRPC_MESSAGE_PUT_STRING (uri),
      "text", JSONRPC_MESSAGE_PUT_STRING (text),
      "version", JSONRPC_MESSAGE_PUT_INT64 (version),
    "}",
    "options", "{",
      "tabSize", JSONRPC_MESSAGE_PUT_INT32 (tab_size),
      "insertSpaces", JSONRPC_MESSAGE_PUT_BOOLEAN (insert_spaces),
    "}",
    "range", "{",
      "start", "{",
        "line", JSONRPC_MESSAGE_PUT_INT32 (b.line),
        "character", JSONRPC_MESSAGE_PUT_INT32 (b.character),
      "}",
      "end", "{",
        "line", JSONRPC_MESSAGE_PUT_INT32 (e.line),
        "character", JSONRPC_MESSAGE_PUT_INT32 (e.character),
      "}",
    "}"
  );

  ide_lsp_client_call_async (priv->client,
                             "textDocument/rangeFormatting",
                             params,
                             cancellable,
                             ide_lsp_formatter_format_call_cb,
                             g_steal_pointer (&task));
}

static gboolean
ide_lsp_formatter_format_range_finish (IdeFormatter  *self,
                                       GAsyncResult  *result,
                                       GError       **error)
{
  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->format_async = ide_lsp_formatter_format_async;
  iface->format_finish = ide_lsp_formatter_format_finish;
  iface->format_range_async = ide_lsp_formatter_format_range_async;
  iface->format_range_finish = ide_lsp_formatter_format_range_finish;
}
