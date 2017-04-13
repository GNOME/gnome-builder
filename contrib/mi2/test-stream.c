/* test-stream.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "mi2-console-message.h"
#include "mi2-input-stream.h"
#include "mi2-output-stream.h"

static GMainLoop *main_loop;

static void
read_message_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  Mi2InputStream *stream = (Mi2InputStream *)object;
  g_autoptr(Mi2Message) message = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (MI2_IS_INPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));

  message = mi2_input_stream_read_message_finish (stream, result, &error);
  g_assert_no_error (error);

  if (message != NULL)
    mi2_input_stream_read_message_async (stream, NULL, read_message_cb, NULL);
  else
    g_main_loop_quit (main_loop);
}

static void
test_basic_read (void)
{
  g_autoptr(Mi2InputStream) stream = NULL;
  g_autoptr(GInputStream) base_stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;

  main_loop = g_main_loop_new (NULL, FALSE);

  file = g_file_new_for_path (TEST_DATA_DIR"/test-stream-1.txt");

  base_stream = G_INPUT_STREAM (g_file_read (file, NULL, NULL));
  g_assert_no_error (error);
  g_assert (base_stream != NULL);

  stream = mi2_input_stream_new (base_stream);
  g_assert_no_error (error);
  g_assert (stream != NULL);

  mi2_input_stream_read_message_async (stream, NULL, read_message_cb, NULL);

  g_main_loop_run (main_loop);
  g_clear_pointer (&main_loop, g_main_loop_unref);
}

static void
write_message_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  Mi2OutputStream *stream = (Mi2OutputStream *)object;
  g_autoptr(GError) error = NULL;
  gboolean r;

  g_assert (MI2_IS_OUTPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));

  r = mi2_output_stream_write_message_finish (stream, result, &error);
  g_assert_no_error (error);
  g_assert (r);

  g_main_loop_quit (main_loop);
}

static void
test_basic_write (void)
{
  g_autoptr(Mi2OutputStream) stream = NULL;
  g_autoptr(GOutputStream) base_stream = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(Mi2Message) message = NULL;
  const gchar *str;

  main_loop = g_main_loop_new (NULL, FALSE);

  base_stream = g_memory_output_stream_new_resizable ();
  g_assert_no_error (error);
  g_assert (base_stream != NULL);

  stream = mi2_output_stream_new (base_stream);
  g_assert_no_error (error);
  g_assert (stream != NULL);

  message = g_object_new (MI2_TYPE_CONSOLE_MESSAGE,
                          "message", "this is a test message",
                          NULL);

  mi2_output_stream_write_message_async (stream, message, NULL, write_message_cb, NULL);

  g_main_loop_run (main_loop);

  str = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (base_stream));
  g_assert_cmpstr (str, ==, "~\"this is a test message\"\n");

  g_clear_pointer (&main_loop, g_main_loop_unref);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Mi2/InputStream/read_message_async", test_basic_read);
  g_test_add_func ("/Mi2/InputStream/write_message_async", test_basic_write);
  return g_test_run ();
}
