/* gbp-git-vcs.c
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

#define G_LOG_DOMAIN "gbp-git-vcs"

#include "config.h"

#include "gbp-git-vcs.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitVcs
{
  IdeObject       parent_instance;
  GgitRepository *repository;
  GFile          *location;
  GFile          *workdir;
  gchar          *branch;
};

enum {
  PROP_0,
  PROP_BRANCH_NAME,
  PROP_LOCATION,
  PROP_REPOSITORY,
  PROP_WORKDIR,
  N_PROPS
};

static void vcs_iface_init (IdeVcsInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GbpGitVcs, gbp_git_vcs, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_VCS, vcs_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gbp_git_vcs_finalize (GObject *object)
{
  GbpGitVcs *self = (GbpGitVcs *)object;

  g_clear_object (&self->repository);
  g_clear_object (&self->location);
  g_clear_object (&self->workdir);
  g_clear_pointer (&self->branch, g_free);

  G_OBJECT_CLASS (gbp_git_vcs_parent_class)->finalize (object);
}

static void
gbp_git_vcs_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GbpGitVcs *self = GBP_GIT_VCS (object);

  switch (prop_id)
    {
    case PROP_BRANCH_NAME:
      g_value_set_string (value, self->branch);
      break;

    case PROP_LOCATION:
      g_value_set_object (value, self->location);
      break;

    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    case PROP_WORKDIR:
      g_value_set_object (value, self->workdir);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_vcs_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GbpGitVcs *self = GBP_GIT_VCS (object);

  switch (prop_id)
    {
    case PROP_BRANCH_NAME:
      self->branch = g_value_dup_string (value);
      break;

    case PROP_LOCATION:
      self->location = g_value_dup_object (value);
      break;

    case PROP_REPOSITORY:
      self->repository = g_value_dup_object (value);
      break;

    case PROP_WORKDIR:
      self->workdir = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_git_vcs_class_init (GbpGitVcsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_vcs_finalize;
  object_class->get_property = gbp_git_vcs_get_property;
  object_class->set_property = gbp_git_vcs_set_property;

  properties [PROP_BRANCH_NAME] =
    g_param_spec_string ("branch-name",
                         "Branch Name",
                         "The name of the branch",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_LOCATION] =
    g_param_spec_object ("location",
                         "Location",
                         "The location for the repository",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         "Repository",
                         "The underlying repository object",
                         GGIT_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_WORKDIR] =
    g_param_spec_object ("workdir",
                         "Workdir",
                         "Working directory of the repository",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_git_vcs_init (GbpGitVcs *self)
{
}

GFile *
gbp_git_vcs_get_location (GbpGitVcs *self)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (self), NULL);
  return self->location;
}

GgitRepository *
gbp_git_vcs_get_repository (GbpGitVcs *self)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (self), NULL);
  return self->repository;
}

static GFile *
gbp_git_vcs_get_workdir (IdeVcs *vcs)
{
  return GBP_GIT_VCS (vcs)->workdir;
}

static gchar *
gbp_git_vcs_get_branch_name (IdeVcs *vcs)
{
  gchar *ret;

  g_return_val_if_fail (GBP_IS_GIT_VCS (vcs), NULL);

  ide_object_lock (IDE_OBJECT (vcs));
  ret = g_strdup (GBP_GIT_VCS (vcs)->branch);
  ide_object_unlock (IDE_OBJECT (vcs));

  return g_steal_pointer (&ret);
}

static IdeVcsConfig *
gbp_git_vcs_get_config (IdeVcs *vcs)
{
  return g_object_new (GBP_TYPE_GIT_VCS_CONFIG, NULL);
}
static gboolean
gbp_git_vcs_is_ignored (IdeVcs  *vcs,
                        GFile   *file,
                        GError **error)
{
  g_autofree gchar *name = NULL;
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  gboolean ret = FALSE;

  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (G_IS_FILE (file));

  /* Note: this function is required to be thread-safe so that workers
   *       can check if files are ignored from a thread without
   *       round-tripping to the main thread.
   */

  /* self->workdir is not changed after creation, so safe
   * to access it from a thread.
   */
  name = g_file_get_relative_path (self->workdir, file);
  if (g_strcmp0 (name, ".git") == 0)
    return TRUE;

  /*
   * If we have a valid name to work with, we want to query the
   * repository. But this could be called from a thread, so ensure
   * we are the only thread accessing self->repository right now.
   */
  if (name != NULL)
    {
      ide_object_lock (IDE_OBJECT (self));
      ret = ggit_repository_path_is_ignored (self->repository, name, error);
      ide_object_unlock (IDE_OBJECT (self));
    }

  return ret;
}

typedef struct
{
  GFile      *repository_location;
  GFile      *directory_or_file;
  GFile      *workdir;
  GListStore *store;
  guint       recursive : 1;
} ListStatus;

static void
list_status_free (gpointer data)
{
  ListStatus *ls = data;

  g_clear_object (&ls->repository_location);
  g_clear_object (&ls->directory_or_file);
  g_clear_object (&ls->workdir);
  g_clear_object (&ls->store);
  g_slice_free (ListStatus, ls);
}

