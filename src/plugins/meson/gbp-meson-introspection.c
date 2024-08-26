/* gbp-meson-introspection.c
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

#define G_LOG_DOMAIN "gbp-meson-introspection"

#include "config.h"

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-introspection.h"

struct _GbpMesonIntrospection
{
  IdePipelineStage parent_instance;

  IdePipeline *pipeline;

  char *etag;

  GListStore *run_commands;

  char *descriptive_name;
  char *subproject_dir;
  char *version;

  guint loaded : 1;
  guint has_built_once : 1;
};

G_DEFINE_FINAL_TYPE (GbpMesonIntrospection, gbp_meson_introspection, IDE_TYPE_PIPELINE_STAGE)

static gboolean
get_bool_member (JsonObject *object,
                 const char *member,
                 gboolean   *location)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);

  *location = FALSE;

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_VALUE (node))
    {
      *location = json_node_get_boolean (node);
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_string_member (JsonObject  *object,
                   const char  *member,
                   char       **location)
{
  JsonNode *node;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);

  g_clear_pointer (location, g_free);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_VALUE (node))
    {
      *location = g_strdup (json_node_get_string (node));
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_strv_member (JsonObject   *object,
                 const char   *member,
                 char       ***location)
{
  JsonNode *node;
  JsonArray *ar;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);

  g_clear_pointer (location, g_strfreev);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_ARRAY (node) &&
      (ar = json_node_get_array (node)))
    {
      GPtrArray *strv = g_ptr_array_new ();
      guint n_items = json_array_get_length (ar);

      for (guint i = 0; i < n_items; i++)
        {
          JsonNode *ele = json_array_get_element (ar, i);
          const char *str;

          if (JSON_NODE_HOLDS_VALUE (ele) &&
              (str = json_node_get_string (ele)))
            g_ptr_array_add (strv, g_strdup (str));
        }

      g_ptr_array_add (strv, NULL);

      *location = (char **)(gpointer)g_ptr_array_free (strv, FALSE);

      return TRUE;
    }

  return FALSE;
}

static gboolean
get_environ_member (JsonObject   *object,
                    const char   *member,
                    char       ***location)
{
  JsonNode *node;
  JsonObject *envobj;

  g_assert (object != NULL);
  g_assert (member != NULL);
  g_assert (location != NULL);
  g_assert (*location == NULL);

  if (json_object_has_member (object, member) &&
      (node = json_object_get_member (object, member)) &&
      JSON_NODE_HOLDS_OBJECT (node) &&
      (envobj = json_node_get_object (node)))
    {
      JsonObjectIter iter;
      const char *key;
      JsonNode *value_node;

      json_object_iter_init (&iter, envobj);
      while (json_object_iter_next (&iter, &key, &value_node))
        {
          const char *value;

          if (!JSON_NODE_HOLDS_VALUE (value_node) ||
              !(value = json_node_get_string (value_node)))
            continue;

          *location = g_environ_setenv (*location, key, value, TRUE);
        }

      return TRUE;
    }

  return FALSE;
}

static void
gbp_meson_introspection_load_buildoptions (GbpMesonIntrospection *self,
                                           JsonArray             *buildoptions)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (buildoptions != NULL);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_projectinfo (GbpMesonIntrospection *self,
                                          JsonObject            *projectinfo)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (projectinfo != NULL);

  get_string_member (projectinfo, "version", &self->version);
  get_string_member (projectinfo, "descriptive_name", &self->descriptive_name);
  get_string_member (projectinfo, "subproject_dir", &self->subproject_dir);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_test (GbpMesonIntrospection *self,
                                   JsonObject            *test)
{
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_auto(GStrv) cmd = NULL;
  g_auto(GStrv) env = NULL;
  g_auto(GStrv) suite = NULL;
  g_autofree char *name = NULL;
  g_autofree char *workdir = NULL;
  g_autofree char *id = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (test != NULL);

  get_strv_member (test, "cmd", &cmd);
  get_strv_member (test, "suite", &suite);
  get_environ_member (test, "env", &env);
  get_string_member (test, "name", &name);
  get_string_member (test, "workdir", &workdir);

  if (workdir == NULL)
    workdir = g_strdup (ide_pipeline_get_builddir (self->pipeline));

  id = g_strdup_printf ("meson:%s", name);

  run_command = ide_run_command_new ();
  ide_run_command_set_id (run_command, id);
  ide_run_command_set_kind (run_command, IDE_RUN_COMMAND_KIND_TEST);
  ide_run_command_set_display_name (run_command, name);
  ide_run_command_set_environ (run_command, (const char * const *)env);
  ide_run_command_set_argv (run_command, (const char * const *)cmd);
  ide_run_command_set_cwd (run_command, workdir);
  ide_run_command_set_can_default (run_command, FALSE);

  g_list_store_append (self->run_commands, run_command);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_tests (GbpMesonIntrospection *self,
                                    JsonArray             *tests)
{
  guint n_items;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (tests != NULL);

  n_items = json_array_get_length (tests);

  for (guint i = 0; i < n_items; i++)
    {
      JsonNode *node = json_array_get_element (tests, i);
      JsonObject *obj;

      if (node != NULL &&
          JSON_NODE_HOLDS_OBJECT (node) &&
          (obj = json_node_get_object (node)))
        gbp_meson_introspection_load_test (self, obj);
    }

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_benchmarks (GbpMesonIntrospection *self,
                                         JsonArray             *benchmarks)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (benchmarks != NULL);

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_targets (GbpMesonIntrospection *self,
                                      JsonArray             *targets)
{
  guint length;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (targets != NULL);

  length = json_array_get_length (targets);

  for (guint i = 0; i < length; i++)
    {
      JsonNode *node = json_array_get_element (targets, i);
      g_autofree char *id = NULL;
      g_autofree char *name = NULL;
      g_autofree char *type = NULL;
      JsonObject *obj;

      if (!JSON_NODE_HOLDS_OBJECT (node) || !(obj = json_node_get_object (node)))
        continue;

      get_string_member (obj, "id", &id);
      get_string_member (obj, "name", &name);
      get_string_member (obj, "type", &type);

      if (ide_str_equal0 (type, "executable") || ide_str_equal0 (type, "custom"))
        {
          g_auto(GStrv) filename = NULL;
          gboolean installed = FALSE;

          get_strv_member (obj, "filename", &filename);
          get_bool_member (obj, "installed", &installed);

          if (!ide_str_empty0 (filename))
            {
              g_autoptr(IdeRunCommand) run_command = NULL;
              g_auto(GStrv) install_filename = NULL;
              g_autofree char *install_dir = NULL;

              if (get_strv_member (obj, "install_filename", &install_filename) &&
                  install_filename != NULL &&
                  install_filename[0] != NULL)
                install_dir = g_path_get_dirname (install_filename[0]);

              /* Ignore custom unless it's installed to somewhere/bin/ */
              if (ide_str_equal0 (type, "custom") &&
                  (install_dir == NULL || !g_str_has_suffix (install_dir, "/bin")))
                continue;

              run_command = ide_run_command_new ();

              /* Setup basics for run command information */
              ide_run_command_set_kind (run_command, IDE_RUN_COMMAND_KIND_UTILITY);
              ide_run_command_set_id (run_command, id);
              ide_run_command_set_display_name (run_command, name);

              /* Only allow automatic discovery if it's installed */
              ide_run_command_set_can_default (run_command, installed);

              /* Use installed path if it's provided. */
              if (install_filename != NULL && install_filename[0] != NULL)
                ide_run_command_set_argv (run_command, IDE_STRV_INIT (install_filename[0]));
              else
                ide_run_command_set_argv (run_command, IDE_STRV_INIT (filename[0]));

              /* Lower priority for any executable not installed to somewhere/bin/ */
              if (install_dir != NULL && g_str_has_suffix (install_dir, "/bin"))
                ide_run_command_set_priority (run_command, 0);
              else
                ide_run_command_set_priority (run_command, 1000);

              g_list_store_append (self->run_commands, run_command);
            }
        }
    }

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_installed (GbpMesonIntrospection *self,
                                        JsonObject            *installed)
{
  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (installed != NULL);

  IDE_EXIT;
}

