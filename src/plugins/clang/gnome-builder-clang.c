/* gnome-builder-clang.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

/* Prologue {{{1 */

#define G_LOG_DOMAIN "gnome-builder-clang"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <jsonrpc-glib.h>
#include <libide-code.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ide-clang.h"

static guint      in_flight;
static gboolean   closing;
static GMainLoop *main_loop;
static GQueue     ops;

/* Client Operations {{{1 */

typedef struct
{
  volatile gint  ref_count;
  JsonrpcClient *client;
  GVariant      *id;
  GCancellable  *cancellable;
  GList          link;
} ClientOp;

static void
client_op_bad_params (ClientOp *op)
{
  g_assert (op != NULL);

  jsonrpc_client_reply_error_async (op->client,
                                    op->id,
                                    JSONRPC_CLIENT_ERROR_INVALID_PARAMS,
                                    "Invalid parameters for method call",
                                    NULL, NULL, NULL);
  jsonrpc_client_close (op->client, NULL, NULL);
}

static void
client_op_error (ClientOp     *op,
                 const GError *error)
{
  g_assert (op != NULL);

  jsonrpc_client_reply_error_async (op->client,
                                    op->id,
                                    error->code,
                                    error->message,
                                    NULL, NULL, NULL);
}

static ClientOp *
client_op_ref (ClientOp *op)
{
  g_return_val_if_fail (op != NULL, NULL);
  g_return_val_if_fail (op->ref_count > 0, NULL);

  g_atomic_int_inc (&op->ref_count);

  return op;
}

static void
client_op_unref (ClientOp *op)
{
  g_return_if_fail (op != NULL);
  g_return_if_fail (op->ref_count > 0);

  if (g_atomic_int_dec_and_test (&op->ref_count))
    {
      g_clear_object (&op->cancellable);
      g_clear_object (&op->client);
      g_clear_pointer (&op->id, g_variant_unref);
      g_queue_unlink (&ops, &op->link);
      g_slice_free (ClientOp, op);

      in_flight--;

      if (closing && in_flight == 0)
        g_main_loop_quit (main_loop);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ClientOp, client_op_unref)

static ClientOp *
client_op_new (JsonrpcClient *client,
               GVariant      *id)
{
  ClientOp *op;

  op = g_slice_new0 (ClientOp);
  op->id = g_variant_ref (id);
  op->client = g_object_ref (client);
  op->cancellable = g_cancellable_new ();
  op->ref_count = 1;
  op->link.data = op;

  g_queue_push_tail_link (&ops, &op->link);

  ++in_flight;

  return op;
}

static void
handle_reply_cb (JsonrpcClient *client,
                 GAsyncResult  *result,
                 gpointer       user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(ClientOp) op = user_data;

  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);
  g_assert (op->client == client);

  if (!jsonrpc_client_reply_finish (client, result, &error))
    {
      /* We want to bail if a reply fails and just cause the process
       * to be respawned by gnome-builder UI process.
       */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_printerr ("Reply failed: %s\n", error->message);
          exit (EXIT_FAILURE);
        }
    }
}

static void
client_op_reply (ClientOp *op,
                 GVariant *reply)
{
  g_autoptr(GVariant) sunk = NULL;

  g_assert (op != NULL);
  g_assert (op->client != NULL);

  if (reply != NULL)
    sunk = g_variant_ref_sink (reply);

  jsonrpc_client_reply_async (op->client,
                              op->id,
                              sunk,
                              op->cancellable,
                              (GAsyncReadyCallback)handle_reply_cb,
                              client_op_ref (op));
}

/* Index File Handler {{{1 */

static void
handle_index_file_cb (IdeClang     *clang,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GPtrArray) entries = NULL;
  g_autoptr(GError) error = NULL;
  GVariantBuilder builder;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!(entries = ide_clang_index_file_finish (clang, result, &error)))
    {
      client_op_error (op, error);
      return;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (entries, ide_code_index_entry_free);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < entries->len; i++)
    {
      IdeCodeIndexEntry *entry = g_ptr_array_index (entries, i);
      struct {
        guint line;
        guint column;
      } begin, end;

      ide_code_index_entry_get_range (entry, &begin.line, &begin.column, &end.line, &end.column);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add_parsed (&builder, "{%s,<%s>}", "name", ide_code_index_entry_get_name (entry) ?: "");
      g_variant_builder_add_parsed (&builder, "{%s,<%s>}", "key", ide_code_index_entry_get_key (entry) ?: "");
      g_variant_builder_add_parsed (&builder, "{%s,<%i>}", "kind", ide_code_index_entry_get_kind (entry));
      g_variant_builder_add_parsed (&builder, "{%s,<%i>}", "flags", ide_code_index_entry_get_flags (entry));
      g_variant_builder_add_parsed (&builder, "{%s,<(%u,%u,%u,%u)>}", "range",
                                    begin.line, begin.column, end.line, end.column);
      g_variant_builder_close (&builder);
    }

  client_op_reply (op, g_variant_builder_end (&builder));
}

