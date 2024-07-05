/* gbp-cmake-run-command-provider.c
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

#define G_LOG_DOMAIN "gbp-cmake-run-command-provider"

#include "config.h"

#include <libide-foundry.h>
#include <libide-io.h>
#include <libide-threading.h>

#include "gbp-cmake-build-system.h"
#include "gbp-cmake-run-command-provider.h"

struct _GbpCmakeRunCommandProvider
{
  IdeObject parent_instance;
};

static GListModel *
parse_manifest_text (char  *contents,
                     gsize  length)
{
  GListStore *store;
  IdeLineReader reader;
  char *line;
  gsize line_length;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (contents != NULL);

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);

  /* TODO: this is the perfect kind of thing to push off to a thread. If
   *       only we had a more competent tasking library.
   */

  ide_line_reader_init (&reader, contents, length);
  while ((line = ide_line_reader_next (&reader, &line_length)))
    {
      const char *binary_path;
      const char *name;

      /* Allow us to treat this as a \0 terminated line. We can do
       * this because g_file_load_contents() (and it's variants) will
       * ensure we have a trailing \0 byte at the end of the contents
       * which is not part of the reported "length".
       */
      line[line_length] = 0;

      if ((binary_path = strstr (line, "/bin/")))
        name = binary_path + strlen ("/bin/");
      else if ((binary_path = strstr (line, "/libexec/")))
        name = binary_path + strlen ("/libexec/");
      else
        continue;

      /* Just a directory, skip it */
      if (name[0] == 0)
        continue;

      /* If for some reason it's in a further subdirectory, skip it */
      if (strchr (name, G_DIR_SEPARATOR) == NULL)
        {
          g_autoptr(IdeRunCommand) run_command = ide_run_command_new ();
          g_autofree char *id = g_strdup_printf ("cmake:%s", name);

          ide_run_command_set_kind (run_command, IDE_RUN_COMMAND_KIND_APPLICATION);
          ide_run_command_set_id (run_command, id);
          ide_run_command_set_display_name (run_command, name);
          ide_run_command_append_argv (run_command, line);
          ide_run_command_set_can_default (run_command, TRUE);

          g_list_store_append (store, run_command);
        }
    }

  IDE_RETURN (G_LIST_MODEL (store));
}

static void
gbp_cmake_run_command_provider_list_commands_load_cb (GObject      *object,
                                                      GAsyncResult *result,
                                                      gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize length = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_file_load_contents_finish (file, result, &contents, &length, NULL, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, parse_manifest_text (contents, length), g_object_unref);

  IDE_EXIT;
}

static void
gbp_cmake_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                    GCancellable          *cancellable,
                                                    GAsyncReadyCallback    callback,
                                                    gpointer               user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) manifest_file = NULL;
  g_autofree char *manifest_path = NULL;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CMAKE_RUN_COMMAND_PROVIDER (provider));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (provider, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_cmake_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (provider));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_CMAKE_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a CMake-based build-system, ignoring request");
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (!ide_pipeline_is_ready (pipeline))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Pipeline not yet ready, cannot list run commands");
      IDE_EXIT;
    }

  manifest_path = ide_pipeline_build_builddir_path (pipeline, "install_manifest.txt", NULL);
  manifest_file = g_file_new_for_path (manifest_path);

  g_file_load_contents_async (manifest_file,
                              cancellable,
                              gbp_cmake_run_command_provider_list_commands_load_cb,
                              g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
gbp_cmake_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                     GAsyncResult           *result,
                                                     GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CMAKE_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_cmake_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_cmake_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpCmakeRunCommandProvider, gbp_cmake_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_cmake_run_command_provider_parent_set (IdeObject *object,
                                           IdeObject *parent)
{
  IDE_ENTRY;

  g_assert (GBP_IS_CMAKE_RUN_COMMAND_PROVIDER (object));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (parent != NULL)
    ide_run_command_provider_invalidates_at_phase (IDE_RUN_COMMAND_PROVIDER (object),
                                                   IDE_PIPELINE_PHASE_CONFIGURE);

  IDE_EXIT;
}

static void
gbp_cmake_run_command_provider_class_init (GbpCmakeRunCommandProviderClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  i_object_class->parent_set = gbp_cmake_run_command_provider_parent_set;
}

static void
gbp_cmake_run_command_provider_init (GbpCmakeRunCommandProvider *self)
{
}
