/* gbp-acp-jsonrpc.c
 *
 * Copyright 2026 GNOME Builder contributors
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

#define G_LOG_DOMAIN "gbp-acp-jsonrpc"

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "gbp-acp-jsonrpc.h"

/*
 * The Agent Client Protocol transports JSON-RPC 2.0 as newline-delimited
 * JSON over stdio. That differs from the LSP Content-Length framing provided
 * by jsonrpc-glib, so we provide a small transport here which reads and
 * writes one JSON document per line.
 *
 * Messages are converted to/from GVariant so that the rest of the plugin can
 * work with the same {a{sv}}/av representation used elsewhere in Builder.
 */

struct _GbpAcpJsonrpc
{
  GObject          parent_instance;
  GDataInputStream *input;
  GOutputStream    *output;
  GCancellable     *cancellable;
  GHashTable       *pending;
  guint64           next_id;
  gboolean          closed;
};

enum
{
  SIGNAL_NOTIFICATION,
  SIGNAL_HANDLE_CALL,
  SIGNAL_FAILED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE (GbpAcpJsonrpc, gbp_acp_jsonrpc, G_TYPE_OBJECT)

static JsonNode *variant_to_json_node (GVariant *value);

static JsonNode *
variant_to_json_node (GVariant *value)
{
  JsonNode *node;

  if (value == NULL)
    return json_node_new (JSON_NODE_NULL);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_VARIANT))
    {
      g_autoptr(GVariant) child = g_variant_get_variant (value);
      return variant_to_json_node (child);
    }

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_VARDICT))
    {
      JsonObject *object = json_object_new ();
      GVariantIter iter;
      GVariant *child;
      const gchar *key;

      g_variant_iter_init (&iter, value);

      while (g_variant_iter_loop (&iter, "{&sv}", &key, &child))
        json_object_set_member (object, key, variant_to_json_node (child));

      node = json_node_new (JSON_NODE_OBJECT);
      json_node_take_object (node, object);

      return node;
    }

  if (g_variant_is_of_type (value, G_VARIANT_TYPE ("av")))
    {
      JsonArray *array = json_array_new ();
      GVariantIter iter;
      GVariant *child;

      g_variant_iter_init (&iter, value);

      while (g_variant_iter_loop (&iter, "v", &child))
        json_array_add_element (array, variant_to_json_node (child));

      node = json_node_new (JSON_NODE_ARRAY);
      json_node_take_array (node, array);

      return node;
    }

  node = json_node_new (JSON_NODE_VALUE);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    json_node_set_string (node, g_variant_get_string (value, NULL));
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
    json_node_set_boolean (node, g_variant_get_boolean (value));
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE))
    json_node_set_double (node, g_variant_get_double (value));
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    json_node_set_int (node, g_variant_get_int64 (value));
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT32))
    json_node_set_int (node, g_variant_get_int32 (value));
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT64))
    json_node_set_int (node, (gint64)g_variant_get_uint64 (value));
  else
    json_node_set_string (node, "");

  return node;
}

static GVariant *
json_node_to_variant (JsonNode *node)
{
  if (node == NULL)
    return g_variant_new_maybe (G_VARIANT_TYPE_STRING, NULL);

  switch (json_node_get_node_type (node))
    {
    case JSON_NODE_OBJECT:
      {
        JsonObject *object = json_node_get_object (node);
        GList *members;
        GVariantBuilder builder;
        GList *iter;

        g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
        members = json_object_get_members (object);

        for (iter = members; iter != NULL; iter = iter->next)
          {
            const gchar *key = iter->data;
            JsonNode *member = json_object_get_member (object, key);
            g_variant_builder_add (&builder, "{sv}", key, json_node_to_variant (member));
          }

        g_list_free (members);

        return g_variant_builder_end (&builder);
      }

    case JSON_NODE_ARRAY:
      {
        JsonArray *array = json_node_get_array (node);
        GVariantBuilder builder;
        guint length;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));
        length = json_array_get_length (array);

        for (guint i = 0; i < length; i++)
          g_variant_builder_add (&builder, "v", json_node_to_variant (json_array_get_element (array, i)));

        return g_variant_builder_end (&builder);
      }

    case JSON_NODE_VALUE:
      {
        GType type = json_node_get_value_type (node);

        if (type == G_TYPE_STRING)
          return g_variant_new_string (json_node_get_string (node));
        if (type == G_TYPE_DOUBLE)
          return g_variant_new_double (json_node_get_double (node));
        if (type == G_TYPE_BOOLEAN)
          return g_variant_new_boolean (json_node_get_boolean (node));
        if (type == G_TYPE_INT64 || type == G_TYPE_INT)
          return g_variant_new_int64 (json_node_get_int (node));

        return g_variant_new_string (json_node_get_string (node) ?: "");
      }

    case JSON_NODE_NULL:
    default:
      return g_variant_new_maybe (G_VARIANT_TYPE_STRING, NULL);
    }
}

