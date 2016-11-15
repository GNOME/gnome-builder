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

#define G_LOG_DOMAIN "test-ide-buffer"

#include <glib.h>
#include <glib/gstdio.h>
#include <ide.h>

#include "application/ide-application-tests.h"

static void
flags_changed_cb (IdeBuffer *buffer,
                  gpointer   user_data)
{
  g_autofree gchar *str = NULL;
  g_autoptr(GTask) task = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  IDE_ENTRY;

  ide_buffer_trim_trailing_whitespace (buffer);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  str = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
  g_assert_cmpstr (str, ==, "abcd\n\n\n");

  g_task_return_boolean (task, TRUE);

  g_object_unref (buffer);

  IDE_EXIT;
}

static void
test_buffer_basic_cb2 (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdeBufferManager *manager = (IdeBufferManager *)object;
  g_autoptr(IdeBuffer) ret = NULL;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;

  IDE_ENTRY;

  ret = ide_buffer_manager_load_file_finish (manager, result, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert (IDE_IS_BUFFER (ret));

  g_signal_connect_object (ret,
                           "line-flags-changed",
                           G_CALLBACK (flags_changed_cb),
                           g_object_ref (task),
                           0);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ret), "abcd  \n\n  \n", -1);

  IDE_EXIT;
}

static void
test_buffer_basic_cb1 (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeBufferManager *manager;
  IdeProject *project;
  GError *error = NULL;

  IDE_ENTRY;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  manager = ide_context_get_buffer_manager (context);
  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, "test-ide-buffer.tmp");

  ide_buffer_manager_load_file_async (manager,
                                      file,
                                      FALSE,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      NULL,
                                      g_task_get_cancellable (task),
                                      test_buffer_basic_cb2,
                                      g_object_ref (task));

  IDE_EXIT;
}

static void
test_buffer_basic (GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *path = NULL;
  GTask *task;

  IDE_ENTRY;

  task = g_task_new (NULL, cancellable, callback, user_data);
  path = g_build_filename (TEST_DATA_DIR, "project1", "configure.ac", NULL);
  project_file = g_file_new_for_path (path);
  ide_context_new_async (project_file, cancellable, test_buffer_basic_cb1, task);

  IDE_EXIT;
}

gint
main (gint   argc,
      gchar *argv[])
{
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();
  ide_application_add_test (app, "/Ide/Buffer/basic", test_buffer_basic, NULL);
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
