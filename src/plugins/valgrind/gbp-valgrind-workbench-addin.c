/* gbp-valgrind-workbench-addin.c
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

#define G_LOG_DOMAIN "gbp-valgrind-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <errno.h>
#include <unistd.h>

#include <libide-foundry.h>
#include <libide-gui.h>

#include "gbp-valgrind-workbench-addin.h"

struct _GbpValgrindWorkbenchAddin
{
  GObject          parent_instance;

  IdeWorkbench    *workbench;
  IdeRunManager   *run_manager;
  IdeBuildManager *build_manager;
  char            *log_name;

  guint            has_handler : 1;
};

static void
gbp_valgrind_workbench_addin_runner_exited_cb (GbpValgrindWorkbenchAddin *self,
                                               IdeRunner                 *runner)
{
  g_autoptr(GFile) file = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));

  if (self->workbench == NULL || self->log_name == NULL)
    IDE_EXIT;

  file = g_file_new_for_path (self->log_name);
  ide_workbench_open_async (self->workbench,
                            file,
                            "editorui",
                            IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_run_handler (IdeRunManager *run_manager,
                                          IdeRunner     *runner,
                                          gpointer       user_data)
{
  GbpValgrindWorkbenchAddin *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *name = NULL;
  char log_fd_param[32];
  int map_fd;
  int fd;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));

  /* Create a temp file to write to and an FD to access it */
  errno = 0;
  fd = g_file_open_tmp ("gnome-builder-valgrind-XXXXXX.txt", &name, &error);
  if (fd < 0)
    {
      g_warning ("Failed to create FD to communicate with Valgrind: %s: %s",
                 g_strerror (errno), error->message);
      IDE_EXIT;
    }

  /* Save the filename so we can open it after exiting */
  g_clear_pointer (&self->log_name, g_free);
  self->log_name = g_steal_pointer (&name);

  /* Get the FD number as it will exist within the subprocess */
  map_fd = ide_runner_take_fd (runner, fd, -1);
  g_snprintf (log_fd_param, sizeof log_fd_param, "--log-fd=%d", map_fd);

  /* Setup arguments to valgrind so it writes output to temp file. Add in
   * reverse order so we can just use prepend() repeatedly */
  ide_runner_prepend_argv (runner, "--track-origins=yes");
  ide_runner_prepend_argv (runner, log_fd_param);
  ide_runner_prepend_argv (runner, "valgrind");

  g_signal_connect_object (runner,
                           "exited",
                           G_CALLBACK (gbp_valgrind_workbench_addin_runner_exited_cb),
                           self,
                           G_CONNECT_SWAPPED);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_notify_pipeline_cb (GbpValgrindWorkbenchAddin *self,
                                                 GParamSpec                *pspec,
                                                 IdeBuildManager           *build_manager)
{
  IdePipeline *pipeline;
  IdeRuntime *runtime;
  gboolean can_handle = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BUILD_MANAGER (build_manager));
  g_assert (IDE_IS_RUN_MANAGER (self->run_manager));

  if (!(pipeline = ide_build_manager_get_pipeline (build_manager)) ||
      !(runtime = ide_pipeline_get_runtime (pipeline)) ||
      !ide_runtime_contains_program_in_path (runtime, "valgrind", NULL))
    IDE_GOTO (not_found);

  can_handle = TRUE;

not_found:
  if (can_handle != self->has_handler)
    {
      self->has_handler = can_handle;

      if (can_handle)
        ide_run_manager_add_handler (self->run_manager,
                                     "valgrind",
                                     _("Run with Valgrind"),
                                     "system-run-symbolic",
                                     "<Primary>F10",
                                     gbp_valgrind_workbench_addin_run_handler,
                                     g_object_ref (self),
                                     g_object_unref);
      else
        ide_run_manager_remove_handler (self->run_manager, "valgrind");
    }

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_project_loaded (IdeWorkbenchAddin *addin,
                                             IdeProjectInfo    *project_info)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;
  IdeBuildManager *build_manager;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (IDE_IS_WORKBENCH (self->workbench));

  context = ide_workbench_get_context (self->workbench);
  build_manager = ide_build_manager_from_context (context);
  run_manager = ide_run_manager_from_context (context);

  g_set_object (&self->build_manager, build_manager);
  g_set_object (&self->run_manager, run_manager);

  g_signal_connect_object (build_manager,
                           "notify::pipeline",
                           G_CALLBACK (gbp_valgrind_workbench_addin_notify_pipeline_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_valgrind_workbench_addin_notify_pipeline_cb (self, NULL, build_manager);

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  IDE_EXIT;
}

static void
gbp_valgrind_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  GbpValgrindWorkbenchAddin *self = (GbpValgrindWorkbenchAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_VALGRIND_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  if (self->build_manager != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->build_manager,
                                            G_CALLBACK (gbp_valgrind_workbench_addin_notify_pipeline_cb),
                                            self);
      g_clear_object (&self->build_manager);
    }

  if (self->run_manager != NULL)
    {
      if (self->has_handler)
        ide_run_manager_remove_handler (self->run_manager, "valgrind");
      g_clear_object (&self->run_manager);
    }

  g_clear_pointer (&self->log_name, g_free);

  self->workbench = NULL;

  IDE_EXIT;
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_valgrind_workbench_addin_load;
  iface->unload = gbp_valgrind_workbench_addin_unload;
  iface->project_loaded = gbp_valgrind_workbench_addin_project_loaded;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpValgrindWorkbenchAddin, gbp_valgrind_workbench_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
gbp_valgrind_workbench_addin_class_init (GbpValgrindWorkbenchAddinClass *klass)
{
}

static void
gbp_valgrind_workbench_addin_init (GbpValgrindWorkbenchAddin *self)
{
}
