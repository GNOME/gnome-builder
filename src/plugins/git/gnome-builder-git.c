/* gnome-builder-git.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/* Prologue {{{1 */

#define G_LOG_DOMAIN "gnome-builder-git"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib-unix.h>
#include <jsonrpc-glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gbp-git.h"

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
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Reply failed: %s", error->message);
    }
}

static void
client_op_reply (ClientOp *op,
                 GVariant *reply)
{
  g_assert (op != NULL);
  g_assert (op->client != NULL);

  jsonrpc_client_reply_async (op->client,
                              op->id,
                              reply,
                              op->cancellable,
                              (GAsyncReadyCallback)handle_reply_cb,
                              client_op_ref (op));
}

/* Initialize {{{1 */

static void
handle_initialize (JsonrpcServer *server,
                   JsonrpcClient *client,
                   const gchar   *method,
                   GVariant      *id,
                   GVariant      *params,
                   GbpGit        *git)
{
  g_autoptr(ClientOp) op = NULL;
  const gchar *uri = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "initialize"));
  g_assert (id != NULL);
  g_assert (GBP_IS_GIT (git));

  op = client_op_new (client, id);

  if (JSONRPC_MESSAGE_PARSE (params, "rootUri", JSONRPC_MESSAGE_GET_STRING (&uri)))
    {
      g_autoptr(GFile) file = g_file_new_for_uri (uri);

      gbp_git_set_workdir (git, file);
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
                       GbpGit        *git)
{
  g_autoptr(ClientOp) op = NULL;
  g_autoptr(GVariant) cid = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "$/cancelRequest"));
  g_assert (id != NULL);
  g_assert (GBP_IS_GIT (git));

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

/* Is Ignored Handler {{{1 */

static void
handle_is_ignored_cb (GbpGit       *git,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  g_assert (GBP_IS_GIT (git));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  ret = gbp_git_is_ignored_finish (git, result, &error);

  if (error != NULL)
    client_op_error (op, error);
  else
    client_op_reply (op, g_variant_new_boolean (ret));
}

static void
handle_is_ignored (JsonrpcServer *server,
                   JsonrpcClient *client,
                   const gchar   *method,
                   GVariant      *id,
                   GVariant      *params,
                   GbpGit        *git)
{
  g_autoptr(ClientOp) op = NULL;
  const gchar *path = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "git/isIgnored"));
  g_assert (id != NULL);
  g_assert (GBP_IS_GIT (git));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "path", JSONRPC_MESSAGE_GET_STRING (&path)))
    {
      client_op_bad_params (op);
      return;
    }

  gbp_git_is_ignored_async (git,
                            path,
                            op->cancellable,
                            (GAsyncReadyCallback)handle_is_ignored_cb,
                            client_op_ref (op));
}

/* Switch Branch Handler {{{1 */

static void
handle_switch_branch_cb (GbpGit       *git,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *switch_to_directory = NULL;
  GVariantDict reply;

  g_assert (GBP_IS_GIT (git));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!gbp_git_switch_branch_finish (git, result, &switch_to_directory, &error))
    {
      client_op_error (op, error);
      return;
    }

  g_variant_dict_init (&reply, NULL);

  if (switch_to_directory != NULL)
    g_variant_dict_insert (&reply, "switch-to-directory", "s", switch_to_directory);

  client_op_reply (op, g_variant_dict_end (&reply));
}

static void
handle_switch_branch (JsonrpcServer *server,
                      JsonrpcClient *client,
                      const gchar   *method,
                      GVariant      *id,
                      GVariant      *params,
                      GbpGit        *git)
{
  g_autoptr(ClientOp) op = NULL;
  const gchar *name = NULL;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "git/switchBranch"));
  g_assert (id != NULL);
  g_assert (GBP_IS_GIT (git));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "name", JSONRPC_MESSAGE_GET_STRING (&name)))
    {
      client_op_bad_params (op);
      return;
    }

  gbp_git_switch_branch_async (git,
                               name,
                               op->cancellable,
                               (GAsyncReadyCallback)handle_switch_branch_cb,
                               client_op_ref (op));
}

/* List Refs by Kind Handler {{{1 */