static gint
gbp_git_vcs_list_status_cb (const gchar     *path,
                            GgitStatusFlags  flags,
                            gpointer         user_data)
{
  ListStatus *state = user_data;
  g_autoptr(GFile) file = NULL;
  g_autoptr(IdeVcsFileInfo) info = NULL;
  IdeVcsFileStatus status = 0;

  g_assert (path != NULL);
  g_assert (state != NULL);
  g_assert (G_IS_LIST_STORE (state->store));
  g_assert (G_IS_FILE (state->workdir));

  file = g_file_get_child (state->workdir, path);

  switch (flags)
    {
    case GGIT_STATUS_INDEX_DELETED:
    case GGIT_STATUS_WORKING_TREE_DELETED:
      status = IDE_VCS_FILE_STATUS_DELETED;
      break;

    case GGIT_STATUS_INDEX_RENAMED:
      status = IDE_VCS_FILE_STATUS_RENAMED;
      break;

    case GGIT_STATUS_INDEX_NEW:
    case GGIT_STATUS_WORKING_TREE_NEW:
      status = IDE_VCS_FILE_STATUS_ADDED;
      break;

    case GGIT_STATUS_INDEX_MODIFIED:
    case GGIT_STATUS_INDEX_TYPECHANGE:
    case GGIT_STATUS_WORKING_TREE_MODIFIED:
    case GGIT_STATUS_WORKING_TREE_TYPECHANGE:
      status = IDE_VCS_FILE_STATUS_CHANGED;
      break;

    case GGIT_STATUS_IGNORED:
      status = IDE_VCS_FILE_STATUS_IGNORED;
      break;

    case GGIT_STATUS_CURRENT:
      status = IDE_VCS_FILE_STATUS_UNCHANGED;
      break;

    default:
      status = IDE_VCS_FILE_STATUS_UNTRACKED;
      break;
    }

  info = g_object_new (IDE_TYPE_VCS_FILE_INFO,
                       "file", file,
                       "status", status,
                       NULL);

  g_list_store_append (state->store, info);

  return 0;
}

static void
gbp_git_vcs_list_status_worker (IdeTask      *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  ListStatus *state = task_data;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitStatusOptions) options = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *relative = NULL;
  gchar *strv[] = { NULL, NULL };

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS (source_object));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->repository_location));

  if (!(repository = ggit_repository_open (state->repository_location, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!(workdir = ggit_repository_get_workdir (repository)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to locate working directory");
      return;
    }

  g_set_object (&state->workdir, workdir);

  if (state->directory_or_file != NULL)
    relative = g_file_get_relative_path (workdir, state->directory_or_file);

  strv[0] = relative;
  options = ggit_status_options_new (GGIT_STATUS_OPTION_DEFAULT,
                                     GGIT_STATUS_SHOW_INDEX_AND_WORKDIR,
                                     (const gchar **)strv);

  store = g_list_store_new (IDE_TYPE_VCS_FILE_INFO);
  g_set_object (&state->store, store);

  if (!ggit_repository_file_status_foreach (repository,
                                            options,
                                            gbp_git_vcs_list_status_cb,
                                            state,
                                            &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&store), g_object_unref);
}

static void
gbp_git_vcs_list_status_async (IdeVcs              *vcs,
                               GFile               *directory_or_file,
                               gboolean             include_descendants,
                               gint                 io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;
  ListStatus *state;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_GIT_VCS (self));
  g_return_if_fail (!directory_or_file || G_IS_FILE (directory_or_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_object_lock (IDE_OBJECT (self));
  state = g_slice_new0 (ListStatus);
  state->directory_or_file = g_object_ref (directory_or_file);
  state->repository_location = ggit_repository_get_location (self->repository);
  state->recursive = !!include_descendants;
  ide_object_unlock (IDE_OBJECT (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_status_async);
  ide_task_set_priority (task, io_priority);
  ide_task_set_return_on_cancel (task, TRUE);
  ide_task_set_task_data (task, state, list_status_free);

  if (state->repository_location == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "No repository loaded");
  else
    ide_task_run_in_thread (task, gbp_git_vcs_list_status_worker);

  IDE_EXIT;
}

static GListModel *
gbp_git_vcs_list_status_finish (IdeVcs        *vcs,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (vcs), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
vcs_iface_init (IdeVcsInterface *iface)
{
  iface->get_workdir = gbp_git_vcs_get_workdir;
  iface->get_branch_name = gbp_git_vcs_get_branch_name;
  iface->get_config = gbp_git_vcs_get_config;
  iface->is_ignored = gbp_git_vcs_is_ignored;
  iface->list_status_async = gbp_git_vcs_list_status_async;
  iface->list_status_finish = gbp_git_vcs_list_status_finish;
}

static void
gbp_git_vcs_reload_worker (IdeTask      *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GError) error = NULL;
  GFile *location = task_data;

  IDE_ENTRY;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS (source_object));
  g_assert (G_IS_FILE (location));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!(repository = ggit_repository_open (location, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&repository), g_object_unref);

  IDE_EXIT;
}

void
gbp_git_vcs_reload_async (GbpGitVcs           *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_source_tag (task, gbp_git_vcs_reload_async);
  ide_task_set_task_data (task, g_object_ref (self->location), g_object_unref);
  ide_task_run_in_thread (task, gbp_git_vcs_reload_worker);

  IDE_EXIT;
}

gboolean
gbp_git_vcs_reload_finish (GbpGitVcs     *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_autoptr(GgitRepository) repository = NULL;
  g_autoptr(GgitRef) ref = NULL;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_GIT_VCS (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ide_object_lock (IDE_OBJECT (self));

  if (!(repository = ide_task_propagate_pointer (IDE_TASK (result), error)))
    goto failure;

  if ((ref = ggit_repository_get_head (repository, NULL)))
    {
      const gchar *name = ggit_ref_get_shorthand (ref);

      if (name != NULL)
        {
          g_free (self->branch);
          self->branch = g_strdup (name);
          g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_NAME]);
        }
    }

  if (g_set_object (&self->repository, repository))
    {
      ide_vcs_emit_changed (IDE_VCS (self));
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_REPOSITORY]);
    }

failure:
  ide_object_unlock (IDE_OBJECT (self));

  return repository != NULL;
}
