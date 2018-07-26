/* gbp-meson-test-provider.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-test-provider"

#include <json-glib/json-glib.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-test.h"
#include "gbp-meson-test-provider.h"

struct _GbpMesonTestProvider
{
  IdeTestProvider  parent_instance;
  GCancellable    *build_cancellable;
  guint            reload_source;
};

G_DEFINE_TYPE (GbpMesonTestProvider, gbp_meson_test_provider, IDE_TYPE_TEST_PROVIDER)

static void
gbp_meson_test_provider_load_json (GbpMesonTestProvider *self,
                                   JsonNode             *root)
{
  JsonArray *array;
  guint length;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));
  g_assert (root != NULL);

  if (!JSON_NODE_HOLDS_ARRAY (root) || !(array = json_node_get_array (root)))
    return;

  ide_test_provider_clear (IDE_TEST_PROVIDER (self));

  length = json_array_get_length (array);

  for (guint i = 0; i < length; i++)
    {
      g_autoptr(GPtrArray) cmd = g_ptr_array_new_with_free_func (g_free);
      g_autoptr(IdeEnvironment) env = ide_environment_new ();
      g_autoptr(IdeTest) test = NULL;
      g_autoptr(GFile) workdir = NULL;
      g_auto(GStrv) environ_ = NULL;
      const gchar *name;
      const gchar *workdir_path;
      const gchar *group = NULL;
      JsonObject *obj;
      JsonArray *sub_array;
      JsonNode *sub_element;
      JsonNode *element;
      JsonNode *member;
      guint timeout = 0;

      if (NULL == (element = json_array_get_element (array, i)) ||
          !JSON_NODE_HOLDS_OBJECT (element) ||
          NULL == (obj = json_node_get_object (element)) ||
          NULL == (member = json_object_get_member (obj, "name")) ||
          !JSON_NODE_HOLDS_VALUE (member) ||
          NULL == (name = json_node_get_string (member)))
        continue;

      if (NULL != (member = json_object_get_member (obj, "timeout")) &&
          JSON_NODE_HOLDS_VALUE (member))
        timeout = json_node_get_int (member);

      if (NULL != (member = json_object_get_member (obj, "suite")) &&
          JSON_NODE_HOLDS_ARRAY (member) &&
          NULL != (sub_array = json_node_get_array (member)) &&
          json_array_get_length (sub_array) > 0 &&
          NULL != (sub_element = json_array_get_element (sub_array, 0)) &&
          JSON_NODE_HOLDS_VALUE (sub_element))
        group = json_node_get_string (sub_element);

      if (NULL != (member = json_object_get_member (obj, "workdir")) &&
          JSON_NODE_HOLDS_VALUE (member) &&
          NULL != (workdir_path = json_node_get_string (member)))
        workdir = g_file_new_for_path (workdir_path);

      if (NULL != (member = json_object_get_member (obj, "cmd")) &&
          JSON_NODE_HOLDS_ARRAY (member) &&
          NULL != (sub_array = json_node_get_array (member)))
        {
          guint cmdlen = json_array_get_length (sub_array);

          for (guint j = 0; j < cmdlen; j++)
            {
              sub_element = json_array_get_element (sub_array, j);
              if (JSON_NODE_HOLDS_VALUE (sub_element))
                {
                  const gchar *str = json_node_get_string (sub_element);

                  if (str)
                    g_ptr_array_add (cmd, g_strdup (str));
                }
            }
        }

      if (NULL != (member = json_object_get_member (obj, "env")) &&
          JSON_NODE_HOLDS_OBJECT (member) &&
          NULL != (obj = json_node_get_object (member)))
        {
          JsonObjectIter iter;
          const gchar *key;
          JsonNode *value;

          json_object_iter_init (&iter, obj);

          while (json_object_iter_next (&iter, &key, &value))
            {
              if (JSON_NODE_HOLDS_VALUE (value))
                ide_environment_setenv (env, key, json_node_get_string (value));
            }
        }

      g_ptr_array_add (cmd, NULL);

      environ_ = ide_environment_get_environ (env);
      if (ide_strv_empty0 (environ_))
        g_clear_pointer (&environ_, g_strfreev);

      test = g_object_new (GBP_TYPE_MESON_TEST,
                           "command", (gchar **)cmd->pdata,
                           "display-name", name,
                           "environ", environ_,
                           "group", group,
                           "id", name,
                           "timeout", timeout,
                           "workdir", workdir,
                           NULL);

      ide_test_provider_add (IDE_TEST_PROVIDER (self), test);
    }
}

static void
gbp_meson_test_provider_communicate_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GbpMesonTestProvider) self = user_data;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stdout_buf = NULL;
  JsonNode *root;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    IDE_GOTO (failure);

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error) ||
      NULL == (root = json_parser_get_root (parser)))
    IDE_GOTO (failure);

  gbp_meson_test_provider_load_json (self, root);

failure:
  ide_test_provider_set_loading (IDE_TEST_PROVIDER (self), FALSE);

  if (error != NULL)
    g_message ("%s", error->message);

  IDE_EXIT;
}

static void
gbp_meson_test_provider_do_reload (GbpMesonTestProvider *self,
                                   IdeBuildPipeline     *pipeline)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *builddir;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  ide_test_provider_clear (IDE_TEST_PROVIDER (self));

  if (NULL == (launcher = ide_build_pipeline_create_launcher (pipeline, &error)))
    IDE_GOTO (failure);

  ide_subprocess_launcher_set_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  builddir = ide_build_pipeline_get_builddir (pipeline);
  ide_subprocess_launcher_set_cwd (launcher, builddir);

  ide_subprocess_launcher_push_argv (launcher, "meson");
  ide_subprocess_launcher_push_argv (launcher, "introspect");
  ide_subprocess_launcher_push_argv (launcher, "--tests");
  ide_subprocess_launcher_push_argv (launcher, builddir);

  if (NULL == (subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    IDE_GOTO (failure);

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         NULL,
                                         gbp_meson_test_provider_communicate_cb,
                                         g_object_ref (self));

  IDE_EXIT;

failure:
  ide_test_provider_set_loading (IDE_TEST_PROVIDER (self), FALSE);

  if (error != NULL)
    g_message ("%s", error->message);

  IDE_EXIT;
}

static void
gbp_meson_test_provider_build_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  g_autoptr(GbpMesonTestProvider) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));

  if (!ide_build_pipeline_build_finish (pipeline, result, &error))
    {
      g_message ("%s", error->message);
      ide_test_provider_set_loading (IDE_TEST_PROVIDER (self), FALSE);
      IDE_EXIT;
    }

  gbp_meson_test_provider_do_reload (self, pipeline);

  IDE_EXIT;
}

static gboolean
gbp_meson_test_provider_reload (gpointer user_data)
{
  GbpMesonTestProvider *self = user_data;
  IdeBuildPipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));

  self->reload_source = 0;

  /* Cancel any other builds in-flight */
  g_cancellable_cancel (self->build_cancellable);
  g_clear_object (&self->build_cancellable);

  /*
   * Check that we're working with a meson build system.
   */
  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);
  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    IDE_RETURN (G_SOURCE_REMOVE);

  /*
   * Get access to the pipeline so we can create a launcher to
   * introspect meson from within the build environment.
   */
  build_manager = ide_context_get_build_manager (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  if (pipeline == NULL)
    IDE_RETURN (G_SOURCE_REMOVE);

  ide_test_provider_set_loading (IDE_TEST_PROVIDER (self), TRUE);

  /*
   * Make sure that the build pipeline has advanced enough for
   * us to continue processing the tests.
   */
  self->build_cancellable = g_cancellable_new ();
  /*
   * TODO: We want to try to avoid the pipeline build like this in
   * the future because it advances the pipeline when the project
   * is opened. Instead, we might want a CONFIGURE stage that auto
   * generates the info, and then watch that GFile.
   *
   * But to do that well, we need to coordinate with the panel to
   * be lazy about fetching unit tests until the panel is displayed.
   */
  ide_build_pipeline_build_async (pipeline,
                                  IDE_BUILD_PHASE_CONFIGURE,
                                  self->build_cancellable,
                                  gbp_meson_test_provider_build_cb,
                                  g_object_ref (self));

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
gbp_meson_test_provider_queue_reload (IdeTestProvider *provider)
{
  GbpMesonTestProvider *self = (GbpMesonTestProvider *)provider;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));

  dzl_clear_source (&self->reload_source);
  self->reload_source = gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                                      2000,
                                                      gbp_meson_test_provider_reload,
                                                      self,
                                                      NULL);
}

