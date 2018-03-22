/* test-ide-runtime.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "test-ide-runtime"

#include <ide.h>

#include "application/ide-application-tests.h"
#include "plugins/gnome-builder-plugins.h"

static void
context_loaded (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeRuntimeManager *runtime_manager;
  IdeRuntime *runtime;
  g_autofree gchar *arch = NULL;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  runtime_manager = ide_context_get_runtime_manager (context);
  g_assert_nonnull (runtime_manager);
  g_assert (IDE_IS_RUNTIME_MANAGER (runtime_manager));

  runtime = ide_runtime_manager_get_runtime (runtime_manager, "host");
  g_assert_nonnull (runtime);
  g_assert (IDE_IS_RUNTIME (runtime));

  arch = ide_runtime_get_arch (runtime);
  g_assert_nonnull (arch);
  g_assert_cmpstr (arch, ==, ide_get_system_arch ());

  g_task_return_boolean (task, TRUE);
}

static void
test_runtime (GCancellable        *cancellable,
              GAsyncReadyCallback  callback,
              gpointer             user_data)
{
  g_autoptr(GFile) project_file = NULL;
  g_autofree gchar *path = NULL;
  const gchar *srcdir = g_getenv ("G_TEST_SRCDIR");
  g_autoptr(GTask) task = NULL;

  task = g_task_new (NULL, cancellable, callback, user_data);

  path = g_build_filename (srcdir, "data", "project1", NULL);
  project_file = g_file_new_for_path (path);

  ide_context_new_async (project_file,
                         cancellable,
                         context_loaded,
                         g_steal_pointer (&task));
}

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "autotools-plugin", "buildconfig", "directory-plugin", NULL };
  g_autoptr(IdeApplication) app = NULL;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new (IDE_APPLICATION_MODE_TESTS);
  ide_application_add_test (app, "/Ide/Runtime/basic", test_runtime, NULL, required_plugins);
  gnome_builder_plugins_init ();

  return g_application_run (G_APPLICATION (app), argc, argv);
}