static void
handle_index_file (JsonrpcServer *server,
                   JsonrpcClient *client,
                   const gchar   *method,
                   GVariant      *id,
                   GVariant      *params,
                   IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/indexFile"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "path", JSONRPC_MESSAGE_GET_STRING (&path)))
    {
      client_op_bad_params (op);
      return;
    }

  JSONRPC_MESSAGE_PARSE (params, "flags", JSONRPC_MESSAGE_GET_STRV (&flags));

  ide_clang_index_file_async (clang,
                              path,
                              (const gchar * const *)flags,
                              op->cancellable,
                              (GAsyncReadyCallback)handle_index_file_cb,
                              client_op_ref (op));
}

/* Get Index Key Handler {{{1 */

static void
handle_get_index_key_cb (IdeClang     *clang,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *key = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  key = ide_clang_get_index_key_finish (clang, result, &error);

  if (key == NULL && error == NULL)
    g_set_error (&error,
                 G_IO_ERROR,
                 G_IO_ERROR_NOT_FOUND,
                 "Failed to locate key");

  if (error != NULL)
    client_op_error (op, error);
  else
    client_op_reply (op, g_variant_new_string (key));
}

static void
handle_get_index_key (JsonrpcServer *server,
                      JsonrpcClient *client,
                      const gchar   *method,
                      GVariant      *id,
                      GVariant      *params,
                      IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path = NULL;
  gint64 line = 0;
  gint64 column = 0;
  gboolean r;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/getIndexKey"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags),
    "line", JSONRPC_MESSAGE_GET_INT64 (&line),
    "column", JSONRPC_MESSAGE_GET_INT64 (&column)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_get_index_key_async (clang,
                                 path,
                                 (const gchar * const *)flags,
                                 line,
                                 column,
                                 op->cancellable,
                                 (GAsyncReadyCallback)handle_get_index_key_cb,
                                 client_op_ref (op));
}

/* Find Nearest Scope {{{1 */

static void
handle_find_nearest_scope_cb (IdeClang     *clang,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(IdeSymbol) scope = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) ret = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!(scope = ide_clang_find_nearest_scope_finish (clang, result, &error)))
    {
      client_op_error (op, error);
      return;
    }

  ret = ide_symbol_to_variant (scope);

  client_op_reply (op, ret);
}

static void
handle_find_nearest_scope (JsonrpcServer *server,
                           JsonrpcClient *client,
                           const gchar   *method,
                           GVariant      *id,
                           GVariant      *params,
                           IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path = NULL;
  gboolean r;
  gint64 line = 0;
  gint64 column = 0;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/findNearestScope"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags),
    "line", JSONRPC_MESSAGE_GET_INT64 (&line),
    "column", JSONRPC_MESSAGE_GET_INT64 (&column)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_find_nearest_scope_async (clang,
                                      path,
                                      (const gchar * const *)flags,
                                      line,
                                      column,
                                      op->cancellable,
                                      (GAsyncReadyCallback)handle_find_nearest_scope_cb,
                                      client_op_ref (op));
}

/* Diagnose Handler {{{1 */

static void
handle_diagnose_cb (IdeClang     *clang,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GPtrArray) diagnostics = NULL;
  g_autoptr(GError) error = NULL;
  GVariantBuilder builder;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!(diagnostics = ide_clang_diagnose_finish (clang, result, &error)))
    {
      client_op_error (op, error);
      return;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (diagnostics, ide_object_unref_and_destroy);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < diagnostics->len; i++)
    {
      IdeDiagnostic *diag = g_ptr_array_index (diagnostics, i);
      g_autoptr(GVariant) var = ide_diagnostic_to_variant (diag);

      g_variant_builder_add_value (&builder, var);
    }

  client_op_reply (op, g_variant_builder_end (&builder));
}

static void
handle_diagnose (JsonrpcServer *server,
                 JsonrpcClient *client,
                 const gchar   *method,
                 GVariant      *id,
                 GVariant      *params,
                 IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/diagnose"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "path", JSONRPC_MESSAGE_GET_STRING (&path)))
    {
      client_op_bad_params (op);
      return;
    }

  JSONRPC_MESSAGE_PARSE (params, "flags", JSONRPC_MESSAGE_GET_STRV (&flags));

  ide_clang_diagnose_async (clang,
                            path,
                            (const gchar * const *)flags,
                            op->cancellable,
                            (GAsyncReadyCallback)handle_diagnose_cb,
                            client_op_ref (op));
}

