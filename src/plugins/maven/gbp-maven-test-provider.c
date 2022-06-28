/* gbp-maven-test-provider.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-maven-test-provider"

#include "config.h"

#include <libide-foundry.h>
#include <libide-io.h>
#include <libide-threading.h>

#include "gbp-maven-build-system.h"
#include "gbp-maven-test.h"
#include "gbp-maven-test-provider.h"

struct _GbpMavenTestProvider
{
  IdeTestProvider parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpMavenTestProvider, gbp_maven_test_provider, IDE_TYPE_TEST_PROVIDER)

static void
gbp_maven_test_provider_add (GbpMavenTestProvider *self,
                             GFile                *file,
                             const char           *test_name)
{
  g_autoptr(GbpMavenTest) test = NULL;
  g_autofree char *class_name = NULL;
  g_autofree char *full_name = NULL;
  g_autofree char *id = NULL;
  char *dot;

  g_assert (GBP_IS_MAVEN_TEST_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (test_name != NULL);

  class_name = g_file_get_basename (file);
  if ((dot = strrchr (class_name, '.')))
    *dot = 0;

  full_name = g_strconcat (class_name, "#", test_name, NULL);
  id = g_strconcat ("maven:%s", full_name, NULL);
  test = gbp_maven_test_new (full_name);

  ide_test_set_id (IDE_TEST (test), id);
  ide_test_set_group (IDE_TEST (test), class_name);
  ide_test_set_display_name (IDE_TEST (test), test_name);

  ide_test_provider_add (IDE_TEST_PROVIDER (self), IDE_TEST (test));
}

static void
find_test_files_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *basedir = (GFile *)object;
  g_autoptr(GbpMavenTestProvider) self = user_data;
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (G_IS_FILE (basedir));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_MAVEN_TEST_PROVIDER (self));

  if (!(files = ide_g_file_find_finish (basedir, result, &error)))
    {
      g_debug ("Failed to find test files: %s", error->message);
      IDE_GOTO (failure);
    }

  ide_test_provider_clear (IDE_TEST_PROVIDER (self));

  for (guint i = 0; i < files->len; i++)
    {
      GFile *file = g_ptr_array_index (files, i);
      g_autofree char *contents = NULL;
      IdeLineReader reader;
      char *line;
      gsize line_len;
      gsize len;

      if (!g_file_load_contents (file, NULL, &contents, &len, NULL, NULL))
        continue;

      /* Obviously this isn't a great way to find tests, but it
       * does allow for skipping any sort of introspection. Mostly
       * just copying what the python plugin did.
       */
      ide_line_reader_init (&reader, contents, len);
      while ((line = ide_line_reader_next (&reader, &line_len)))
        {
          char *name;
          char *paren;

          line[line_len] = 0;

          if (!(name = strstr (line, "public void")))
            continue;

          if (!(paren = strchr (name, '(')))
            continue;

          *paren = 0;
          name += strlen ("public void");
          g_strstrip (name);

          gbp_maven_test_provider_add (self, file, name);
        }
    }

failure:
  ide_test_provider_set_loading (IDE_TEST_PROVIDER (self), FALSE);

  IDE_EXIT;
}

static void
gbp_maven_test_provider_reload (IdeTestProvider *provider)
{
  g_autofree char *project_dir = NULL;
  g_autoptr(GFile) testdir = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MAVEN_TEST_PROVIDER (provider));

  context = ide_object_get_context (IDE_OBJECT (provider));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MAVEN_BUILD_SYSTEM (build_system))
    IDE_EXIT;

  if (ide_test_provider_get_loading (provider))
    IDE_EXIT;

  ide_test_provider_set_loading (provider, TRUE);

  project_dir = gbp_maven_build_system_get_project_dir (GBP_MAVEN_BUILD_SYSTEM (build_system));
  testdir = g_file_new_build_filename (project_dir, "src", "test", "java", NULL);

  ide_g_file_find_with_depth_async (testdir, "*.java", 5,
                                    NULL,
                                    find_test_files_cb,
                                    g_object_ref (provider));

  IDE_EXIT;
}

static void
gbp_maven_test_provider_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeTest *test;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  test = ide_task_get_task_data (task);

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    {
      ide_test_set_status (test, IDE_TEST_STATUS_FAILED);
      ide_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      ide_test_set_status (test, IDE_TEST_STATUS_SUCCESS);
      ide_task_return_boolean (task, TRUE);
    }

  IDE_EXIT;
}

static void
gbp_maven_test_provider_run_async (IdeTestProvider     *provider,
                                   IdeTest             *test,
                                   IdePipeline         *pipeline,
                                   VtePty              *pty,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *d_test = NULL;
  const char *suite_name;
  IdeRuntime *runtime;

  IDE_ENTRY;

  g_assert (GBP_IS_MAVEN_TEST_PROVIDER (provider));
  g_assert (GBP_IS_MAVEN_TEST (test));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!pty || VTE_IS_PTY (pty));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_maven_test_provider_run_async);
  ide_task_set_task_data (task, g_object_ref (test), g_object_unref);

  suite_name = gbp_maven_test_get_suite_name (GBP_MAVEN_TEST (test));

  if (!(runtime = ide_pipeline_get_runtime (pipeline)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to run test: %s",
                                 gbp_maven_test_get_suite_name (GBP_MAVEN_TEST (test)));
      IDE_EXIT;
    }

  run_context = ide_run_context_new ();
  ide_runtime_prepare_to_run (runtime, pipeline, run_context);

  if (pty != NULL)
    ide_run_context_set_pty (run_context, pty);

  ide_run_context_set_cwd (run_context, ide_pipeline_get_srcdir (pipeline));

  /*
   * http://maven.apache.org/surefire/maven-surefire-plugin/examples/single-test.html
   * it has to be junit 4.x
   */
  d_test = g_strconcat ("-Dtest=", suite_name, NULL);
  ide_run_context_append_args (run_context, IDE_STRV_INIT ("mvn", d_test, "test"));

  if (!(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_test_set_status (test, IDE_TEST_STATUS_RUNNING);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   gbp_maven_test_provider_run_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_maven_test_provider_run_finish (IdeTestProvider  *provider,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MAVEN_TEST_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_maven_test_provider_class_init (GbpMavenTestProviderClass *klass)
{
  IdeTestProviderClass *test_provider_class = IDE_TEST_PROVIDER_CLASS (klass);

  test_provider_class->reload = gbp_maven_test_provider_reload;
  test_provider_class->run_async = gbp_maven_test_provider_run_async;
  test_provider_class->run_finish = gbp_maven_test_provider_run_finish;
}

static void
gbp_maven_test_provider_init (GbpMavenTestProvider *self)
{
}
