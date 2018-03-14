/* test-ide-buffer-manager.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include "application/ide-application-tests.h"
#include "../plugins/gnome-builder-plugins.h"

#ifdef G_DISABLE_ASSERT
# undef G_DISABLE_ASSERT
#endif

static gint   save_count;
static gint   load_count;
static gchar *tmpfilename;

static void
save_buffer_cb (IdeBufferManager *buffer_manager,
                IdeBuffer        *buffer,
                gpointer          user_data)
{
  save_count++;
}

static void
buffer_loaded_cb (IdeBufferManager *buffer_manager,
                  IdeBuffer        *buffer,
                  gpointer          user_data)
{
  load_count++;
}

static void
test_buffer_manager_basic_cb3 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(GTask) task = user_data;
  GError *error = NULL;
  gboolean ret;

  g_unlink (tmpfilename);
  g_free (tmpfilename);
  tmpfilename = NULL;

  ret = ide_buffer_manager_save_file_finish (buffer_manager, result, &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_task_return_boolean (task, TRUE);
}

static void
test_buffer_manager_basic_cb2 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(IdeBuffer) buffer = NULL;
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeProgress) progress = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GFile) gfile = NULL;
  g_autoptr(IdeFile) file = NULL;
  IdeContext *context;
  GtkTextIter begin, end;
  g_autofree gchar *text = NULL;
  GError *error = NULL;
  int tmpfd;

  context = ide_object_get_context (IDE_OBJECT (buffer_manager));

  buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error);
  g_assert_no_error (error);
  g_assert (!buffer || IDE_IS_BUFFER (buffer));

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
  g_assert (g_str_has_prefix (text, "AC_PREREQ([2.69])\n"));

  tmpfd = g_file_open_tmp (NULL, &tmpfilename, &error);
  g_assert_no_error (error);
  g_assert_cmpint (-1, !=, tmpfd);
  close (tmpfd); /* not secure, but okay for tests */

  gfile = g_file_new_for_path (tmpfilename);
  file = ide_file_new (context, gfile);

  ide_buffer_manager_save_file_async (buffer_manager,
                                      buffer,
                                      file,
                                      &progress,
                                      g_task_get_cancellable (task),
                                      test_buffer_manager_basic_cb3,
                                      g_object_ref (task));

  g_assert (IDE_IS_PROGRESS (progress));
}

static void
test_buffer_manager_basic_cb1 (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeProgress) progress = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeBufferManager *buffer_manager;
  g_autofree gchar *path = NULL;
  GError *error = NULL;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);

  buffer_manager = ide_context_get_buffer_manager (context);
  g_signal_connect (buffer_manager, "save-buffer", G_CALLBACK (save_buffer_cb), task);
  g_signal_connect (buffer_manager, "buffer-loaded", G_CALLBACK (buffer_loaded_cb), task);

  path = g_build_filename (TEST_DATA_DIR, "project1", "configure.ac", NULL);
  file = ide_file_new_for_path (context, path);

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      FALSE,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      &progress,
                                      g_task_get_cancellable (task),
                                      test_buffer_manager_basic_cb2,
                                      g_object_ref (task));

  g_assert (IDE_IS_PROGRESS (progress));
}

static void
test_buffer_manager_basic (GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *path = NULL;
  const gchar *srcdir = g_getenv ("G_TEST_SRCDIR");
  g_autoptr(GTask) task = NULL;

  task = g_task_new (NULL, cancellable, callback, user_data);

  path = g_build_filename (srcdir, "data", "project1", "configure.ac", NULL);
  project_file = g_file_new_for_path (path);

  ide_context_new_async (project_file,
                         cancellable,
                         test_buffer_manager_basic_cb1,
                         g_object_ref (task));
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
  ide_application_add_test (app, "/Ide/BufferManager/basic", test_buffer_manager_basic, NULL, required_plugins);
  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