/* Locate Symbol {{{1 */

static void
handle_locate_symbol_cb (IdeClang     *clang,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!(symbol = ide_clang_locate_symbol_finish (clang, result, &error)))
    {
      client_op_error (op, error);
      return;
    }

  ret = ide_symbol_to_variant (symbol);

  client_op_reply (op, ret);
}

static void
handle_locate_symbol (JsonrpcServer *server,
                      JsonrpcClient *client,
                      const gchar   *method,
                      GVariant      *id,
                      GVariant      *params,
                      IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path;
  gboolean r;
  gint64 line = 0;
  gint64 column = 0;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/locateSymbol"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags),
    "line", JSONRPC_MESSAGE_GET_INT64 (&line),
    "column", JSONRPC_MESSAGE_GET_INT64 (&column)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_locate_symbol_async (clang,
                                 path,
                                 (const gchar * const *)flags,
                                 line,
                                 column,
                                 op->cancellable,
                                 (GAsyncReadyCallback)handle_locate_symbol_cb,
                                 client_op_ref (op));
}

/* Get Symbol Tree Handler {{{1 */

static void
handle_get_symbol_tree_cb (IdeClang     *clang,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  ret = ide_clang_get_symbol_tree_finish (clang, result, &error);

  if (!ret)
    client_op_error (op, error);
  else
    client_op_reply (op, ret);
}

static void
handle_get_symbol_tree (JsonrpcServer *server,
                        JsonrpcClient *client,
                        const gchar   *method,
                        GVariant      *id,
                        GVariant      *params,
                        IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path;
  gboolean r;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/getSymbolTree"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_get_symbol_tree_async (clang,
                                   path,
                                   (const gchar * const *)flags,
                                   op->cancellable,
                                   (GAsyncReadyCallback)handle_get_symbol_tree_cb,
                                   client_op_ref (op));
}

/* Completion Handler {{{1 */

static void
handle_complete_cb (IdeClang     *clang,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  ret = ide_clang_complete_finish (clang, result, &error);

  if (!ret)
    client_op_error (op, error);
  else
    client_op_reply (op, ret);
}

static void
handle_complete (JsonrpcServer *server,
                 JsonrpcClient *client,
                 const gchar   *method,
                 GVariant      *id,
                 GVariant      *params,
                 IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path;
  gboolean r;
  gint64 line = 0;
  gint64 column = 0;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/complete"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags),
    "line", JSONRPC_MESSAGE_GET_INT64 (&line),
    "column", JSONRPC_MESSAGE_GET_INT64 (&column)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_complete_async (clang,
                            path,
                            line,
                            column,
                            (const gchar * const *)flags,
                            op->cancellable,
                            (GAsyncReadyCallback)handle_complete_cb,
                            client_op_ref (op));
}

/* Get Highlight Index {{{1 */

static void
handle_get_highlight_index_cb (IdeClang     *clang,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(IdeHighlightIndex) index = NULL;
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CLANG (clang));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if ((index = ide_clang_get_highlight_index_finish (clang, result, &error)))
    ret = ide_highlight_index_to_variant (index);

  if (!ret)
    client_op_error (op, error);
  else
    client_op_reply (op, ret);
}

static void
handle_get_highlight_index (JsonrpcServer *server,
                            JsonrpcClient *client,
                            const gchar   *method,
                            GVariant      *id,
                            GVariant      *params,
                            IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_auto(GStrv) flags = NULL;
  const gchar *path;
  gboolean r;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/getHighlightIndex"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  r = JSONRPC_MESSAGE_PARSE (params,
    "path", JSONRPC_MESSAGE_GET_STRING (&path),
    "flags", JSONRPC_MESSAGE_GET_STRV (&flags)
  );

  if (!r)
    {
      client_op_bad_params (op);
      return;
    }

  ide_clang_get_highlight_index_async (clang,
                                       path,
                                       (const gchar * const *)flags,
                                       op->cancellable,
                                       (GAsyncReadyCallback)handle_get_highlight_index_cb,
                                       client_op_ref (op));
}

/* Set Buffer Contents {{{1 */

static void
handle_set_buffer (JsonrpcServer *server,
                   JsonrpcClient *client,
                   const gchar   *method,
                   GVariant      *id,
                   GVariant      *params,
                   IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GFile) file = NULL;
  const gchar *path = NULL;
  const gchar *contents = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "clang/setBuffer"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "path", JSONRPC_MESSAGE_GET_STRING (&path)))
    {
      client_op_bad_params (op);
      return;
    }

  /* Get the new contents (or NULL bytes if we are unsetting it */
  if (g_variant_lookup (params, "contents", "^&ay", &contents))
    bytes = g_bytes_new (contents, strlen (contents));

  file = g_file_new_for_path (path);
  ide_clang_set_unsaved_file (clang, file, bytes);

  client_op_reply (op, g_variant_new_boolean (TRUE));
}