static char *
get_current_etag (IdePipeline *pipeline)
{
  g_autofree char *build_dot_ninja = NULL;
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_PIPELINE (pipeline));

  build_dot_ninja = ide_pipeline_build_builddir_path (pipeline, "build.ninja", NULL);
  file = g_file_new_for_path (build_dot_ninja);
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_ETAG_VALUE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL, NULL);

  if (info == NULL)
    return NULL;

  return g_strdup (g_file_info_get_etag (info));
}

static void
gbp_meson_introspection_query (IdePipelineStage *stage,
                               IdePipeline      *pipeline,
                               GPtrArray        *targets,
                               GCancellable     *cancellable)
{
  GbpMesonIntrospection *self = (GbpMesonIntrospection *)stage;
  g_autofree char *etag = NULL;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  etag = get_current_etag (pipeline);

  ide_pipeline_stage_set_completed (stage,
                                    ide_str_equal0 (etag, self->etag));
}

static void
gbp_meson_introspection_load_json (GbpMesonIntrospection *self,
                                   JsonObject            *root)
{
  JsonNode *member;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (root != NULL);

  if (json_object_has_member (root, "buildoptions") &&
      (member = json_object_get_member (root, "buildoptions")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_buildoptions (self, json_node_get_array (member));

  if (json_object_has_member (root, "projectinfo") &&
      (member = json_object_get_member (root, "projectinfo")) &&
      JSON_NODE_HOLDS_OBJECT (member))
    gbp_meson_introspection_load_projectinfo (self, json_node_get_object (member));

  if (json_object_has_member (root, "tests") &&
      (member = json_object_get_member (root, "tests")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_tests (self, json_node_get_array (member));

  if (json_object_has_member (root, "benchmarks") &&
      (member = json_object_get_member (root, "benchmarks")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_benchmarks (self, json_node_get_array (member));

  if (json_object_has_member (root, "installed") &&
      (member = json_object_get_member (root, "installed")) &&
      JSON_NODE_HOLDS_OBJECT (member))
    gbp_meson_introspection_load_installed (self, json_node_get_object (member));

  if (json_object_has_member (root, "targets") &&
      (member = json_object_get_member (root, "targets")) &&
      JSON_NODE_HOLDS_ARRAY (member))
    gbp_meson_introspection_load_targets (self, json_node_get_array (member));

  IDE_EXIT;
}

static void
gbp_meson_introspection_load_stream_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  JsonParser *parser = (JsonParser *)object;
  GbpMesonIntrospection *self;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  const char *etag;
  JsonObject *obj;
  JsonNode *root;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (JSON_IS_PARSER (parser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!json_parser_load_from_stream_finish (parser, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);
  etag = ide_task_get_task_data (task);

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (etag != NULL);

  /* Clear all of our previously loaded state */
  g_set_str (&self->etag, etag);
  g_list_store_remove_all (self->run_commands);

  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_OBJECT (root) &&
      (obj = json_node_get_object (root)))
    gbp_meson_introspection_load_json (self, obj);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_meson_introspection_build_async (IdePipelineStage    *stage,
                                     IdePipeline         *pipeline,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GbpMesonIntrospection *self = (GbpMesonIntrospection *)stage;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GIOStream) io_stream = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *meson = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->has_built_once = TRUE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_introspection_build_async);
  ide_task_set_task_data (task, get_current_etag (pipeline), g_free);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);
  meson = gbp_meson_build_system_locate_meson (GBP_MESON_BUILD_SYSTEM (build_system), pipeline);

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (GBP_IS_MESON_BUILD_SYSTEM (build_system));
  g_assert (meson != NULL);

  run_context = ide_run_context_new ();
  ide_pipeline_prepare_run_context (pipeline, run_context);
  ide_run_context_append_args (run_context, IDE_STRV_INIT (meson, "introspect", "--all", "--force-object-output"));

  /* Create a stream to communicate with the subprocess and then spawn it */
  if (!(io_stream = ide_run_context_create_stdio_stream (run_context, &error)) ||
      !(subprocess = ide_run_context_spawn (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  /* Keep stream alive for duration of operation */
  g_object_set_data_full (G_OBJECT (task),
                          "IO_STREAM",
                          g_object_ref (io_stream),
                          g_object_unref);

  /* Start parsing our input stream */
  parser = json_parser_new ();
  json_parser_load_from_stream_async (parser,
                                      g_io_stream_get_input_stream (io_stream),
                                      cancellable,
                                      gbp_meson_introspection_load_stream_cb,
                                      g_steal_pointer (&task));

  /* Make sure something watches the child */
  ide_subprocess_wait_async (subprocess, NULL, NULL, NULL);

  IDE_EXIT;
}

static gboolean
gbp_meson_introspection_build_finish (IdePipelineStage  *stage,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (stage));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
gbp_meson_introspection_destroy (IdeObject *object)
{
  GbpMesonIntrospection *self = (GbpMesonIntrospection *)object;

  g_clear_object (&self->run_commands);

  g_clear_pointer (&self->descriptive_name, g_free);
  g_clear_pointer (&self->subproject_dir, g_free);
  g_clear_pointer (&self->version, g_free);
  g_clear_pointer (&self->etag, g_free);

  g_clear_weak_pointer (&self->pipeline);

  IDE_OBJECT_CLASS (gbp_meson_introspection_parent_class)->destroy (object);
}

static void
gbp_meson_introspection_class_init (GbpMesonIntrospectionClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdePipelineStageClass *pipeline_stage_class = IDE_PIPELINE_STAGE_CLASS (klass);

  i_object_class->destroy = gbp_meson_introspection_destroy;

  pipeline_stage_class->query = gbp_meson_introspection_query;
  pipeline_stage_class->build_async = gbp_meson_introspection_build_async;
  pipeline_stage_class->build_finish = gbp_meson_introspection_build_finish;
}

static void
gbp_meson_introspection_init (GbpMesonIntrospection *self)
{
  self->run_commands = g_list_store_new (IDE_TYPE_RUN_COMMAND);

  ide_pipeline_stage_set_name (IDE_PIPELINE_STAGE (self),
                               _("Load Meson Introspection"));
}

GbpMesonIntrospection *
gbp_meson_introspection_new (IdePipeline *pipeline)
{
  GbpMesonIntrospection *self;

  g_return_val_if_fail (IDE_IS_PIPELINE (pipeline), NULL);

  self = g_object_new (GBP_TYPE_MESON_INTROSPECTION, NULL);
  g_set_weak_pointer (&self->pipeline, pipeline);

  return self;
}

static void
gbp_meson_introspection_list_run_commands_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  g_autoptr(IdeTask) task = user_data;
  GbpMesonIntrospection *self;

  IDE_ENTRY;

  g_assert (IDE_IS_PIPELINE (object) || IDE_IS_PIPELINE_STAGE (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (GBP_IS_MESON_INTROSPECTION (self));

  ide_task_return_pointer (task, g_object_ref (self->run_commands), g_object_unref);

  IDE_EXIT;
}

void
gbp_meson_introspection_list_run_commands_async (GbpMesonIntrospection *self,
                                                 GCancellable          *cancellable,
                                                 GAsyncReadyCallback    callback,
                                                 gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_MESON_INTROSPECTION (self));
  g_return_if_fail (IDE_IS_PIPELINE (self->pipeline));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_introspection_list_run_commands_async);

  if (!self->has_built_once)
    {
      g_autofree char *build_dot_ninja = ide_pipeline_build_builddir_path (self->pipeline, "build.ninja", NULL);

      /* If there is a build.ninja then assume we can skip running through
       * the pipeline and just introspection immediately.
       */
      if (g_file_test (build_dot_ninja, G_FILE_TEST_EXISTS))
        ide_pipeline_stage_build_async (IDE_PIPELINE_STAGE (self),
                                        self->pipeline,
                                        cancellable,
                                        gbp_meson_introspection_list_run_commands_cb,
                                        g_steal_pointer (&task));
      else
        ide_pipeline_build_async (self->pipeline,
                                  IDE_PIPELINE_PHASE_CONFIGURE,
                                  cancellable,
                                  gbp_meson_introspection_list_run_commands_cb,
                                  g_steal_pointer (&task));

      IDE_EXIT;
    }

  ide_task_return_pointer (task, g_object_ref (self->run_commands), g_object_unref);

  IDE_EXIT;
}

GListModel *
gbp_meson_introspection_list_run_commands_finish (GbpMesonIntrospection  *self,
                                                  GAsyncResult           *result,
                                                  GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_INTROSPECTION (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}
