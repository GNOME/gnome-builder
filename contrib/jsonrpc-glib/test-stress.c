/* test-stress.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <jsonrpc-glib.h>

static GMainLoop *main_loop;
static gint n_ops;

static gboolean begin_next_op_source (gpointer data);

#if 0
# define LOG(s) g_print(s "\n")
#else
# define LOG(s) do { } while (0)
#endif

static GIOStream *
create_stream (gint read_fd, gint write_fd)
{
  g_autoptr(GInputStream) input = g_unix_input_stream_new (read_fd, TRUE);
  g_autoptr(GOutputStream) output = g_unix_output_stream_new (write_fd, TRUE);

  return g_simple_io_stream_new (input, output);
}

static void
server_handle_reply_cb (JsonrpcClient *client,
                        GAsyncResult  *result,
                        gpointer       user_data)
{
  g_autoptr(JsonrpcServer) server = user_data;
  g_autoptr(GError) error = NULL;
  gboolean r;

  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (JSONRPC_IS_SERVER (server));

  r = jsonrpc_client_reply_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, 1);

  /*
   * If there are no operations left to perform, close the
   * connection to test the server disconnected state on the
   * client.
   */
  if (n_ops == 0)
    {
      LOG ("server: closing client stream");
      r = jsonrpc_client_close (client, NULL, &error);
      g_assert_no_error (error);
      g_assert_cmpint (r, ==, 1);
    }
}

static gboolean
server_handle_call_cb (JsonrpcServer *server,
                       JsonrpcClient *client,
                       const gchar   *method,
                       GVariant      *id,
                       GVariant      *params)
{
  GVariantDict dict;

  /* Just reply with the info we got */

  LOG ("server: handling incoming call");

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "method", "s", method);
  g_variant_dict_insert_value (&dict, "id", id);
  g_variant_dict_insert_value (&dict, "params", params);

  jsonrpc_client_reply_async (client,
                              id,
                              g_variant_dict_end (&dict),
                              NULL,
                              (GAsyncReadyCallback) server_handle_reply_cb,
                              g_object_ref (server));

  LOG ("server: replied to client");

  return TRUE;
}

static void
server_notification_cb (JsonrpcServer *server,
                        JsonrpcClient *client,
                        const gchar   *method,
                        GVariant      *params)
{
}

static void
client_call_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  JsonrpcClient *client = JSONRPC_CLIENT (object);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) reply = NULL;
  gboolean r;

  LOG ("client: got reply from server");

  r = jsonrpc_client_call_finish (client, result, &reply, &error);

  if (n_ops < 0)
    {
      /* We expect an error here, for the stream being closed */
      g_assert (error != NULL);
      g_assert_cmpint (r, ==, 0);
      g_assert (reply == NULL);
      g_main_loop_quit (main_loop);
      return;
    }

  g_assert (error || result);

  g_timeout_add_full (0, 0, begin_next_op_source, g_object_ref (client), g_object_unref);
}

static gboolean
begin_next_op_source (gpointer data)
{
  JsonrpcClient *client = data;
  g_autoptr(GVariant) params = NULL;

  params = JSONRPC_MESSAGE_NEW (
    "ops-left", JSONRPC_MESSAGE_PUT_INT32 (n_ops)
  );

  LOG ("client: dispatching next async call");

  n_ops--;

  jsonrpc_client_call_async (client,
                             "some/operation",
                             g_steal_pointer (&params),
                             NULL,
                             client_call_cb,
                             NULL);

  return G_SOURCE_REMOVE;
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(JsonrpcServer) server = NULL;
  g_autoptr(GIOStream) client_stream = NULL;
  g_autoptr(GIOStream) server_stream = NULL;
  gint pair1[2];
  gint pair2[2];

  main_loop = g_main_loop_new (NULL, FALSE);

  n_ops = 1000;

  /*
   * The goal here is to create a server and a client and submit a large number
   * of replies between them. At some point the server will "fail" by closing
   * the stream to ensure that the client handles things properly.
   */

  g_assert_cmpint (0, ==, pipe (pair1));
  g_assert_cmpint (0, ==, pipe (pair2));

  client_stream = create_stream (pair1[0], pair2[1]);
  server_stream = create_stream (pair2[0], pair1[1]);

  client = jsonrpc_client_new (client_stream);
  server = jsonrpc_server_new ();

  g_signal_connect (server,
                    "handle-call",
                    G_CALLBACK (server_handle_call_cb),
                    NULL);

  g_signal_connect (server,
                    "notification",
                    G_CALLBACK (server_notification_cb),
                    NULL);

  jsonrpc_server_accept_io_stream (server, server_stream);

  g_timeout_add_full (0, 0, begin_next_op_source, g_object_ref (client), g_object_unref);

  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  return 0;
}
