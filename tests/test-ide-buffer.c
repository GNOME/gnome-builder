/* test-ide-buffer.c
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

#include "tests.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <ide.h>

typedef struct
{
  GMainLoop    *main_loop;
  IdeContext   *context;
  GCancellable *cancellable;
  GError       *error;
} test_buffer_basic_state;

static void
flags_changed_cb (IdeBuffer *buffer,
                  gpointer   user_data)
{
  test_buffer_basic_state *state = user_data;
  g_autofree gchar *str = NULL;
  GtkTextIter begin;
  GtkTextIter end;

  ide_buffer_trim_trailing_whitespace (buffer);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  str = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
  g_assert_cmpstr (str, ==, "abcd\n\n\n");

  g_object_unref (buffer);

  g_main_loop_quit (state->main_loop);
}

static void
test_buffer_basic_cb2 (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeBufferManager *manager = (IdeBufferManager *)object;
  test_buffer_basic_state *state = user_data;
  GError *error = NULL;
  g_autoptr(IdeBuffer) ret = NULL;

  ret = ide_buffer_manager_load_file_finish (manager, result, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert (IDE_IS_BUFFER (ret));

  g_signal_connect (ret, "line-flags-changed", G_CALLBACK (flags_changed_cb), state);
  g_object_ref (ret);

  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ret), "abcd  \n\n  \n", -1);
}

static void
test_buffer_basic_cb1 (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  test_buffer_basic_state *state = user_data;
  g_autoptr(IdeFile) file = NULL;
  IdeBufferManager *manager;
  IdeProject *project;

  state->context = ide_context_new_finish (result, &state->error);

  if (!state->context)
    goto failure;

  manager = ide_context_get_buffer_manager (state->context);

  project = ide_context_get_project (state->context);
  file = ide_project_get_file_for_path (project, "test-ide-buffer.tmp");

  ide_buffer_manager_load_file_async (manager,
                                      file,
                                      FALSE,
                                      NULL,
                                      state->cancellable,
                                      test_buffer_basic_cb2,
                                      state);

  return;

failure:
  g_main_loop_quit (state->main_loop);
}

static void
test_buffer_basic (void)
{
  test_buffer_basic_state state = { 0 };
  IdeBufferManager *manager;
  GFile *project_file;

  project_file = g_file_new_for_path (TEST_DATA_DIR"/project1/configure.ac");

  state.main_loop = g_main_loop_new (NULL, FALSE);
  state.cancellable = g_cancellable_new ();

  ide_context_new_async (project_file, state.cancellable,
                         test_buffer_basic_cb1, &state);

  g_main_loop_run (state.main_loop);

  g_assert_no_error (state.error);
  g_assert (state.context);

  manager = ide_context_get_buffer_manager (state.context);
  g_assert (IDE_IS_BUFFER_MANAGER (manager));

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
  gtk_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Buffer/basic", test_buffer_basic);
  return g_test_run ();
}