static const gchar *
ref_kind_string (GbpGitRefKind kind)
{
  switch (kind)
    {
    case GBP_GIT_REF_BRANCH: return "branch";
    case GBP_GIT_REF_TAG: return "tag";
    case GBP_GIT_REF_ANY: return "any";
    default: return "";
    }
}

static GbpGitRefKind
parse_kind_string (const gchar *str)
{
  if (str == NULL)
    return 0;

  if (g_str_equal (str, "branch"))
    return GBP_GIT_REF_BRANCH;

  if (g_str_equal (str, "tag"))
    return GBP_GIT_REF_TAG;

  if (g_str_equal (str, "any"))
    return GBP_GIT_REF_ANY;

  return 0;
}

static void
handle_list_refs_by_kind_cb (GbpGit       *git,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(ClientOp) op = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) refs = NULL;
  GVariantBuilder builder;

  g_assert (GBP_IS_GIT (git));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (op != NULL);

  if (!(refs = gbp_git_list_refs_by_kind_finish (git, result, &error)))
    {
      client_op_error (op, error);
      return;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (guint i = 0; i < refs->len; i++)
    {
      const GbpGitRef *ref = g_ptr_array_index (refs, i);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
      g_variant_builder_add_parsed (&builder, "{%s,<%s>}", "name", ref->name);
      g_variant_builder_add_parsed (&builder, "{%s,<%s>}", "kind", ref_kind_string (ref->kind));
      g_variant_builder_add_parsed (&builder, "{%s,<%b>}", "is-remote", ref->is_remote);
      g_variant_builder_close (&builder);
    }

  client_op_reply (op, g_variant_builder_end (&builder));
}

static void
handle_list_refs_by_kind (JsonrpcServer *server,
                          JsonrpcClient *client,
                          const gchar   *method,
                          GVariant      *id,
                          GVariant      *params,
                          GbpGit        *git)
{
  g_autoptr(ClientOp) op = NULL;
  const gchar *kind = NULL;
  GbpGitRefKind kind_enum;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (g_str_equal (method, "git/listRefsByKind"));
  g_assert (id != NULL);
  g_assert (GBP_IS_GIT (git));

  op = client_op_new (client, id);

  if (!JSONRPC_MESSAGE_PARSE (params, "kind", JSONRPC_MESSAGE_GET_STRING (&kind)) ||
      !(kind_enum = parse_kind_string (kind)))
    {
      client_op_bad_params (op);
      return;
    }

  gbp_git_list_refs_by_kind_async (git,
                                   kind_enum,
                                   op->cancellable,
                                   (GAsyncReadyCallback)handle_list_refs_by_kind_cb,
                                   client_op_ref (op));
}

/* Main Loop and Setup {{{1 */

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GInputStream) input = g_unix_input_stream_new (STDIN_FILENO, FALSE);
  g_autoptr(GOutputStream) output = g_unix_output_stream_new (STDOUT_FILENO, FALSE);
  g_autoptr(GIOStream) stream = g_simple_io_stream_new (input, output);
  g_autoptr(JsonrpcServer) server = NULL;
  g_autoptr(GbpGit) git = NULL;
  g_autoptr(GError) error = NULL;

  /* Always ignore SIGPIPE */
  signal (SIGPIPE, SIG_IGN);

  g_set_prgname ("gnome-builder-git");

  /* redirect logging to stderr */
  g_log_set_handler (NULL, G_LOG_LEVEL_MASK, log_handler_cb, NULL);

  main_loop = g_main_loop_new (NULL, FALSE);
  git = gbp_git_new ();
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
  jsonrpc_server_add_handler (server, method, (JsonrpcServerHandler)func, g_object_ref (git), g_object_unref)

  ADD_HANDLER ("initialize", handle_initialize);
  ADD_HANDLER ("git/isIgnored", handle_is_ignored);
  ADD_HANDLER ("git/listRefsByKind", handle_list_refs_by_kind);
  ADD_HANDLER ("git/switchBranch", handle_switch_branch);
  ADD_HANDLER ("$/cancelRequest", handle_cancel_request);

#undef ADD_HANDLER

  jsonrpc_server_accept_io_stream (server, stream);

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}

/* vim:set foldmethod=marker: */
