/* test-ide-build-pipeline.c
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "test-ide-build-pipeline"

#include <ide.h>

#include "application/ide-application-tests.h"
#include "plugins/gnome-builder-plugins.h"

static void
execute_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  gint r;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  g_debug ("Pipeline callback of completion");

  r = ide_build_pipeline_execute_finish (pipeline, result, &error);
  g_assert_no_error (error);
  g_assert_cmpint (r, ==, TRUE);

  g_task_return_boolean (task, TRUE);
}

static void
context_loaded (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeBuildPipeline) pipeline = NULL;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;

  context = ide_context_new_finish (result, &error);
  g_assert_no_error (error);
  g_assert (context != NULL);
  g_assert (IDE_IS_CONTEXT (context));

  config_manager = ide_context_get_configuration_manager (context);
  g_assert (IDE_IS_CONFIGURATION_MANAGER (config_manager));

  config = ide_configuration_manager_get_current (config_manager);
  g_assert (IDE_IS_CONFIGURATION (config));

  pipeline = g_object_new (IDE_TYPE_BUILD_PIPELINE,
                           "context", context,
                           "configuration", config,
                           NULL);

  ide_build_pipeline_request_phase (pipeline, IDE_BUILD_PHASE_BUILD);

  g_debug ("Executing pipeline");

  ide_build_pipeline_execute_async (pipeline,
                                    NULL,
                                    execute_cb,
                                    g_steal_pointer (&task));
}

static void
test_build_pipeline (GCancellable        *cancellable,
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
  IdeApplication *app;
  gint ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new ();
  ide_application_add_test (app, "/Ide/BuildPipeline/basic", test_build_pipeline, NULL);
  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return ret;
}
