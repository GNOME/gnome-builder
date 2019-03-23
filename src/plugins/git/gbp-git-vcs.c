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

#include <stdlib.h>

#include "gbp-git-branch.h"
#include "gbp-git-client.h"
#include "gbp-git-tag.h"
#include "gbp-git-vcs.h"
#include "gbp-git-vcs-config.h"

struct _GbpGitVcs
{
  IdeObject       parent_instance;
  GbpGitClient   *client;
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
gbp_git_vcs_parent_set (IdeObject *object,
                        IdeObject *parent)
{
  GbpGitVcs *self = (GbpGitVcs *)object;
  g_autoptr(IdeContext) context = NULL;

  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!object || IDE_IS_OBJECT (parent));

  if (object == NULL)
    return;

  context = ide_object_ref_context (IDE_OBJECT (self));
  self->client = g_object_ref (gbp_git_client_from_context (context));
}

static void
gbp_git_vcs_finalize (GObject *object)
{
  GbpGitVcs *self = (GbpGitVcs *)object;

  g_clear_object (&self->client);
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
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_vcs_finalize;
  object_class->get_property = gbp_git_vcs_get_property;
  object_class->set_property = gbp_git_vcs_set_property;

  i_object_class->parent_set = gbp_git_vcs_parent_set;

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
  if ((name = g_file_get_relative_path (self->workdir, file)))
    {
      if (g_strcmp0 (name, ".git") == 0 ||
          g_str_has_prefix (name, ".git/") ||
          gbp_git_client_is_ignored (self->client, file, error))
        return TRUE;
    }

  return FALSE;
}

static void
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
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) workdir = NULL;
  g_autofree gchar *path = NULL;
  GbpGitClient *client;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_GIT_VCS (self));
  g_return_if_fail (!directory_or_file || G_IS_FILE (directory_or_file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_status_async);
  ide_task_set_priority (task, io_priority);

  context = ide_object_ref_context (IDE_OBJECT (self));
  client = gbp_git_client_from_context (context);
  workdir = ide_context_ref_workdir (context);

  if (!g_file_equal (directory_or_file, workdir) &&
      !g_file_has_prefix (directory_or_file, workdir))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Status is not supported on this file");
      IDE_EXIT;
    }

  if (!(path = g_file_get_relative_path (workdir, directory_or_file)))
    path = g_strdup (".");

  gbp_git_client_list_status_async (client,
                                    path,
                                    include_descendants,
                                    cancellable,
                                    gbp_git_vcs_list_status_cb,
                                    g_steal_pointer (&task));

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
gbp_git_vcs_list_branches_worker (IdeTask      *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  GbpGitVcs *self = source_object;
  g_autoptr(GPtrArray) branches = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  branches = g_ptr_array_new_with_free_func (g_object_unref);

  ide_object_lock (IDE_OBJECT (self));

  if (self->repository != NULL)
    {
      g_autoptr(GgitBranchEnumerator) enumerator = NULL;
      g_autoptr(GError) error = NULL;

      if (!(enumerator = ggit_repository_enumerate_branches (self->repository,
                                                             GGIT_BRANCH_LOCAL,
                                                             &error)))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          goto unlock;
        }

      while (ggit_branch_enumerator_next (enumerator))
        {
          g_autoptr(GgitRef) ref = ggit_branch_enumerator_get (enumerator);
          const gchar *name = ggit_ref_get_name (ref);

          g_ptr_array_add (branches, gbp_git_branch_new (name));
        }

      ide_task_return_pointer (task,
                               g_steal_pointer (&branches),
                               g_ptr_array_unref);
    }
  else
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No repository to access");
    }

unlock:
  ide_object_unlock (IDE_OBJECT (self));
}

static void
gbp_git_vcs_list_branches_async (IdeVcs              *vcs,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_status_async);
  ide_task_set_return_on_cancel (task, TRUE);

  ide_object_lock (IDE_OBJECT (self));
  if (self->repository == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "No repository loaded");
  else
    ide_task_run_in_thread (task, gbp_git_vcs_list_branches_worker);
  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static GPtrArray *
