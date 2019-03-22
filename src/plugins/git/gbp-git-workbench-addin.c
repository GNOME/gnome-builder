/* gbp-git-workbench-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-git-workbench-addin"

#include "config.h"

#include <libide-editor.h>
#include <libide-io.h>
#include <libide-threading.h>

#include "gbp-git-buffer-change-monitor.h"
#include "gbp-git-client.h"
#include "gbp-git-index-monitor.h"
#include "gbp-git-vcs.h"
#include "gbp-git-workbench-addin.h"

struct _GbpGitWorkbenchAddin
{
  GObject             parent_instance;
  IdeWorkbench       *workbench;
  GbpGitIndexMonitor *monitor;
  guint               has_loaded : 1;
};

static void
gbp_git_workbench_addin_discover_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpGitClient *client = (GbpGitClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeVcs) vcs = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GFile) dot_git = NULL;
  g_autofree gchar *branch = NULL;
  gboolean is_worktree = FALSE;

  IDE_ENTRY;

  g_assert (GBP_IS_GIT_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_git_client_discover_finish (client, result, &workdir, &dot_git, &branch, &is_worktree, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  vcs = g_object_new (GBP_TYPE_GIT_VCS,
                      "branch-name", branch,
                      "workdir", workdir,
                      "location", dot_git,
                      NULL);

  ide_task_return_pointer (task, g_steal_pointer (&vcs), g_object_unref);

  IDE_EXIT;
}

static void
gbp_git_workbench_addin_load_project_async (IdeWorkbenchAddin   *addin,
                                            IdeProjectInfo      *project_info,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  GbpGitClient *client;
  IdeContext *context;
  GFile *directory;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_PROJECT_INFO (project_info));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->has_loaded = TRUE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_workbench_addin_load_project_async);

  if (!(directory = ide_project_info_get_directory (project_info)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Missing directory from project info");
      return;
    }

  context = ide_workbench_get_context (self->workbench);
  client = gbp_git_client_from_context (context);

  gbp_git_client_discover_async (client,
                                 directory,
                                 cancellable,
                                 gbp_git_workbench_addin_discover_cb,
                                 g_steal_pointer (&task));
}

static void
gbp_git_workbench_addin_foreach_buffer_cb (IdeBuffer *buffer,
                                           gpointer   user_data)
{
  IdeBufferChangeMonitor *monitor;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));

  monitor = ide_buffer_get_change_monitor (buffer);

  if (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (monitor))
    ide_buffer_change_monitor_reload (monitor);
}

static void
gbp_git_workbench_addin_reload_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpGitVcs *vcs = (GbpGitVcs *)object;
  g_autoptr(GbpGitWorkbenchAddin) self = user_data;
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));

  if (!gbp_git_vcs_reload_finish (vcs, result, &error))
    return;

  if (self->workbench == NULL)
    return;

  context = ide_workbench_get_context (self->workbench);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_foreach (buffer_manager,
                              gbp_git_workbench_addin_foreach_buffer_cb,
                              NULL);
}

static void
gbp_git_workbench_addin_monitor_changed_cb (GbpGitWorkbenchAddin *self,
                                            GbpGitIndexMonitor   *monitor)
{
  IdeContext *context;
  IdeVcs *vcs;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (GBP_IS_GIT_INDEX_MONITOR (monitor));

  context = ide_workbench_get_context (self->workbench);
  vcs = ide_vcs_from_context (context);

  if (!GBP_IS_GIT_VCS (vcs))
    IDE_EXIT;

  gbp_git_vcs_reload_async (GBP_GIT_VCS (vcs),
                            NULL,
                            gbp_git_workbench_addin_reload_cb,
                            g_object_ref (self));

  IDE_EXIT;
}

static gboolean
gbp_git_workbench_addin_load_project_finish (IdeWorkbenchAddin  *addin,
                                             GAsyncResult       *result,
                                             GError            **error)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;
  g_autoptr(GbpGitVcs) vcs = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_TASK (result));

  if ((vcs = ide_task_propagate_pointer (IDE_TASK (result), error)))
    {
      if (IDE_IS_WORKBENCH (self->workbench))
        {
          /* Set the vcs for the workbench */
          ide_workbench_set_vcs (self->workbench, IDE_VCS (vcs));

          if (self->monitor == NULL)
            {
              GFile *location = gbp_git_vcs_get_location (vcs);

              self->monitor = gbp_git_index_monitor_new (location);

              g_signal_connect_object (self->monitor,
                                       "changed",
                                       G_CALLBACK (gbp_git_workbench_addin_monitor_changed_cb),
                                       self,
                                       G_CONNECT_SWAPPED);
            }
        }
    }

  return vcs != NULL;
}

static void
gbp_git_workbench_addin_load (IdeWorkbenchAddin *addin,
                              IdeWorkbench      *workbench)
{
  GBP_GIT_WORKBENCH_ADDIN (addin)->workbench = workbench;
}

static void
gbp_git_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                IdeWorkbench      *workbench)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  g_clear_object (&self->monitor);

  self->workbench = NULL;
}

static void
load_git_for_editor_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  gbp_git_workbench_addin_load_project_finish (IDE_WORKBENCH_ADDIN (object), result, NULL);
}

static void
gbp_git_workbench_addin_workspace_added (IdeWorkbenchAddin *addin,
                                         IdeWorkspace      *workspace)
{
  GbpGitWorkbenchAddin *self = (GbpGitWorkbenchAddin *)addin;

  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!self->has_loaded)
    {
      /* If we see a new IdeEditorWorkspace without having loaded a project,
       * that means that we are in a non-project scenario (dedicated editor
       * window). We can try our best to load a git repository based on
       * the files that are loaded.
       */
      if (IDE_IS_EDITOR_WORKSPACE (workspace))
        {
          IdeContext *context = ide_workbench_get_context (self->workbench);
          g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
          g_autoptr(IdeTask) task = NULL;
          GbpGitClient *client;

          self->has_loaded = TRUE;

          client = gbp_git_client_from_context (context);

          task = ide_task_new (self, NULL, load_git_for_editor_cb, NULL);
          ide_task_set_source_tag (task, gbp_git_workbench_addin_workspace_added);

          /* Reuse our discovery process, which is normally used when loading
           * a project (with known directory, etc).
           */
          gbp_git_client_discover_async (client,
                                         workdir,
                                         NULL,
                                         gbp_git_workbench_addin_discover_cb,
                                         g_steal_pointer (&task));
        }
    }
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gbp_git_workbench_addin_load;
  iface->unload = gbp_git_workbench_addin_unload;
  iface->load_project_async = gbp_git_workbench_addin_load_project_async;
  iface->load_project_finish = gbp_git_workbench_addin_load_project_finish;
  iface->workspace_added = gbp_git_workbench_addin_workspace_added;
}

G_DEFINE_TYPE_WITH_CODE (GbpGitWorkbenchAddin, gbp_git_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN,
                                                workbench_addin_iface_init))

static void
gbp_git_workbench_addin_class_init (GbpGitWorkbenchAddinClass *klass)
{
}

static void
gbp_git_workbench_addin_init (GbpGitWorkbenchAddin *self)
{
}