static GVariant *
member_to_variant (JsonObject  *object,
                   const gchar *member)
{
  JsonNode *node;

  if (!json_object_has_member (object, member))
    return NULL;

  node = json_object_get_member (object, member);

  if (node == NULL || json_node_get_node_type (node) == JSON_NODE_NULL)
    return NULL;

  /* Ensure the returned value is non-floating so the caller owns a strong
   * reference. Otherwise g_signal_emit() would consume it via
   * g_value_set_variant() and our own unref would be a use-after-free.
   */
  return g_variant_ref_sink (json_node_to_variant (node));
}

static void
gbp_acp_jsonrpc_write_object (GbpAcpJsonrpc *self,
                              JsonObject    *object)
{
  g_autoptr(JsonGenerator) generator = NULL;
  g_autoptr(JsonNode) node = NULL;
  g_autofree gchar *data = NULL;
  gsize length = 0;

  g_assert (GBP_IS_ACP_JSONRPC (self));
  g_assert (object != NULL);

  node = json_node_new (JSON_NODE_OBJECT);
  json_node_take_object (node, object);

  generator = json_generator_new ();
  json_generator_set_root (generator, node);
  data = json_generator_to_data (generator, &length);

  if (data != NULL)
    {
      gsize written = 0;

      if (g_output_stream_write_all (self->output, data, length, &written, NULL, NULL))
        g_output_stream_write_all (self->output, "\n", 1, NULL, NULL, NULL);

      g_output_stream_flush (self->output, NULL, NULL);
    }
}

static void
gbp_acp_jsonrpc_fail_pending (GbpAcpJsonrpc *self,
                              const GError  *error)
{
  GHashTableIter iter;
  gpointer value;

  g_hash_table_iter_init (&iter, self->pending);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      GTask *task = value;

      g_task_return_error (task, g_error_copy (error));
      g_hash_table_iter_remove (&iter);
    }
}

static void
gbp_acp_jsonrpc_handle_response (GbpAcpJsonrpc *self,
                                 JsonObject    *object)
{
  GTask *task;
  gint64 id;
  gint64 *key;

  if (!json_object_has_member (object, "id"))
    return;

  id = json_object_get_int_member (object, "id");
  key = &id;

  if ((task = g_hash_table_lookup (self->pending, key)) == NULL)
    return;

  g_object_ref (task);
  g_hash_table_remove (self->pending, key);

  if (json_object_has_member (object, "error"))
    {
      JsonObject *error_object = json_object_get_object_member (object, "error");
      const gchar *message = NULL;

      if (error_object != NULL &&
          json_object_has_member (error_object, "message"))
        message = json_object_get_string_member (error_object, "message");

      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "%s", message ?: _("ACP request failed"));
    }
  else
    {
      GVariant *result = member_to_variant (object, "result");
      g_task_return_pointer (task, result, (GDestroyNotify)g_variant_unref);
    }

  g_object_unref (task);
}

