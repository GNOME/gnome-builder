/* gbp-meson-build-target-provider.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-meson-build-target-provider"

#include <json-glib/json-glib.h>

#include "gbp-meson-build-system.h"
#include "gbp-meson-build-target.h"
#include "gbp-meson-build-target-provider.h"

struct _GbpMesonBuildTargetProvider
{
  IdeObject parent_instance;
};

static const gchar *
get_first_string (JsonNode *member)
{
  if (JSON_NODE_HOLDS_VALUE (member))
    return json_node_get_string (member);

  if (JSON_NODE_HOLDS_ARRAY (member))
    {
      JsonArray *ar = json_node_get_array (member);

      if (json_array_get_length (ar) > 0)
        {
          JsonNode *ele = json_array_get_element (ar, 0);
          return get_first_string (ele);
        }
    }

  return NULL;
}

static void
gbp_meson_build_target_provider_communicate_cb (GObject      *object,
                                                GAsyncResult *result,
                                                gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  GbpMesonBuildTargetProvider *self;
  g_autofree gchar *stdout_buf = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GFile) builddir = NULL;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  IdeContext *context;
  JsonArray *array;
  JsonNode *root;
  guint len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /*
   * The output from meson introspect --targets is a JSON formatted array
   * of objects containing target information.
   */

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (NULL == (root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_ARRAY (root) ||
      NULL == (array = json_node_get_array (root)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_DATA,
                                 "Invalid JSON received from meson introspect");
      return;
    }

  self = ide_task_get_source_object (task);
  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  builddir = g_file_new_for_path (ide_pipeline_get_builddir (pipeline));

  len = json_array_get_length (array);
  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < len; i++)
    {
      JsonNode *element = json_array_get_element (array, i);
      const gchar *name;
      const gchar *filename;
      const gchar *type;
      JsonObject *obj;
      JsonNode *member;

      if (JSON_NODE_HOLDS_OBJECT (element) &&
          NULL != (obj = json_node_get_object (element)) &&
          NULL != (member = json_object_get_member (obj, "name")) &&
          JSON_NODE_HOLDS_VALUE (member) &&
          NULL != (name = json_node_get_string (member)) &&
          NULL != (member = json_object_get_member (obj, "filename")) &&
          NULL != (filename = get_first_string (member)) &&
          NULL != (member = json_object_get_member (obj, "type")) &&
          JSON_NODE_HOLDS_VALUE (member) &&
          NULL != (type = json_node_get_string (member)))
        {
          g_autoptr(IdeBuildTarget) target = NULL;
          g_autofree char *base = NULL;
          g_autofree char *dir_path = NULL;
          g_autoptr(GFile) file = NULL;
          g_autoptr(GFile) dir = NULL;
          IdeArtifactKind kind = 0;

          g_debug ("Found target %s", name);

          file = g_file_new_for_path (filename);
          base = g_file_get_relative_path (builddir, file);
          dir_path = g_path_get_dirname (filename);
          dir = g_file_new_for_path (dir_path);

          if (ide_str_equal0 (type, "executable"))
            kind = IDE_ARTIFACT_KIND_EXECUTABLE;
          else if (ide_str_equal0 (type, "static library"))
            kind = IDE_ARTIFACT_KIND_STATIC_LIBRARY;
          else if (ide_str_equal0 (type, "shared library"))
            kind = IDE_ARTIFACT_KIND_SHARED_LIBRARY;

          target = gbp_meson_build_target_new (context, dir, base, filename, kind);

          g_ptr_array_add (ret, g_steal_pointer (&target));
        }
    }

  ide_task_return_pointer (task, g_steal_pointer (&ret), g_ptr_array_unref);
}

static void
gbp_meson_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                   GCancellable           *cancellable,
                                                   GAsyncReadyCallback     callback,
                                                   gpointer                user_data)
{
  GbpMesonBuildTargetProvider *self = (GbpMesonBuildTargetProvider *)provider;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunCommand) run_command = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = NULL;
  IdePipeline *pipeline;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_meson_build_target_provider_get_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_MESON_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a meson build system, ignoring");
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Build pipeline is not ready, cannot extract targets");
      IDE_EXIT;
    }

  run_command = ide_run_command_new ();
  ide_run_command_set_argv (run_command, IDE_STRV_INIT ("meson", "introspect", "--targets", ide_pipeline_get_builddir (pipeline)));
  run_context = ide_pipeline_create_run_context (pipeline, run_command);
  launcher = ide_run_context_end (run_context, &error);
  ide_subprocess_launcher_set_flags (launcher, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_meson_build_target_provider_communicate_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static GPtrArray *
gbp_meson_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                    GAsyncResult            *result,
                                                    GError                 **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_MESON_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

#ifdef IDE_ENABLE_TRACE
  if (ret != NULL)
    {
      IDE_TRACE_MSG ("Discovered %u targets", ret->len);

      for (guint i = 0; i < ret->len; i++)
        {
          IdeBuildTarget *target = g_ptr_array_index (ret, i);
          g_autofree gchar *name = NULL;

          g_assert (GBP_IS_MESON_BUILD_TARGET (target));
          g_assert (IDE_IS_BUILD_TARGET (target));

          name = ide_build_target_get_name (target);
          IDE_TRACE_MSG ("[%u]: %s", i, name);
        }
    }
#endif

  IDE_RETURN (IDE_PTR_ARRAY_STEAL_FULL (&ret));
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = gbp_meson_build_target_provider_get_targets_async;
  iface->get_targets_finish = gbp_meson_build_target_provider_get_targets_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpMesonBuildTargetProvider,
                         gbp_meson_build_target_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER,
                                                build_target_provider_iface_init))

static void
gbp_meson_build_target_provider_class_init (GbpMesonBuildTargetProviderClass *klass)
{
}

static void
gbp_meson_build_target_provider_init (GbpMesonBuildTargetProvider *self)
{
}
