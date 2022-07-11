/* gbp-waf-run-command-provider.c
 *
 * Copyright 2019 Alex Mitchell
 * Copyright 2016-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-waf-run-command-provider"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-foundry.h>
#include <libide-threading.h>

#include "gbp-waf-build-system.h"
#include "gbp-waf-run-command-provider.h"

struct _GbpWafRunCommandProvider
{
  IdeObject parent_instance;
};

static void
gbp_waf_run_command_provider_list_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autofree char *stdout_buf = NULL;
  IdeLineReader reader;
  char *line;
  gsize line_len;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_line_reader_init (&reader, stdout_buf, -1);

  /* Skip first two lines */
  ide_line_reader_next (&reader, &line_len);
  ide_line_reader_next (&reader, &line_len);

  /* TODO: We pretend that everything is installed, how can we determine
   * if that is really the case? This allows us to choose a target
   * in the project-tree to run. There don't seem to be any options from
   * "waf list" to get information about the targets.
   */

  store = g_list_store_new (IDE_TYPE_RUN_COMMAND);

  while ((line = ide_line_reader_next (&reader, &line_len)))
    {
      g_autoptr(IdeRunCommand) run_command = NULL;
      g_autofree char *id = NULL;

      line[line_len] = 0;

      g_strstrip (line);

      /* Skip last line -> "'list' finished successfully (time)" */
      if (g_str_has_prefix (line, "'list' "))
        break;

      /* Skip things that are outside tree */
      if (g_str_has_prefix (line, ".."))
        continue;

      id = g_strdup_printf ("waf:%s", line);

      run_command = ide_run_command_new ();
      ide_run_command_set_id (run_command, id);
      ide_run_command_set_priority (run_command, 0);
      ide_run_command_set_display_name (run_command, line);
      ide_run_command_set_argv (run_command, IDE_STRV_INIT (line));

      g_list_store_append (store, run_command);
    }

  ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);

  IDE_EXIT;
}

static void
gbp_waf_run_command_provider_list_commands_async (IdeRunCommandProvider *provider,
                                                  GCancellable          *cancellable,
                                                  GAsyncReadyCallback    callback,
                                                  gpointer               user_data)
{
  GbpWafRunCommandProvider *self = (GbpWafRunCommandProvider *)provider;
  g_autoptr(IdeSubprocessLauncher) launcher  = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *waf = NULL;
  IdeBuildManager *build_manager;
  IdeBuildSystem *build_system;
  IdePipeline *pipeline;
  IdeContext *context;
  const char *python;

  IDE_ENTRY;

  g_assert (GBP_IS_WAF_RUN_COMMAND_PROVIDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_waf_run_command_provider_list_commands_async);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_build_system_from_context (context);

  if (!GBP_IS_WAF_BUILD_SYSTEM (build_system))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not a waf build system");
      IDE_EXIT;
    }

  build_manager = ide_build_manager_from_context (context);
  pipeline = ide_build_manager_get_pipeline (build_manager);

  if (pipeline == NULL ||
      !ide_pipeline_is_ready (pipeline) ||
      ide_pipeline_get_phase (pipeline) < IDE_PIPELINE_PHASE_CONFIGURE)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Pipeline not ready, cannot list run commands");
      IDE_EXIT;
    }

  waf = gbp_waf_build_system_locate_waf (GBP_WAF_BUILD_SYSTEM (build_system));
  if (gbp_waf_build_system_wants_python2 (GBP_WAF_BUILD_SYSTEM (build_system), NULL))
    python = "python2";
  else
    python = "python3";

  launcher = ide_pipeline_create_launcher (pipeline, NULL);
  ide_subprocess_launcher_push_args (launcher, IDE_STRV_INIT (python, waf, "list", "--color=no"));
  /* There appears to be some installations that will write to stderr instead of stdout */
  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                      G_SUBPROCESS_FLAGS_STDERR_MERGE));
  ide_subprocess_launcher_set_cwd (launcher, ide_pipeline_get_srcdir (pipeline));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         gbp_waf_run_command_provider_list_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static GListModel *
gbp_waf_run_command_provider_list_commands_finish (IdeRunCommandProvider  *provider,
                                                   GAsyncResult           *result,
                                                   GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_assert (GBP_IS_WAF_RUN_COMMAND_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
run_command_provider_iface_init (IdeRunCommandProviderInterface *iface)
{
  iface->list_commands_async = gbp_waf_run_command_provider_list_commands_async;
  iface->list_commands_finish = gbp_waf_run_command_provider_list_commands_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpWafRunCommandProvider, gbp_waf_run_command_provider, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_RUN_COMMAND_PROVIDER, run_command_provider_iface_init))

static void
gbp_waf_run_command_provider_class_init (GbpWafRunCommandProviderClass *klass)
{
}

static void
gbp_waf_run_command_provider_init (GbpWafRunCommandProvider *self)
{
}