static void
gbp_acp_jsonrpc_handle_request (GbpAcpJsonrpc *self,
                                JsonObject    *object)
{
  const gchar *method = NULL;
  GVariant *id_variant;
  GVariant *params;
  gboolean handled = FALSE;
  JsonNode *id_node;

  if (!json_object_has_member (object, "method"))
    return;

  method = json_object_get_string_member (object, "method");
  params = member_to_variant (object, "params");

  id_node = json_object_get_member (object, "id");

  if (id_node != NULL && json_node_get_node_type (id_node) != JSON_NODE_NULL)
    {
      if (json_node_get_value_type (id_node) == G_TYPE_STRING)
        id_variant = g_variant_ref_sink (g_variant_new_string (json_node_get_string (id_node)));
      else
        id_variant = g_variant_ref_sink (g_variant_new_int64 (json_node_get_int (id_node)));

      g_signal_emit (self, signals[SIGNAL_HANDLE_CALL], 0,
                     method, id_variant, params, &handled);

      if (!handled)
        gbp_acp_jsonrpc_reply_error (self, id_variant,
                                     -32601, _("Method not found"));

      g_clear_pointer (&id_variant, g_variant_unref);
    }
  else
    {
      g_signal_emit (self, signals[SIGNAL_NOTIFICATION], 0, method, params);
    }

  g_clear_pointer (&params, g_variant_unref);
}

static void
gbp_acp_jsonrpc_handle_line (GbpAcpJsonrpc *self,
                             const gchar   *line)
{
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  JsonNode *root;
  JsonObject *object;

  if (line == NULL || *line == '\0')
    return;

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, line, -1, &error))
    {
      g_debug ("Failed to parse ACP message: %s", error->message);
      return;
    }

  root = json_parser_get_root (parser);

  if (root == NULL || json_node_get_node_type (root) != JSON_NODE_OBJECT)
    return;

  object = json_node_get_object (root);

  if (json_object_has_member (object, "method"))
    gbp_acp_jsonrpc_handle_request (self, object);
  else
    gbp_acp_jsonrpc_handle_response (self, object);
}

static void
gbp_acp_jsonrpc_read_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(GbpAcpJsonrpc) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *line = NULL;
  gsize length = 0;

  g_assert (GBP_IS_ACP_JSONRPC (self));

  line = g_data_input_stream_read_line_finish_utf8 (G_DATA_INPUT_STREAM (object),
                                                    result, &length, &error);

  if (line == NULL)
    {
      if (!self->closed && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_autoptr(GError) failed = NULL;

          failed = error != NULL ? g_error_copy (error)
                                 : g_error_new_literal (G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
                                                        _("ACP connection closed"));
          g_signal_emit (self, signals[SIGNAL_FAILED], 0, failed);
        }
      return;
    }

  gbp_acp_jsonrpc_handle_line (self, line);

  if (!self->closed)
    g_data_input_stream_read_line_async (self->input,
                                         G_PRIORITY_DEFAULT,
                                         self->cancellable,
                                         gbp_acp_jsonrpc_read_cb,
                                         g_object_ref (self));
}

static void
gbp_acp_jsonrpc_dispose (GObject *object)
{
  GbpAcpJsonrpc *self = (GbpAcpJsonrpc *)object;

  gbp_acp_jsonrpc_close (self);

  G_OBJECT_CLASS (gbp_acp_jsonrpc_parent_class)->dispose (object);
}

static void
gbp_acp_jsonrpc_finalize (GObject *object)
{
  GbpAcpJsonrpc *self = (GbpAcpJsonrpc *)object;

  g_clear_object (&self->input);
  g_clear_object (&self->output);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->pending, g_hash_table_unref);

  G_OBJECT_CLASS (gbp_acp_jsonrpc_parent_class)->finalize (object);
}

static void
gbp_acp_jsonrpc_class_init (GbpAcpJsonrpcClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_acp_jsonrpc_dispose;
  object_class->finalize = gbp_acp_jsonrpc_finalize;

  signals[SIGNAL_NOTIFICATION] =
    g_signal_new ("notification",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VARIANT);

  signals[SIGNAL_HANDLE_CALL] =
    g_signal_new ("handle-call",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_BOOLEAN, 3, G_TYPE_STRING, G_TYPE_VARIANT, G_TYPE_VARIANT);

  signals[SIGNAL_FAILED] =
    g_signal_new ("failed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_ERROR);
}

static void
gbp_acp_jsonrpc_init (GbpAcpJsonrpc *self)
{
  self->pending = g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, g_object_unref);
  self->next_id = 0;
}

