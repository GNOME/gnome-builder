/* gbp-acp-client.c
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

#define G_LOG_DOMAIN "gbp-acp-client"

#include "config.h"

#include <glib/gi18n.h>
#include <jsonrpc-glib.h>

#include <libide-core.h>
#include <libide-threading.h>

#include "gbp-acp-client.h"
#include "gbp-acp-jsonrpc.h"
#include "gbp-acp-terminal.h"

#define ACP_PROTOCOL_VERSION 1
#define ACP_DEFAULT_COMMAND "opencode"

typedef enum
{
  GBP_ACP_STATE_DISCONNECTED,
  GBP_ACP_STATE_INITIALIZING,
  GBP_ACP_STATE_CREATING_SESSION,
  GBP_ACP_STATE_READY,
  GBP_ACP_STATE_CLOSED,
} GbpAcpState;

struct _GbpAcpClient
{
  GObject        parent_instance;

  IdeContext    *context;
  GFile         *workdir;
  GbpAcpJsonrpc *rpc;
  IdeSubprocess *subprocess;
  GCancellable  *cancellable;
  GHashTable    *terminals;

  GVariant      *config_options;
  gchar         *session_id;
  gchar         *agent_name;

  GTask         *prompt_task;
  GVariant      *permission_id;

  GbpAcpState    state;
};

enum
{
  SIGNAL_UPDATE,
  SIGNAL_PERMISSION,
  SIGNAL_READY,
  SIGNAL_FAILED,
  SIGNAL_EXITED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

enum
{
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GbpAcpClient, gbp_acp_client, G_TYPE_OBJECT)

static void gbp_acp_client_on_initialize_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data);
static void gbp_acp_client_on_session_new_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data);

static gchar **
variant_dup_strv (GVariant *array)
{
  g_autoptr(GVariantIter) iter = NULL;
  GPtrArray *ar;
  GVariant *child;

  if (array == NULL || !g_variant_is_of_type (array, G_VARIANT_TYPE ("av")))
    return NULL;

  ar = g_ptr_array_new_with_free_func (g_free);
  iter = g_variant_iter_new (array);

  while (g_variant_iter_loop (iter, "v", &child))
    {
      if (g_variant_is_of_type (child, G_VARIANT_TYPE_STRING))
        g_ptr_array_add (ar, g_variant_dup_string (child, NULL));
    }

  g_ptr_array_add (ar, NULL);

  return (gchar **)g_ptr_array_free (ar, FALSE);
}

static gchar *
slice_lines (const gchar *contents,
             gint64       line,
             gint64       limit)
{
  g_auto(GStrv) lines = NULL;
  GString *str;
  guint n_lines;
  guint start;
  guint count;

  if (line <= 1 && limit <= 0)
    return g_strdup (contents);

  lines = g_strsplit (contents, "\n", -1);
  n_lines = g_strv_length (lines);
  start = line > 1 ? MIN ((guint)(line - 1), n_lines) : 0;
  count = limit > 0 ? MIN ((guint)limit, n_lines - start) : (n_lines - start);

  str = g_string_new (NULL);
  for (guint i = start; i < start + count; i++)
    {
      if (i != start)
        g_string_append_c (str, '\n');
      g_string_append (str, lines[i]);
    }

  return g_string_free (str, FALSE);
}

static GVariant *
build_exit_status (GbpAcpTerminal *terminal)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  if (gbp_acp_terminal_get_exited (terminal))
    {
      g_variant_builder_add (&builder, "{sv}", "exitCode",
                             g_variant_new_int32 (gbp_acp_terminal_get_exit_code (terminal)));
      g_variant_builder_add (&builder, "{sv}", "signal",
                             g_variant_new_string (gbp_acp_terminal_get_signal (terminal) ?: ""));
    }

  return g_variant_builder_end (&builder);
}

static void
gbp_acp_client_set_state (GbpAcpClient *self,
                          GbpAcpState   state)
{
  self->state = state;
}

static void
gbp_acp_client_fail (GbpAcpClient *self,
                     const GError *error)
{
  g_autoptr(GError) copy = NULL;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));

  copy = error != NULL ? g_error_copy (error) : NULL;

  if (copy != NULL)
    g_warning ("ACP agent failed: %s", copy->message);

  g_signal_emit (self, signals[SIGNAL_FAILED], 0, copy);
  gbp_acp_client_stop (self);
}

static void
gbp_acp_client_send_initialize (GbpAcpClient *self)
{
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "protocolVersion", JSONRPC_MESSAGE_PUT_INT64 (ACP_PROTOCOL_VERSION),
    "clientCapabilities", "{",
      "fs", "{",
        "readTextFile", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
        "writeTextFile", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
      "}",
      "terminal", JSONRPC_MESSAGE_PUT_BOOLEAN (TRUE),
      "session", "{",
        "configOptions", "{",
          "boolean", "{",
          "}",
        "}",
      "}",
    "}",
    "clientInfo", "{",
      "name", JSONRPC_MESSAGE_PUT_STRING ("gnome-builder"),
      "title", JSONRPC_MESSAGE_PUT_STRING (_("GNOME Builder")),
      "version", JSONRPC_MESSAGE_PUT_STRING (PACKAGE_VERSION),
    "}");

  gbp_acp_client_set_state (self, GBP_ACP_STATE_INITIALIZING);

  gbp_acp_jsonrpc_call_async (self->rpc,
                             "initialize",
                             params,
                             self->cancellable,
                             gbp_acp_client_on_initialize_cb,
                             g_object_ref (self));
}

static void
gbp_acp_client_send_new_session (GbpAcpClient *self)
{
  g_autoptr(GVariant) params = NULL;
  g_autofree gchar *cwd = NULL;

  cwd = g_file_get_path (self->workdir);

  params = JSONRPC_MESSAGE_NEW (
    "cwd", JSONRPC_MESSAGE_PUT_STRING (cwd),
    "mcpServers", "[", "]");

  gbp_acp_client_set_state (self, GBP_ACP_STATE_CREATING_SESSION);

  gbp_acp_jsonrpc_call_async (self->rpc,
                             "session/new",
                             params,
                             self->cancellable,
                             gbp_acp_client_on_session_new_cb,
                             g_object_ref (self));
}

static void
gbp_acp_client_handle_update (GbpAcpClient *self,
                              GVariant     *params)
{
  GVariant *update;

  g_assert (GBP_IS_ACP_CLIENT (self));

  if (params == NULL)
    return;

  update = g_variant_lookup_value (params, "update", G_VARIANT_TYPE_VARDICT);

  if (update != NULL)
    {
      g_signal_emit (self, signals[SIGNAL_UPDATE], 0, update);
      g_variant_unref (update);
    }
}

static void
gbp_acp_client_on_notification (GbpAcpClient  *self,
                                const gchar   *method,
                                GVariant      *params,
                                JsonrpcClient *rpc)
{
  g_assert (GBP_IS_ACP_CLIENT (self));
  g_assert (method != NULL);

  if (g_strcmp0 (method, "session/update") == 0)
    gbp_acp_client_handle_update (self, params);
  else
    g_debug ("Unhandled ACP notification “%s”", method);
}

static void
gbp_acp_client_on_failed (GbpAcpClient  *self,
                          JsonrpcClient *rpc)
{
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_ACP_CLIENT (self));

  error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
                               _("The ACP agent connection was closed"));
  gbp_acp_client_fail (self, error);
}

static void
gbp_acp_client_reply_error (GbpAcpClient *self,
                            GVariant     *id,
                            const gchar  *message)
{
  gbp_acp_jsonrpc_reply_error (self->rpc, id, -32603, message);
}

static gboolean
gbp_acp_client_handle_read_file (GbpAcpClient *self,
                                 GVariant     *id,
                                 GVariant     *params)
{
  g_autoptr(GVariant) reply = NULL;
  g_autofree gchar *contents = NULL;
  g_autofree gchar *sliced = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *path = NULL;
  gint64 line = 0;
  gint64 limit = 0;

  if (!g_variant_lookup (params, "path", "&s", &path) || path == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Missing path");
      return TRUE;
    }

  if (!g_file_get_contents (path, &contents, NULL, &error))
    {
      gbp_acp_client_reply_error (self, id, error->message);
      return TRUE;
    }

  JSONRPC_MESSAGE_PARSE (params, "line", JSONRPC_MESSAGE_GET_INT64 (&line));
  JSONRPC_MESSAGE_PARSE (params, "limit", JSONRPC_MESSAGE_GET_INT64 (&limit));

  sliced = slice_lines (contents, line, limit);
  reply = JSONRPC_MESSAGE_NEW ("content", JSONRPC_MESSAGE_PUT_STRING (sliced));

  gbp_acp_jsonrpc_reply (self->rpc, id, reply);

  return TRUE;
}

static gboolean
gbp_acp_client_handle_write_file (GbpAcpClient *self,
                                  GVariant     *id,
                                  GVariant     *params)
{
  g_autoptr(GError) error = NULL;
  const gchar *path = NULL;
  const gchar *content = NULL;

  if (!g_variant_lookup (params, "path", "&s", &path) || path == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Missing path");
      return TRUE;
    }

  if (!g_variant_lookup (params, "content", "&s", &content))
    content = "";

  if (!g_file_set_contents (path, content, -1, &error))
    {
      gbp_acp_client_reply_error (self, id, error->message);
      return TRUE;
    }

  gbp_acp_jsonrpc_reply (self->rpc, id, NULL);

  return TRUE;
}

static gboolean
gbp_acp_client_handle_permission (GbpAcpClient *self,
                                  GVariant     *id,
                                  GVariant     *params)
{
  GVariant *tool_call;
  GVariant *options;

  g_clear_pointer (&self->permission_id, g_variant_unref);
  self->permission_id = g_variant_ref (id);

  tool_call = g_variant_lookup_value (params, "toolCall", G_VARIANT_TYPE_VARDICT);
  options = g_variant_lookup_value (params, "options", NULL);

  g_signal_emit (self, signals[SIGNAL_PERMISSION], 0, tool_call, options);

  g_clear_pointer (&tool_call, g_variant_unref);
  g_clear_pointer (&options, g_variant_unref);

  return TRUE;
}

static gboolean
gbp_acp_client_handle_terminal_create (GbpAcpClient *self,
                                       GVariant     *id,
                                       GVariant     *params)
{
  g_autoptr(GbpAcpTerminal) terminal = NULL;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) args = NULL;
  g_autoptr(GVariant) env = NULL;
  g_autofree gchar **args_strv = NULL;
  g_autofree gchar **env_names = NULL;
  g_autofree gchar **env_values = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  const gchar *command = NULL;
  const gchar *cwd = NULL;
  gint64 output_byte_limit = 0;
  GVariant *child;
  guint n_env = 0;

  if (!g_variant_lookup (params, "command", "&s", &command) || command == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Missing command");
      return TRUE;
    }

  args = g_variant_lookup_value (params, "args", NULL);
  env = g_variant_lookup_value (params, "env", NULL);
  g_variant_lookup (params, "cwd", "&s", &cwd);
  JSONRPC_MESSAGE_PARSE (params, "outputByteLimit", JSONRPC_MESSAGE_GET_INT64 (&output_byte_limit));

  args_strv = variant_dup_strv (args);

  if (env != NULL && g_variant_is_of_type (env, G_VARIANT_TYPE ("av")))
    {
      guint n = g_variant_n_children (env);
      env_names = g_new0 (gchar *, n + 1);
      env_values = g_new0 (gchar *, n + 1);
      iter = g_variant_iter_new (env);
      while (g_variant_iter_loop (iter, "v", &child))
        {
          const gchar *name = NULL;
          const gchar *value = NULL;

          if (!g_variant_is_of_type (child, G_VARIANT_TYPE_VARDICT))
            continue;
          if (!g_variant_lookup (child, "name", "&s", &name))
            continue;
          g_variant_lookup (child, "value", "&s", &value);

          env_names[n_env] = g_strdup (name);
          env_values[n_env] = g_strdup (value ?: "");
          n_env++;
        }
    }

  terminal = gbp_acp_terminal_new (command,
                                   (const gchar * const *)args_strv,
                                   args_strv != NULL ? g_strv_length (args_strv) : 0,
                                   (const gchar * const *)env_names,
                                   (const gchar * const *)env_values,
                                   n_env,
                                   cwd,
                                   output_byte_limit > 0 ? (guint64)output_byte_limit : 0);
  gbp_acp_terminal_start (terminal);

  reply = JSONRPC_MESSAGE_NEW ("terminalId",
                               JSONRPC_MESSAGE_PUT_STRING (gbp_acp_terminal_get_id (terminal)));

  g_hash_table_insert (self->terminals,
                       g_strdup (gbp_acp_terminal_get_id (terminal)),
                       g_steal_pointer (&terminal));

  gbp_acp_jsonrpc_reply (self->rpc, id, reply);

  return TRUE;
}

static GbpAcpTerminal *
gbp_acp_client_lookup_terminal (GbpAcpClient *self,
                                GVariant     *params)
{
  const gchar *terminal_id = NULL;

  if (params == NULL || !g_variant_lookup (params, "terminalId", "&s", &terminal_id))
    return NULL;

  return g_hash_table_lookup (self->terminals, terminal_id);
}

static gboolean
gbp_acp_client_handle_terminal_output (GbpAcpClient *self,
                                       GVariant     *id,
                                       GVariant     *params)
{
  GbpAcpTerminal *terminal;
  GVariantBuilder builder;

  terminal = gbp_acp_client_lookup_terminal (self, params);

  if (terminal == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Unknown terminal");
      return TRUE;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "output",
                         g_variant_new_string (gbp_acp_terminal_get_output (terminal)));
  g_variant_builder_add (&builder, "{sv}", "truncated",
                         g_variant_new_boolean (gbp_acp_terminal_get_truncated (terminal)));

  if (gbp_acp_terminal_get_exited (terminal))
    g_variant_builder_add (&builder, "{sv}", "exitStatus", build_exit_status (terminal));

  gbp_acp_jsonrpc_reply (self->rpc, id, g_variant_builder_end (&builder));

  return TRUE;
}

typedef struct
{
  GbpAcpClient *self;
  GVariant     *id;
} TerminalWait;

static void
terminal_wait_free (TerminalWait *data)
{
  g_clear_object (&data->self);
  g_clear_pointer (&data->id, g_variant_unref);
  g_free (data);
}

static void
gbp_acp_client_terminal_wait_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  TerminalWait *data = user_data;
  GbpAcpTerminal *terminal = (GbpAcpTerminal *)object;
  g_autoptr(GError) error = NULL;

  if (!gbp_acp_terminal_wait_finish (terminal, result, &error))
    gbp_acp_client_reply_error (data->self, data->id, error->message);
  else
    gbp_acp_jsonrpc_reply (data->self->rpc, data->id, build_exit_status (terminal));

  terminal_wait_free (data);
}

static gboolean
gbp_acp_client_handle_terminal_wait (GbpAcpClient *self,
                                     GVariant     *id,
                                     GVariant     *params)
{
  GbpAcpTerminal *terminal;
  TerminalWait *data;

  terminal = gbp_acp_client_lookup_terminal (self, params);

  if (terminal == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Unknown terminal");
      return TRUE;
    }

  data = g_new0 (TerminalWait, 1);
  data->self = g_object_ref (self);
  data->id = g_variant_ref (id);

  gbp_acp_terminal_wait_async (terminal, NULL, gbp_acp_client_terminal_wait_cb, data);

  return TRUE;
}

static gboolean
gbp_acp_client_handle_terminal_kill (GbpAcpClient *self,
                                     GVariant     *id,
                                     GVariant     *params)
{
  GbpAcpTerminal *terminal = gbp_acp_client_lookup_terminal (self, params);

  if (terminal == NULL)
    {
      gbp_acp_client_reply_error (self, id, "Unknown terminal");
      return TRUE;
    }

  gbp_acp_terminal_kill (terminal);
  gbp_acp_jsonrpc_reply (self->rpc, id, NULL);

  return TRUE;
}

static gboolean
gbp_acp_client_handle_terminal_release (GbpAcpClient *self,
                                        GVariant     *id,
                                        GVariant     *params)
{
  const gchar *terminal_id = NULL;

  if (params != NULL && g_variant_lookup (params, "terminalId", "&s", &terminal_id))
    {
      GbpAcpTerminal *terminal = g_hash_table_lookup (self->terminals, terminal_id);

      if (terminal != NULL)
        {
          gbp_acp_terminal_kill (terminal);
          g_hash_table_remove (self->terminals, terminal_id);
        }
    }

  gbp_acp_jsonrpc_reply (self->rpc, id, NULL);

  return TRUE;
}

static gboolean
gbp_acp_client_on_handle_call (GbpAcpClient  *self,
                               const gchar   *method,
                               GVariant      *id,
                               GVariant      *params,
                               JsonrpcClient *rpc)
{
  g_assert (GBP_IS_ACP_CLIENT (self));
  g_assert (method != NULL);

  if (g_strcmp0 (method, "session/request_permission") == 0)
    return gbp_acp_client_handle_permission (self, id, params);

  if (g_strcmp0 (method, "fs/read_text_file") == 0)
    return gbp_acp_client_handle_read_file (self, id, params);

  if (g_strcmp0 (method, "fs/write_text_file") == 0)
    return gbp_acp_client_handle_write_file (self, id, params);

  if (g_strcmp0 (method, "terminal/create") == 0)
    return gbp_acp_client_handle_terminal_create (self, id, params);

  if (g_strcmp0 (method, "terminal/output") == 0)
    return gbp_acp_client_handle_terminal_output (self, id, params);

  if (g_strcmp0 (method, "terminal/wait_for_exit") == 0)
    return gbp_acp_client_handle_terminal_wait (self, id, params);

  if (g_strcmp0 (method, "terminal/kill") == 0)
    return gbp_acp_client_handle_terminal_kill (self, id, params);

  if (g_strcmp0 (method, "terminal/release") == 0)
    return gbp_acp_client_handle_terminal_release (self, id, params);

  g_debug ("Unhandled ACP call “%s”", method);

  return FALSE;
}

static void
gbp_acp_client_on_subprocess_exited (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  g_autoptr(GbpAcpClient) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_ACP_CLIENT (self));

  ide_subprocess_wait_finish (IDE_SUBPROCESS (object), result, &error);

  g_signal_emit (self, signals[SIGNAL_EXITED], 0);

  if (self->prompt_task != NULL)
    {
      g_autoptr(GTask) task = g_steal_pointer (&self->prompt_task);
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE,
                               _("The ACP agent exited"));
    }
}

static void
gbp_acp_client_on_initialize_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(GbpAcpClient) self = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) agent_info = NULL;
  const gchar *name = NULL;
  gint64 protocol_version = 0;

  g_assert (GBP_IS_ACP_CLIENT (self));

  reply = gbp_acp_jsonrpc_call_finish (GBP_ACP_JSONRPC (object), result, &error);

  if (error != NULL)
    {
      gbp_acp_client_fail (self, error);
      return;
    }

  JSONRPC_MESSAGE_PARSE (reply, "protocolVersion", JSONRPC_MESSAGE_GET_INT64 (&protocol_version));

  if (protocol_version != ACP_PROTOCOL_VERSION)
    {
      g_autoptr(GError) mismatch = NULL;

      mismatch = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                              _("ACP agent uses unsupported protocol version %" G_GINT64_FORMAT),
                              protocol_version);
      gbp_acp_client_fail (self, mismatch);
      return;
    }

  agent_info = g_variant_lookup_value (reply, "agentInfo", G_VARIANT_TYPE_VARDICT);

  if (agent_info != NULL && g_variant_lookup (agent_info, "name", "&s", &name))
    {
      g_free (self->agent_name);
      self->agent_name = g_strdup (name);
    }

  gbp_acp_client_send_new_session (self);
}

static void
gbp_acp_client_on_session_new_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  g_autoptr(GbpAcpClient) self = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) options = NULL;
  const gchar *session_id = NULL;

  g_assert (GBP_IS_ACP_CLIENT (self));

  reply = gbp_acp_jsonrpc_call_finish (GBP_ACP_JSONRPC (object), result, &error);

  if (error != NULL)
    {
      gbp_acp_client_fail (self, error);
      return;
    }

  if (!g_variant_lookup (reply, "sessionId", "&s", &session_id) || session_id == NULL)
    {
      g_autoptr(GError) invalid = NULL;

      invalid = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                     _("ACP agent did not return a session id"));
      gbp_acp_client_fail (self, invalid);
      return;
    }

  g_free (self->session_id);
  self->session_id = g_strdup (session_id);

  options = g_variant_lookup_value (reply, "configOptions", NULL);
  if (options != NULL)
    {
      g_clear_pointer (&self->config_options, g_variant_unref);
      self->config_options = g_variant_ref (options);
    }

  gbp_acp_client_set_state (self, GBP_ACP_STATE_READY);
  g_signal_emit (self, signals[SIGNAL_READY], 0);
}

static void
gbp_acp_client_on_prompt_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GbpAcpClient) self = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = NULL;
  const gchar *stop_reason = NULL;

  g_assert (GBP_IS_ACP_CLIENT (self));

  task = g_steal_pointer (&self->prompt_task);

  reply = gbp_acp_jsonrpc_call_finish (GBP_ACP_JSONRPC (object), result, &error);

  if (error != NULL)
    {
      if (task != NULL)
        g_task_return_error (task, g_steal_pointer (&error));
      else
        g_warning ("ACP prompt failed: %s", error->message);
      return;
    }

  g_variant_lookup (reply, "stopReason", "&s", &stop_reason);

  if (task != NULL)
    g_task_return_pointer (task, g_strdup (stop_reason ?: "end_turn"), g_free);
}

static void
gbp_acp_client_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbpAcpClient *self = GBP_ACP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_acp_client_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbpAcpClient *self = GBP_ACP_CLIENT (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_acp_client_dispose (GObject *object)
{
  GbpAcpClient *self = (GbpAcpClient *)object;

  gbp_acp_client_stop (self);

  G_OBJECT_CLASS (gbp_acp_client_parent_class)->dispose (object);
}

static void
gbp_acp_client_finalize (GObject *object)
{
  GbpAcpClient *self = (GbpAcpClient *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->workdir);
  g_clear_object (&self->rpc);
  g_clear_object (&self->subprocess);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->terminals, g_hash_table_unref);
  g_clear_pointer (&self->config_options, g_variant_unref);
  g_clear_pointer (&self->session_id, g_free);
  g_clear_pointer (&self->agent_name, g_free);
  g_clear_pointer (&self->permission_id, g_variant_unref);
  g_clear_object (&self->prompt_task);

  G_OBJECT_CLASS (gbp_acp_client_parent_class)->finalize (object);
}

static void
gbp_acp_client_class_init (GbpAcpClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_acp_client_dispose;
  object_class->finalize = gbp_acp_client_finalize;
  object_class->set_property = gbp_acp_client_set_property;
  object_class->get_property = gbp_acp_client_get_property;

  properties[PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[SIGNAL_UPDATE] =
    g_signal_new ("update",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_VARIANT);

  signals[SIGNAL_PERMISSION] =
    g_signal_new ("permission",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_VARIANT, G_TYPE_VARIANT);

  signals[SIGNAL_READY] =
    g_signal_new ("ready",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[SIGNAL_FAILED] =
    g_signal_new ("failed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_EXITED] =
    g_signal_new ("exited",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gbp_acp_client_init (GbpAcpClient *self)
{
  self->terminals = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->state = GBP_ACP_STATE_DISCONNECTED;
}

GbpAcpClient *
gbp_acp_client_new (IdeContext *context)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GBP_TYPE_ACP_CLIENT,
                       "context", context,
                       NULL);
}

void
gbp_acp_client_start (GbpAcpClient *self)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(GInputStream) to_stdout = NULL;
  g_autoptr(GOutputStream) to_stdin = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *cwd = NULL;
  const gchar *command;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));

  if (self->state != GBP_ACP_STATE_DISCONNECTED)
    return;

  self->workdir = ide_context_ref_workdir (self->context);
  cwd = g_file_get_path (self->workdir);
  command = g_getenv ("GBP_ACP_AGENT");
  if (command == NULL || *command == '\0')
    command = ACP_DEFAULT_COMMAND;

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  ide_subprocess_launcher_set_clear_env (launcher, FALSE);

  if (cwd != NULL)
    ide_subprocess_launcher_set_cwd (launcher, cwd);

  ide_subprocess_launcher_push_argv (launcher, command);
  ide_subprocess_launcher_push_argv (launcher, "acp");

  self->cancellable = g_cancellable_new ();
  self->subprocess = ide_subprocess_launcher_spawn (launcher, self->cancellable, &error);

  if (self->subprocess == NULL)
    {
      gbp_acp_client_fail (self, error);
      return;
    }

  to_stdin = ide_subprocess_get_stdin_pipe (self->subprocess);
  to_stdout = ide_subprocess_get_stdout_pipe (self->subprocess);

  if (to_stdin == NULL || to_stdout == NULL)
    {
      g_autoptr(GError) pipe_error = NULL;

      pipe_error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                                        _("Failed to create ACP agent pipes"));
      gbp_acp_client_fail (self, pipe_error);
      return;
    }

  self->rpc = gbp_acp_jsonrpc_new (to_stdout, to_stdin);

  g_signal_connect_object (self->rpc, "notification",
                           G_CALLBACK (gbp_acp_client_on_notification),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rpc, "handle-call",
                           G_CALLBACK (gbp_acp_client_on_handle_call),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->rpc, "failed",
                           G_CALLBACK (gbp_acp_client_on_failed),
                           self, G_CONNECT_SWAPPED);

  ide_subprocess_wait_async (self->subprocess,
                             self->cancellable,
                             gbp_acp_client_on_subprocess_exited,
                             g_object_ref (self));

  gbp_acp_jsonrpc_start (self->rpc);
  gbp_acp_client_send_initialize (self);
}

void
gbp_acp_client_stop (GbpAcpClient *self)
{
  g_return_if_fail (GBP_IS_ACP_CLIENT (self));

  if (self->state == GBP_ACP_STATE_CLOSED)
    return;

  gbp_acp_client_set_state (self, GBP_ACP_STATE_CLOSED);

  if (self->cancellable != NULL)
    g_cancellable_cancel (self->cancellable);

  if (self->subprocess != NULL)
    ide_subprocess_force_exit (self->subprocess);

  if (self->rpc != NULL)
    gbp_acp_jsonrpc_close (self->rpc);

  g_hash_table_remove_all (self->terminals);
}

gboolean
gbp_acp_client_is_ready (GbpAcpClient *self)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), FALSE);
  return self->state == GBP_ACP_STATE_READY;
}

gboolean
gbp_acp_client_is_busy (GbpAcpClient *self)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), FALSE);
  return self->prompt_task != NULL;
}

const gchar *
gbp_acp_client_get_session_id (GbpAcpClient *self)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), NULL);
  return self->session_id;
}

const gchar *
gbp_acp_client_get_agent_name (GbpAcpClient *self)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), NULL);
  return self->agent_name;
}

GVariant *
gbp_acp_client_dup_config_options (GbpAcpClient *self)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), NULL);

  if (self->config_options != NULL)
    return g_variant_ref (self->config_options);

  return NULL;
}

void
gbp_acp_client_prompt_async (GbpAcpClient       *self,
                             const gchar        *text,
                             GCancellable       *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer            user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));
  g_return_if_fail (text != NULL);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_acp_client_prompt_async);

  if (self->state != GBP_ACP_STATE_READY)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED,
                               _("The ACP agent is not ready"));
      return;
    }

  if (self->prompt_task != NULL)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_BUSY,
                               _("A prompt is already in progress"));
      return;
    }

  params = JSONRPC_MESSAGE_NEW (
    "sessionId", JSONRPC_MESSAGE_PUT_STRING (self->session_id),
    "prompt", "[",
      "{",
        "type", JSONRPC_MESSAGE_PUT_STRING ("text"),
        "text", JSONRPC_MESSAGE_PUT_STRING (text),
      "}",
    "]");

  self->prompt_task = g_steal_pointer (&task);

  gbp_acp_jsonrpc_call_async (self->rpc,
                             "session/prompt",
                             params,
                             cancellable,
                             gbp_acp_client_on_prompt_cb,
                             g_object_ref (self));
}

gchar *
gbp_acp_client_prompt_finish (GbpAcpClient  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (GBP_IS_ACP_CLIENT (self), NULL);
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

void
gbp_acp_client_cancel (GbpAcpClient *self)
{
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));

  if (self->rpc == NULL || self->session_id == NULL)
    return;

  if (self->permission_id != NULL)
    {
      g_autoptr(GVariant) cancelled = NULL;

      cancelled = JSONRPC_MESSAGE_NEW ("outcome", "{",
                                         "outcome", JSONRPC_MESSAGE_PUT_STRING ("cancelled"),
                                       "}");
      gbp_acp_jsonrpc_reply (self->rpc, self->permission_id, cancelled);
      g_clear_pointer (&self->permission_id, g_variant_unref);
    }

  params = JSONRPC_MESSAGE_NEW ("sessionId",
                                JSONRPC_MESSAGE_PUT_STRING (self->session_id));

  gbp_acp_jsonrpc_send_notification (self->rpc, "session/cancel", params);
}

void
gbp_acp_client_reply_permission (GbpAcpClient *self,
                                 const gchar  *option_id)
{
  g_autoptr(GVariant) result = NULL;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));

  if (self->permission_id == NULL || self->rpc == NULL)
    return;

  result = JSONRPC_MESSAGE_NEW (
    "outcome", "{",
      "outcome", JSONRPC_MESSAGE_PUT_STRING ("selected"),
      "optionId", JSONRPC_MESSAGE_PUT_STRING (option_id),
    "}");

  gbp_acp_jsonrpc_reply (self->rpc, self->permission_id, result);

  g_clear_pointer (&self->permission_id, g_variant_unref);
}

static void
gbp_acp_client_on_set_config_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(GbpAcpClient) self = user_data;
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GVariant) options = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GBP_IS_ACP_CLIENT (self));

  reply = gbp_acp_jsonrpc_call_finish (GBP_ACP_JSONRPC (object), result, &error);

  if (error != NULL)
    {
      g_warning ("Failed to set ACP config option: %s", error->message);
      return;
    }

  options = g_variant_lookup_value (reply, "configOptions", NULL);
  if (options != NULL)
    {
      g_clear_pointer (&self->config_options, g_variant_unref);
      self->config_options = g_variant_ref (options);
    }
}

void
gbp_acp_client_set_config_option (GbpAcpClient *self,
                                  const gchar  *config_id,
                                  const gchar  *value)
{
  g_autoptr(GVariant) params = NULL;

  g_return_if_fail (GBP_IS_ACP_CLIENT (self));
  g_return_if_fail (config_id != NULL);
  g_return_if_fail (value != NULL);

  if (self->state != GBP_ACP_STATE_READY || self->rpc == NULL)
    return;

  params = JSONRPC_MESSAGE_NEW (
    "sessionId", JSONRPC_MESSAGE_PUT_STRING (self->session_id ?: ""),
    "configId", JSONRPC_MESSAGE_PUT_STRING (config_id),
    "value", JSONRPC_MESSAGE_PUT_STRING (value));

  gbp_acp_jsonrpc_call_async (self->rpc,
                             "session/set_config_option",
                             params,
                             self->cancellable,
                             gbp_acp_client_on_set_config_cb,
                             g_object_ref (self));
}