/* Initialize {{{1 */

static void
handle_initialize (JsonrpcServer *server,
                   JsonrpcClient *client,
                   const gchar   *method,
                   GVariant      *id,
                   GVariant      *params,
                   IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  const gchar *uri = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "initialize"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  if (JSONRPC_MESSAGE_PARSE (params, "rootUri", JSONRPC_MESSAGE_GET_STRING (&uri)))
    {
      g_autoptr(GFile) file = g_file_new_for_uri (uri);

      ide_clang_set_workdir (clang, file);
    }

  client_op_reply (op, NULL);
}

/* Cancel Request {{{1 */

static void
handle_cancel_request (JsonrpcServer *server,
                       JsonrpcClient *client,
                       const gchar   *method,
                       GVariant      *id,
                       GVariant      *params,
                       IdeClang      *clang)
{
  g_autoptr(ClientOp) op = NULL;
  g_autoptr(GVariant) cid = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "$/cancelRequest"));
  g_assert (id != NULL);
  g_assert (IDE_IS_CLANG (clang));

  op = client_op_new (client, id);

  if (params == NULL ||
      !(cid = g_variant_lookup_value (params, "id", NULL)) ||
      g_variant_equal (id, cid))
    {
      client_op_bad_params (op);
      return;
    }

  /* Lookup in-flight command to cancel it */

  for (const GList *iter = ops.head; iter != NULL; iter = iter->next)
    {
      ClientOp *ele = iter->data;

      if (g_variant_equal (ele->id, cid))
        {
          g_cancellable_cancel (ele->cancellable);
          break;
        }
    }

  client_op_reply (op, NULL);
}

/* Main and Server Setup {{{1 */

static void
on_client_closed_cb (JsonrpcServer *server,
                     JsonrpcClient *client,
                     gpointer       user_data)
{
  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));

  closing = TRUE;

  if (in_flight == 0)
    g_main_loop_quit (main_loop);
}

static void
log_handler_cb (const gchar    *log_domain,
                GLogLevelFlags  level,
                const gchar    *message,
                gpointer        user_data)
{
  /* Only write to stderr so that we don't interrupt IPC */
  g_printerr ("%s: %s\n", log_domain, message);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GInputStream) input = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  g_autoptr(GOutputStream) output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
  g_autoptr(GIOStream) stream = g_simple_io_stream_new (input, output);
  g_autoptr(JsonrpcServer) server = NULL;
  g_autoptr(IdeClang) clang = NULL;
  g_autoptr(GError) error = NULL;

  /* Always ignore SIGPIPE */
  signal (SIGPIPE, SIG_IGN);

  /* redirect logging to stderr */
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler_cb, NULL);

  main_loop = g_main_loop_new (NULL, FALSE);
  clang = ide_clang_new ();
  server = jsonrpc_server_new ();

  if (!g_unix_set_fd_nonblocking (STDIN_FILENO, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (STDOUT_FILENO, TRUE, &error))
    {
      g_printerr ("Failed to set FD non-blocking: %s\n", error->message);
      return EXIT_FAILURE;
    }

  g_signal_connect (server,
                    "client-closed",
                    G_CALLBACK (on_client_closed_cb),
                    NULL);

#define ADD_HANDLER(method, func) \
  jsonrpc_server_add_handler (server, method, (JsonrpcServerHandler)func, g_object_ref (clang), g_object_unref)

  ADD_HANDLER ("initialize", handle_initialize);
  ADD_HANDLER ("clang/complete", handle_complete);
  ADD_HANDLER ("clang/diagnose", handle_diagnose);
  ADD_HANDLER ("clang/findNearestScope", handle_find_nearest_scope);
  ADD_HANDLER ("clang/getIndexKey", handle_get_index_key);
  ADD_HANDLER ("clang/getSymbolTree", handle_get_symbol_tree);
  ADD_HANDLER ("clang/indexFile", handle_index_file);
  ADD_HANDLER ("clang/locateSymbol", handle_locate_symbol);
  ADD_HANDLER ("clang/getHighlightIndex", handle_get_highlight_index);
  ADD_HANDLER ("clang/setBuffer", handle_set_buffer);
  ADD_HANDLER ("$/cancelRequest", handle_cancel_request);

#undef ADD_HANDLER

  jsonrpc_server_accept_io_stream (server, stream);

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}

/* vim:set foldmethod=marker: */
