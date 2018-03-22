/* test-ide-buffer.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
#include "../plugins/gnome-builder-plugins.h"

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

  g_task_return_boolean (task, TRUE);

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
  g_autoptr(GError) error = NULL;
  IdeBufferManager *manager;

  IDE_ENTRY;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  manager = ide_context_get_buffer_manager (context);
  file = ide_file_new_for_path (context, "test-ide-buffer.tmp");

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
  static const gchar *required_plugins[] = { "autotools-plugin", "buildconfig", "directory-plugin", NULL };
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();
  ide_application_add_test (app, "/Ide/Buffer/basic", test_buffer_basic, NULL, required_plugins);
  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