GbpAcpJsonrpc *
gbp_acp_jsonrpc_new (GInputStream  *input,
                     GOutputStream *output)
{
  GbpAcpJsonrpc *self;

  g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (output), NULL);

  self = g_object_new (GBP_TYPE_ACP_JSONRPC, NULL);
  self->input = g_data_input_stream_new (input);
  self->output = g_object_ref (output);
  self->cancellable = g_cancellable_new ();

  return self;
}

void
gbp_acp_jsonrpc_start (GbpAcpJsonrpc *self)
{
  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));

  if (self->closed)
    return;

  g_data_input_stream_read_line_async (self->input,
                                       G_PRIORITY_DEFAULT,
                                       self->cancellable,
                                       gbp_acp_jsonrpc_read_cb,
                                       g_object_ref (self));
}

void
gbp_acp_jsonrpc_close (GbpAcpJsonrpc *self)
{
  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));

  if (self->closed)
    return;

  self->closed = TRUE;

  g_cancellable_cancel (self->cancellable);

  if (g_hash_table_size (self->pending) > 0)
    {
      g_autoptr(GError) error = NULL;

      error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_CANCELLED,
                                   _("ACP connection closed"));
      gbp_acp_jsonrpc_fail_pending (self, error);
    }
}

void
gbp_acp_jsonrpc_call_async (GbpAcpJsonrpc       *self,
                            const gchar         *method,
                            GVariant            *params,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr(JsonObject) object = NULL;
  g_autoptr(GTask) task = NULL;
  gint64 id;
  gint64 *key;

  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));
  g_return_if_fail (method != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_acp_jsonrpc_call_async);

  if (self->closed)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CLOSED,
                               _("ACP connection is closed"));
      return;
    }

  id = (gint64)++self->next_id;
  key = g_new (gint64, 1);
  *key = id;
  g_hash_table_insert (self->pending, key, g_object_ref (task));

  object = json_object_new ();
  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_int_member (object, "id", id);
  json_object_set_string_member (object, "method", method);

  if (params != NULL)
    json_object_set_member (object, "params", variant_to_json_node (params));

  gbp_acp_jsonrpc_write_object (self, g_steal_pointer (&object));
}

GVariant *
gbp_acp_jsonrpc_call_finish (GbpAcpJsonrpc  *self,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_return_val_if_fail (GBP_IS_ACP_JSONRPC (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
gbp_acp_jsonrpc_send_notification (GbpAcpJsonrpc *self,
                                   const gchar   *method,
                                   GVariant      *params)
{
  g_autoptr(JsonObject) object = NULL;

  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));
  g_return_if_fail (method != NULL);

  if (self->closed)
    return;

  object = json_object_new ();
  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_string_member (object, "method", method);

  if (params != NULL)
    json_object_set_member (object, "params", variant_to_json_node (params));

  gbp_acp_jsonrpc_write_object (self, g_steal_pointer (&object));
}

void
gbp_acp_jsonrpc_reply (GbpAcpJsonrpc *self,
                       GVariant      *id,
                       GVariant      *result)
{
  g_autoptr(JsonObject) object = NULL;

  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));
  g_return_if_fail (id != NULL);

  if (self->closed)
    return;

  object = json_object_new ();
  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_member (object, "id", variant_to_json_node (id));

  if (result != NULL)
    json_object_set_member (object, "result", variant_to_json_node (result));
  else
    json_object_set_null_member (object, "result");

  gbp_acp_jsonrpc_write_object (self, g_steal_pointer (&object));
}

void
gbp_acp_jsonrpc_reply_error (GbpAcpJsonrpc *self,
                             GVariant      *id,
                             gint           code,
                             const gchar   *message)
{
  g_autoptr(JsonObject) object = NULL;
  JsonObject *error_object;

  g_return_if_fail (GBP_IS_ACP_JSONRPC (self));
  g_return_if_fail (id != NULL);

  if (self->closed)
    return;

  object = json_object_new ();
  json_object_set_string_member (object, "jsonrpc", "2.0");
  json_object_set_member (object, "id", variant_to_json_node (id));

  error_object = json_object_new ();
  json_object_set_int_member (error_object, "code", code);
  json_object_set_string_member (error_object, "message", message ?: "");
  json_object_set_object_member (object, "error", error_object);

  gbp_acp_jsonrpc_write_object (self, g_steal_pointer (&object));
}