gbp_git_vcs_list_branches_finish (IdeVcs        *vcs,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (vcs), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static gint
compare_tags (gconstpointer a,
              gconstpointer b)
{
  return g_utf8_collate (*(const gchar **)a, *(const gchar **)b);
}

static void
gbp_git_vcs_list_tags_worker (IdeTask      *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
  GbpGitVcs *self = source_object;
  g_autoptr(GPtrArray) tags = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  tags = g_ptr_array_new_with_free_func (g_object_unref);

  ide_object_lock (IDE_OBJECT (self));

  if (self->repository != NULL)
    {
      g_autoptr(GgitBranchEnumerator) enumerator = NULL;
      g_auto(GStrv) names = NULL;
      g_autoptr(GError) error = NULL;

      if (!(names = ggit_repository_list_tags (self->repository, &error)))
        {
          ide_task_return_error (task, g_steal_pointer (&error));
          goto unlock;
        }

      qsort (names, g_strv_length (names), sizeof (gchar *), compare_tags);

      for (guint i = 0; names[i] != NULL; i++)
        g_ptr_array_add (tags, gbp_git_tag_new (names[i]));

      ide_task_return_pointer (task,
                               g_steal_pointer (&tags),
                               g_ptr_array_unref);
    }
  else
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No repository to access");
    }

unlock:
  ide_object_unlock (IDE_OBJECT (self));
}

static void
gbp_git_vcs_list_tags_async (IdeVcs              *vcs,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GbpGitVcs *self = (GbpGitVcs *)vcs;
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_list_status_async);
  ide_task_set_return_on_cancel (task, TRUE);

  ide_object_lock (IDE_OBJECT (self));
  if (self->repository == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "No repository loaded");
  else
    ide_task_run_in_thread (task, gbp_git_vcs_list_tags_worker);
  ide_object_unlock (IDE_OBJECT (self));

  IDE_EXIT;
}

static GPtrArray *
gbp_git_vcs_list_tags_finish (IdeVcs        *vcs,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (GBP_IS_GIT_VCS (vcs), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
gbp_git_vcs_switch_branch_worker (IdeTask      *task,
                                  gpointer      source_object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
  g_autoptr(GgitCheckoutOptions) checkout_options = NULL;
  g_autoptr(GgitObject) obj = NULL;
  g_autoptr(GgitRef) ref = NULL;
  g_autoptr(GError) error = NULL;
  GbpGitVcs *self = source_object;
  const gchar *id = task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_GIT_VCS (self));
  g_assert (id != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ide_object_lock (IDE_OBJECT (self));

  if (self->repository == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "No repository to switch");
      goto unlock;
    }

  if (!(ref = ggit_repository_lookup_reference (self->repository, id, &error)) ||
      !(obj = ggit_ref_lookup (ref, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto unlock;
    }

  checkout_options = ggit_checkout_options_new ();
  ggit_checkout_options_set_strategy (checkout_options, GGIT_CHECKOUT_SAFE);

  /* Update the tree contents */
  if (!ggit_repository_checkout_tree (self->repository,
                                      obj,
                                      checkout_options,
                                      &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto unlock;
    }

  /* Now update head to point at the branch */
  if (!ggit_repository_set_head (self->repository, id, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      goto unlock;
    }

  ide_task_return_boolean (task, TRUE);

unlock:
  ide_object_unlock (IDE_OBJECT (self));
}

static void
gbp_git_vcs_switch_branch_async (IdeVcs              *vcs,
                                 IdeVcsBranch        *branch,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (GBP_IS_GIT_BRANCH (branch));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (vcs, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_git_vcs_switch_branch_async);
  ide_task_set_task_data (task,
                          g_strdup (ide_vcs_branch_get_name (branch)),
                          g_free);
  ide_task_run_in_thread (task, gbp_git_vcs_switch_branch_worker);
}

static gboolean
gbp_git_vcs_switch_branch_finish (IdeVcs        *vcs,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GIT_VCS (vcs));
  g_assert (IDE_IS_TASK (result));

  ide_vcs_emit_changed (vcs);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
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
  iface->list_branches_async = gbp_git_vcs_list_branches_async;
  iface->list_branches_finish = gbp_git_vcs_list_branches_finish;
  iface->list_tags_async = gbp_git_vcs_list_tags_async;
  iface->list_tags_finish = gbp_git_vcs_list_tags_finish;
  iface->switch_branch_async = gbp_git_vcs_switch_branch_async;
  iface->switch_branch_finish = gbp_git_vcs_switch_branch_finish;
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
          if (!ide_str_equal0 (name, self->branch))
            {
              g_free (self->branch);
              self->branch = g_strdup (name);
              g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BRANCH_NAME]);
            }
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
