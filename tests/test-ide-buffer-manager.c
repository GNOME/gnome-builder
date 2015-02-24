/* test-ide-buffer-manager.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <ide.h>

typedef struct
{
  GMainLoop    *main_loop;
  IdeContext   *context;
  GCancellable *cancellable;
  GError       *error;
  gchar        *tmpfile;
  gint          load_count;
  gint          save_count;
} test_buffer_manager_basic_state;

static void
save_buffer_cb (IdeBufferManager                *buffer_manager,
                IdeBuffer                       *buffer,
                test_buffer_manager_basic_state *state)
{
  state->save_count++;
}

static void
buffer_loaded_cb (IdeBufferManager                *buffer_manager,
                  IdeBuffer                       *buffer,
                  test_buffer_manager_basic_state *state)
{
  state->load_count++;
}

static void
test_buffer_manager_basic_cb3 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  test_buffer_manager_basic_state *state = user_data;
  gboolean ret;

  ret = ide_buffer_manager_save_file_finish (buffer_manager, result, &state->error);

  g_assert_no_error (state->error);
  g_assert (ret);

  g_main_loop_quit (state->main_loop);
}

static void
test_buffer_manager_basic_cb2 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeProgress) progress = NULL;
  IdeProject *project;
  GtkTextIter begin, end;
  IdeFile *file;
  test_buffer_manager_basic_state *state = user_data;
  g_autoptr(gchar) text = NULL;
  int fd;

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &state->error);
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
  g_assert_cmpstr (text, ==, "LT_INIT");

  g_assert_no_error (state->error);
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  fd = g_file_open_tmp (NULL, &state->tmpfile, &state->error);
  g_assert_no_error (state->error);
  g_assert_cmpint (-1, !=, fd);
  close (fd); /* not secure, but okay for tests */

  project = ide_context_get_project (state->context);
  file = ide_project_get_file_for_path (project, state->tmpfile);

  ide_buffer_manager_save_file_async (buffer_manager,
                                      buffer,
                                      file,
                                      &progress,
                                      state->cancellable,
                                      test_buffer_manager_basic_cb3,
                                      state);

  g_assert (IDE_IS_PROGRESS (progress));
}

static void
test_buffer_manager_basic_cb1 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  test_buffer_manager_basic_state *state = user_data;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(IdeProgress) progress = NULL;
  IdeBufferManager *buffer_manager;
  IdeProject *project;

  state->context = ide_context_new_finish (result, &state->error);

  if (!state->context)
    goto failure;

  buffer_manager = ide_context_get_buffer_manager (state->context);

  g_signal_connect (buffer_manager, "save-buffer", G_CALLBACK (save_buffer_cb), state);
  g_signal_connect (buffer_manager, "buffer-loaded", G_CALLBACK (buffer_loaded_cb), state);

  project = ide_context_get_project (state->context);
  file = ide_project_get_file_for_path (project, TEST_DATA_DIR"/project1/configure.ac");

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      FALSE,
                                      &progress,
                                      state->cancellable,
                                      test_buffer_manager_basic_cb2,
                                      state);

  g_assert (IDE_IS_PROGRESS (progress));

  return;

failure:
  g_main_loop_quit (state->main_loop);
}

static void
test_buffer_manager_basic (void)
{
  test_buffer_manager_basic_state state = { 0 };
  IdeBufferManager *buffer_manager;
  GFile *project_file;

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");

  state.main_loop = g_main_loop_new (NULL, FALSE);
  state.cancellable = g_cancellable_new ();

  ide_context_new_async (project_file, state.cancellable,
                         test_buffer_manager_basic_cb1, &state);

  g_main_loop_run (state.main_loop);

  if (state.tmpfile)
    g_unlink (state.tmpfile);

  g_assert_no_error (state.error);
  g_assert (state.context);

  buffer_manager = ide_context_get_buffer_manager (state.context);
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  g_assert_cmpint (state.load_count, ==, 1);
  g_assert_cmpint (state.save_count, ==, 1);

  g_clear_object (&state.cancellable);
  g_clear_object (&state.context);
  g_clear_error (&state.error);
  g_main_loop_unref (state.main_loop);
  g_clear_object (&project_file);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/BufferManager/basic", test_buffer_manager_basic);
  return g_test_run ();
}
