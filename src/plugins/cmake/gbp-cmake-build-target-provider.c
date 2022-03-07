/* gbp-cmake-build-target-provider.c
 *
 * Copyright 2021-2022 Günther Wagner <info@gunibert.de>
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

#include "gbp-cmake-build-target-provider.h"
#include "gbp-cmake-build-target.h"
#include <json-glib/json-glib.h>

struct _GbpCmakeBuildTargetProvider
{
  IdeObject parent_instance;
};

static void build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpCmakeBuildTargetProvider, gbp_cmake_build_target_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_TARGET_PROVIDER, build_target_provider_iface_init))

GbpCmakeBuildTargetProvider *
gbp_cmake_build_target_provider_new (void)
{
  return g_object_new (GBP_TYPE_CMAKE_BUILD_TARGET_PROVIDER, NULL);
}

static void
gbp_cmake_build_target_provider_class_init (GbpCmakeBuildTargetProviderClass *klass)
{
}

static void
gbp_cmake_build_target_provider_init (GbpCmakeBuildTargetProvider *self)
{
}

static void
gbp_cmake_build_target_provider_create_target (GbpCmakeBuildTargetProvider  *self,
                                               GPtrArray                   **ret,
                                               IdeContext                   *context,
                                               JsonObject                   *obj)
{
  g_autoptr(IdeBuildTarget) target = NULL;
  g_autoptr(GFile) install_directory = NULL;
  g_autofree gchar *install_dir = NULL;
  g_autofree gchar *install_dir_abs = NULL;
  g_autofree gchar *name = NULL;
  JsonArray *artefacts;
  JsonObject *path_object;
  JsonObject *install;
  JsonObject *prefix;
  JsonArray *destination_array;
  JsonObject *destination;
  const gchar *artefacts_path;
  const gchar *prefix_path;
  const gchar *destination_path;

  g_return_if_fail (GBP_IS_CMAKE_BUILD_TARGET_PROVIDER (self));

  /* ignore target if no install rule is present */
  if (!json_object_has_member (obj, "install"))
    return;

  artefacts = json_object_get_array_member (obj, "artifacts");
  /* currently we support only one artefact executable */
  path_object = json_array_get_object_element (artefacts, 0);
  artefacts_path = json_object_get_string_member (path_object, "path");

  install = json_object_get_object_member (obj, "install");
  prefix = json_object_get_object_member (install, "prefix");
  prefix_path = json_object_get_string_member (prefix, "path");
  destination_array = json_object_get_array_member (install, "destinations");
  destination = json_array_get_object_element (destination_array, 0);
  destination_path = json_object_get_string_member (destination, "path");

  install_dir = g_path_get_dirname (artefacts_path);
  if (g_str_has_prefix (destination_path, prefix_path))
    install_dir_abs = g_strdup (destination_path);
  else
    install_dir_abs = g_build_path (G_DIR_SEPARATOR_S, prefix_path, destination_path, NULL);

  install_directory = g_file_new_for_path (install_dir_abs);

  name = g_path_get_basename (artefacts_path);
  target = gbp_cmake_build_target_new (context, install_directory, name);

  g_debug ("Found target %s with install directory %s", name, install_dir_abs);

  g_ptr_array_add (*ret, g_steal_pointer (&target));
}

static void
gbp_cmake_build_target_provider_get_targets_async (IdeBuildTargetProvider *provider,
                                                   GCancellable           *cancellable,
                                                   GAsyncReadyCallback     callback,
                                                   gpointer                user_data)
{
  GbpCmakeBuildTargetProvider *self = GBP_CMAKE_BUILD_TARGET_PROVIDER (provider);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(GFile) reply = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autofree gchar *replydir = NULL;
  IdeContext *context;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  const gchar *builddir;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_TARGET_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_build_target_provider_get_targets_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);
  builddir = ide_pipeline_get_builddir (pipeline);

  replydir = g_build_path (G_DIR_SEPARATOR_S, builddir, ".cmake", "api", "v1", "reply", NULL);

  if (!g_file_test (replydir, G_FILE_TEST_EXISTS))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_EXISTS,
                                 "Response codemodel does not exists, ignoring");
      IDE_EXIT;
    }

  ret = g_ptr_array_new_with_free_func (g_object_unref);
  reply = g_file_new_for_path (replydir);
  enumerator = g_file_enumerate_children (reply, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NONE, cancellable, NULL);

  while (TRUE)
    {
      g_autoptr(GFileInputStream) stream = NULL;
      g_autoptr(JsonParser) parser = NULL;
      GFile *file;
      JsonNode *root;
      JsonObject *jobject;

      if (!g_file_enumerator_iterate (enumerator, NULL, &file, cancellable, NULL))
        goto out;
      if (!file)
        break;

      stream = g_file_read (file, cancellable, NULL);
      parser = json_parser_new ();
      json_parser_load_from_stream (parser, G_INPUT_STREAM (stream), cancellable, NULL);

      root = json_parser_get_root (parser);
      jobject = json_node_get_object (root);
      if (json_object_has_member (jobject, "type") &&
          ide_str_equal0 (json_object_get_string_member (jobject, "type"), "EXECUTABLE"))
        {
          gbp_cmake_build_target_provider_create_target (self, &ret, context, jobject);
        }
    }

out:
  IDE_PROBE;
  ide_task_return_pointer (task, g_steal_pointer (&ret), g_ptr_array_unref);

  IDE_EXIT;
}

static GPtrArray *
gbp_cmake_build_target_provider_get_targets_finish (IdeBuildTargetProvider  *provider,
                                                    GAsyncResult            *result,
                                                    GError                 **error)
{
  GPtrArray *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_BUILD_TARGET_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (IDE_PTR_ARRAY_STEAL_FULL (&ret));
}

static void
build_target_provider_iface_init (IdeBuildTargetProviderInterface *iface)
{
  iface->get_targets_async = gbp_cmake_build_target_provider_get_targets_async;
  iface->get_targets_finish = gbp_cmake_build_target_provider_get_targets_finish;
}
