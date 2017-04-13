/* test-server.c
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>
#include <glib-unix.h>
#include <jsonrpc-glib.h>
#include <unistd.h>

static void
handle_notification (JsonrpcServer *server,
                     JsonrpcClient *client,
                     const gchar   *method,
                     JsonNode      *params,
                     gpointer       user_data)
{
  gint *count = user_data;

  g_assert (JSONRPC_IS_SERVER (server));
  g_assert (JSONRPC_IS_CLIENT (client));
  g_assert (method != NULL);
  g_assert (params != NULL);

  (*count)++;
}

static gboolean
handle_call (JsonrpcServer *server,
             JsonrpcClient *client,
             const gchar   *method,
             GVariant      *id,
             GVariant      *params,
             gpointer       user_data)
{
  const gchar *rootPath = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GVariantDict) dict = { 0 };
  gboolean r;

  g_assert (id != NULL);
  g_assert (params != NULL);
  g_assert (g_variant_is_of_type (id, G_VARIANT_TYPE_INT64));
  g_assert (g_variant_is_of_type (params, G_VARIANT_TYPE_VARDICT));

  g_variant_dict_init (&dict, params);
  r = g_variant_dict_lookup (&dict, "rootPath", "&s", &rootPath);
  g_assert_cmpint (r, ==, TRUE);
  g_assert_cmpstr (rootPath, ==, ".");
  g_variant_dict_clear (&dict);

  g_assert_cmpint (1, ==, g_variant_get_int64 (id));
  g_assert_cmpstr (method, ==, "initialize");

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "foo", "s", "bar");
  r = jsonrpc_client_reply (client, id, g_variant_dict_end (&dict), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  return TRUE;
}

static void
test_basic (gboolean use_gvariant)
{
  g_autoptr(JsonrpcServer) server = NULL;
  g_autoptr(JsonrpcClient) client = NULL;
  g_autoptr(GInputStream) input_a = NULL;
  g_autoptr(GInputStream) input_b = NULL;
  g_autoptr(GOutputStream) output_a = NULL;
  g_autoptr(GOutputStream) output_b = NULL;
  g_autoptr(GIOStream) stream_a = NULL;
  g_autoptr(GIOStream) stream_b = NULL;
  g_autoptr(GVariant) message = NULL;
  g_autoptr(GVariant) return_value = NULL;
  g_autoptr(GError) error = NULL;
  GVariantDict dict;
  gint pair_a[2];
  gint pair_b[2];
  gint count = 0;
  gint r;

  r = g_unix_open_pipe (pair_a, FD_CLOEXEC, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  r = g_unix_open_pipe (pair_b, FD_CLOEXEC, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  input_a = g_unix_input_stream_new (pair_a[0], TRUE);
  input_b = g_unix_input_stream_new (pair_b[0], TRUE);
  output_a = g_unix_output_stream_new (pair_a[1], TRUE);
  output_b = g_unix_output_stream_new (pair_b[1], TRUE);

  stream_a = g_simple_io_stream_new (input_a, output_b);
  stream_b = g_simple_io_stream_new (input_b, output_a);

  client = jsonrpc_client_new (stream_a);

  /* Possibly upgrade connection to gvariant encoding */
  jsonrpc_client_set_use_gvariant (client, use_gvariant);

  server = jsonrpc_server_new ();
  jsonrpc_server_accept_io_stream (server, stream_b);

  g_signal_connect (server,
                    "handle-call",
                    G_CALLBACK (handle_call),
                    NULL);

  g_signal_connect (server,
                    "notification",
                    G_CALLBACK (handle_notification),
                    &count);

  r = jsonrpc_client_send_notification (client, "testNotification", NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "rootPath", "s", ".");
  message = g_variant_dict_end (&dict);

  r = jsonrpc_client_call (client,
                           "initialize",
                           g_steal_pointer (&message),
                           NULL,
                           &return_value,
                           &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);
  g_assert (return_value != NULL);

  g_assert_cmpint (count, ==, 1);
}

static void
test_basic_json (void)
{
  test_basic (FALSE);
}

static void
test_basic_gvariant (void)
{
  test_basic (TRUE);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Jsonrpc/Server/json", test_basic_json);
  g_test_add_func ("/Jsonrpc/Server/gvariant", test_basic_gvariant);
  return g_test_run ();
}
