/* test-ide-context.c
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

#include <ide.h>

#include "application/ide-application-tests.h"
#include "../plugins/gnome-builder-plugins.h"

typedef gboolean (*TestCallback) (IdeContext  *context,
                                  GError     **error);

static void
test_new_async_cb1 (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  TestCallback callback;
  IdeBuildSystem *bs;
  IdeVcs *vcs;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);

  bs = ide_context_get_build_system (context);
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (bs), ==, "IdeAutotoolsBuildSystem");

  vcs = ide_context_get_vcs (context);
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (vcs), ==, "IdeDirectoryVcs");

  callback = g_task_get_task_data (task);

  if (callback != NULL)
    {
      gboolean r = callback (context, &error);
      g_assert_no_error (error);
      g_assert_true (r);
    }

  g_task_return_boolean (task, TRUE);
}

static void
test_with_callback (TestCallback         test_func,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) project_file = NULL;
  GTask *task;

  /* user_data is a function callback or NULL, to
   * execute after loading */
  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_task_data (task, test_func, NULL);
  path = g_build_filename (TEST_DATA_DIR, "project1", "configure.ac", NULL);
  project_file = g_file_new_for_path (path);
  ide_context_new_async (project_file, cancellable, test_new_async_cb1, task);
}

#define ADD_TEST_FUNC(name, func)                              \
static void                                                    \
test_##name (GCancellable        *cancellable,                 \
             GAsyncReadyCallback  callback,                    \
             gpointer             user_data)                   \
{                                                              \
  test_with_callback (func, cancellable, callback, user_data); \
}
ADD_TEST_FUNC (new_async, NULL)
#undef ADD_TEST_FUNC

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "autotools-plugin", "directory-plugin", NULL };
  g_autoptr(IdeApplication) app = NULL;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();

#define ADD_TEST_FUNC(name) \
  ide_application_add_test (app, "/Ide/Context/"#name, test_##name, NULL, required_plugins);
ADD_TEST_FUNC(new_async)
#undef ADD_TEST_FUNC

  gnome_builder_plugins_init ();

  return g_application_run (G_APPLICATION (app), argc, argv);
}