static void
gbp_meson_test_provider_notify_pipeline (GbpMesonTestProvider *self,
                                         GParamSpec           *pspec,
                                         IdeBuildManager      *build_manager)
{
  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));

  gbp_meson_test_provider_queue_reload (IDE_TEST_PROVIDER (self));
}

static void
gbp_meson_test_provider_run_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeRunner *runner = (IdeRunner *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  IdeTest *test;

  g_assert (IDE_IS_RUNNER (runner));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  test = ide_task_get_task_data (task);
  g_assert (IDE_IS_TEST (test));

  if (!ide_runner_run_finish (runner, result, &error))
    {
      ide_test_set_status (test, IDE_TEST_STATUS_FAILED);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_test_set_status (test, IDE_TEST_STATUS_SUCCESS);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_meson_test_provider_run_build_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeBuildPipeline *pipeline = (IdeBuildPipeline *)object;
  g_autoptr(IdeRunner) runner = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GCancellable *cancellable;
  const gchar * const *command;
  const gchar * const *environ_;
  const gchar *builddir;
  IdeRuntime *runtime;
  IdeTest *test;
  GFile *workdir;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_build_pipeline_build_finish (pipeline, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Create a runner to execute the test within */
  runtime = ide_build_pipeline_get_runtime (pipeline);
  runner = ide_runtime_create_runner (runtime, NULL);

  if (runner == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to create runner for executing unit test");
      IDE_EXIT;
    }

  test = ide_task_get_task_data (task);
  cancellable = ide_task_get_cancellable (task);

  g_assert (IDE_IS_TEST (test));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_runner_set_flags (runner,
                        (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                         G_SUBPROCESS_FLAGS_STDERR_PIPE));

  /* Default to running from builddir */
  builddir = ide_build_pipeline_get_builddir (pipeline);
  ide_runner_set_cwd (runner, builddir);

  /* And override of the test requires it */
  workdir = gbp_meson_test_get_workdir (GBP_MESON_TEST (test));
  if (workdir != NULL)
    {
      g_autofree gchar *path = g_file_get_path (workdir);
      ide_runner_set_cwd (runner, path);
    }

  /* Set our command as specified by meson */
  command = gbp_meson_test_get_command (GBP_MESON_TEST (test));
  ide_runner_push_args (runner, command);

  /* Make sure the environment is respected */
  if ((environ_ = gbp_meson_test_get_environ (GBP_MESON_TEST (test))))
    {
      IdeEnvironment *dest = ide_runner_get_environment (runner);

      for (guint i = 0; environ_[i] != NULL; i++)
        {
          g_autofree gchar *key = NULL;
          g_autofree gchar *value = NULL;

          if (ide_environ_parse (environ_[i], &key, &value))
            ide_environment_setenv (dest, key, value);
        }
    }

  ide_test_set_status (test, IDE_TEST_STATUS_RUNNING);

  ide_runner_run_async (runner,
                        cancellable,
                        gbp_meson_test_provider_run_cb,
                        g_steal_pointer (&task));

  IDE_EXIT;
}

static void
gbp_meson_test_provider_run_async (IdeTestProvider     *provider,
                                   IdeTest             *test,
                                   IdeBuildPipeline    *pipeline,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GbpMesonTestProvider *self = (GbpMesonTestProvider *)provider;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));
  g_assert (GBP_IS_MESON_TEST (test));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_test_provider_run_async);
  ide_task_set_task_data (task, g_object_ref (test), g_object_unref);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /* Currently, we don't have a way to determine what targets
   * need to be built before the test can run, so we must build
   * the entire project up to the build phase.
   */

  ide_build_pipeline_build_async (pipeline,
                                  IDE_BUILD_PHASE_BUILD,
                                  cancellable,
                                  gbp_meson_test_provider_run_build_cb,
                                  g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_meson_test_provider_run_finish (IdeTestProvider  *provider,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  g_assert (IDE_IS_TEST_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_meson_test_provider_constructed (GObject *object)
{
  GbpMesonTestProvider *self = (GbpMesonTestProvider *)object;
  IdeBuildManager *build_manager;
  IdeContext *context;

  g_assert (GBP_IS_MESON_TEST_PROVIDER (self));

  G_OBJECT_CLASS (gbp_meson_test_provider_parent_class)->constructed (object);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);

  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (gbp_meson_test_provider_notify_pipeline),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_meson_test_provider_notify_pipeline (self, NULL, build_manager);
}

static void
gbp_meson_test_provider_dispose (GObject *object)
{
  GbpMesonTestProvider *self = (GbpMesonTestProvider *)object;

  dzl_clear_source (&self->reload_source);
  g_cancellable_cancel (self->build_cancellable);
  g_clear_object (&self->build_cancellable);

  G_OBJECT_CLASS (gbp_meson_test_provider_parent_class)->dispose (object);
}

static void
gbp_meson_test_provider_class_init (GbpMesonTestProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTestProviderClass *provider_class = IDE_TEST_PROVIDER_CLASS (klass);

  object_class->constructed = gbp_meson_test_provider_constructed;
  object_class->dispose = gbp_meson_test_provider_dispose;

  provider_class->run_async = gbp_meson_test_provider_run_async;
  provider_class->run_finish = gbp_meson_test_provider_run_finish;
  provider_class->reload = gbp_meson_test_provider_queue_reload;
}

static void
gbp_meson_test_provider_init (GbpMesonTestProvider *self)
{
}
