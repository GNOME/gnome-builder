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

#include <libgit2-glib/ggit.h>
#include <libide-editor.h>
#include <libide-io.h>
#include <libide-threading.h>

#include "gbp-git-buffer-change-monitor.h"
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
gbp_git_workbench_addin_load_project_worker (IdeTask      *task,
                                             gpointer      source_object,
                                             gpointer      task_data,
                                             GCancellable *cancellable)
{
  GbpGitWorkbenchAddin *self = source_object;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GbpGitVcs) vcs = NULL;
  g_autoptr(GFile) location = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *worktree_branch = NULL;
  GFile *directory = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));
  g_assert (G_IS_FILE (directory));

  /* Short-circuit if we don't .git */
  if (!(location = ggit_repository_discover_full (directory, TRUE, NULL, &error)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Failed to locate git repository location");
      return;
    }

  g_debug ("Located .git at %s", g_file_peek_path (location));

  /* If @location is a regular file, we might have a git-worktree link */
  if (g_file_query_file_type (location, 0, NULL) == G_FILE_TYPE_REGULAR)
    {
      g_autofree gchar *contents = NULL;
      gsize len;

      if (g_file_load_contents (location, NULL, &contents, &len, NULL, NULL))
        {
          IdeLineReader reader;
          gchar *line;
          gsize line_len;

          ide_line_reader_init (&reader, contents, len);

          while ((line = ide_line_reader_next (&reader, &line_len)))
            {
              line[line_len] = 0;

              if (g_str_has_prefix (line, "gitdir: "))
                {
                  g_autoptr(GFile) location_parent = g_file_get_parent (location);
                  const gchar *path = line + strlen ("gitdir: ");
                  const gchar *branch;

                  g_clear_object (&location);

                  if (g_path_is_absolute (path))
                    location = g_file_new_for_path (path);
                  else
                    location = g_file_resolve_relative_path (location_parent, path);

                  /*
                   * Worktrees only have a single branch, and it is the name
                   * of the suffix of .git/worktrees/<name>
                   */
                  if ((branch = strrchr (line, G_DIR_SEPARATOR)))
                    worktree_branch = g_strdup (branch + 1);

                  break;
                }
            }
        }
    }

  if (!(repository = ggit_repository_open (location, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  workdir = ggit_repository_get_workdir (repository);

  g_assert (G_IS_FILE (location));
  g_assert (G_IS_FILE (workdir));
  g_assert (GGIT_IS_REPOSITORY (repository));

  if (worktree_branch == NULL)
    {
      g_autoptr(GgitRef) ref = NULL;

      if ((ref = ggit_repository_get_head (repository, NULL)))
        worktree_branch = g_strdup (ggit_ref_get_shorthand (ref));

      if (worktree_branch == NULL)
        worktree_branch = g_strdup ("master");
    }

  vcs = g_object_new (GBP_TYPE_GIT_VCS,
                      "branch-name", worktree_branch,
                      "location", location,
                      "repository", repository,
                      "workdir", workdir,
                      NULL);

  ide_task_return_pointer (task, g_steal_pointer (&vcs), g_object_unref);
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

  /* Try to discover the git repository from a worker thread. If we find
   * it, we'll set the VCS on the workbench for various components to use.
   */
  ide_task_set_task_data (task, g_object_ref (directory), g_object_unref);
  ide_task_run_in_thread (task, gbp_git_workbench_addin_load_project_worker);
}

static void
gbp_git_workbench_addin_foreach_buffer_cb (IdeBuffer *buffer,
                                           gpointer   user_data)
{
  GgitRepository *repository = user_data;
  IdeBufferChangeMonitor *monitor;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GGIT_IS_REPOSITORY (repository));

  monitor = ide_buffer_get_change_monitor (buffer);

  if (GBP_IS_GIT_BUFFER_CHANGE_MONITOR (monitor))
    gbp_git_buffer_change_monitor_set_repository (GBP_GIT_BUFFER_CHANGE_MONITOR (monitor),
                                                  repository);
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
  GgitRepository *repository;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GIT_WORKBENCH_ADDIN (self));

  if (!gbp_git_vcs_reload_finish (vcs, result, &error))
    return;

  if (self->workbench == NULL)
    return;

  repository = gbp_git_vcs_get_repository (vcs);
  context = ide_workbench_get_context (self->workbench);
  buffer_manager = ide_buffer_manager_from_context (context);

  ide_buffer_manager_foreach (buffer_manager,
                              gbp_git_workbench_addin_foreach_buffer_cb,
                              repository);
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

          self->has_loaded = TRUE;

          task = ide_task_new (self, NULL, load_git_for_editor_cb, NULL);
          ide_task_set_source_tag (task, gbp_git_workbench_addin_workspace_added);
          ide_task_set_task_data (task, g_object_ref (workdir), g_object_unref);
          ide_task_run_in_thread (task, gbp_git_workbench_addin_load_project_worker);
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
