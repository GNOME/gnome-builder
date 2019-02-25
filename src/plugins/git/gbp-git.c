/* gbp-git.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#define G_LOG_DOMAIN "gbp-git"

#include "config.h"

#include <libgit2-glib/ggit.h>

#include "gbp-git.h"

struct _GbpGit
{
  GObject         parent_instance;
  GFile          *workdir;
  GgitRepository *repository;
};

G_DEFINE_TYPE (GbpGit, gbp_git, G_TYPE_OBJECT)

static void
gbp_git_finalize (GObject *object)
{
  GbpGit *self = (GbpGit *)object;

  g_clear_object (&self->workdir);
  g_clear_object (&self->repository);

  G_OBJECT_CLASS (gbp_git_parent_class)->finalize (object);
}

static void
gbp_git_class_init (GbpGitClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_git_finalize;
}

static void
gbp_git_init (GbpGit *self)
{
}

GbpGit *
gbp_git_new (void)
{
  return g_object_new (GBP_TYPE_GIT, NULL);
}

void
gbp_git_set_workdir (GbpGit *self,
                     GFile  *workdir)
{
  g_return_if_fail (GBP_IS_GIT (self));
  g_return_if_fail (G_IS_FILE (workdir));

  if (g_set_object (&self->workdir, workdir))
    {
      g_clear_object (&self->repository);
    }
}

static void
gbp_git_is_ignored (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_boolean (task, FALSE);
}

void
gbp_git_is_ignored_async (GbpGit              *self,
                          const gchar         *path,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_is_ignored_async);
  g_task_set_priority (task, G_PRIORITY_HIGH);
  g_task_run_in_thread (task, gbp_git_is_ignored);
}

gboolean
gbp_git_is_ignored_finish (GbpGit        *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gbp_git_switch_branch (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_boolean (task, FALSE);
}

void
gbp_git_switch_branch_async (GbpGit              *self,
                             const gchar         *branch_name,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_switch_branch_async);
  g_task_set_priority (task, G_PRIORITY_LOW + 1000);
  g_task_run_in_thread (task, gbp_git_switch_branch);
}

gboolean
gbp_git_switch_branch_finish (GbpGit        *self,
                              GAsyncResult  *result,
                              gchar        **switch_to_directory,
                              GError       **error)
{
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *other_path = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  other_path = g_task_propagate_pointer (G_TASK (result), &local_error);

  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  if (switch_to_directory != NULL)
    *switch_to_directory = g_steal_pointer (&other_path);

  return TRUE;
}

static void
gbp_git_list_refs_by_kind (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_pointer (task,
                         g_ptr_array_new (),
                         (GDestroyNotify)g_ptr_array_unref);
}

void
gbp_git_list_refs_by_kind_async (GbpGit              *self,
                                 GbpGitRefKind        kind,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (kind > 0);
  g_assert (kind <= 3);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_list_refs_by_kind_async);
  g_task_run_in_thread (task, gbp_git_list_refs_by_kind);
}

GPtrArray *
gbp_git_list_refs_by_kind_finish (GbpGit        *self,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gbp_git_list_status (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_return_pointer (task,
                         g_ptr_array_new (),
                         (GDestroyNotify)g_ptr_array_unref);
}

void
gbp_git_list_status_async (GbpGit              *self,
                           const gchar         *directory_or_file,
                           gboolean             include_descendants,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (GBP_IS_GIT (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_git_list_status_async);
  g_task_run_in_thread (task, gbp_git_list_status);
}

GPtrArray *
gbp_git_list_status_finish (GbpGit        *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_assert (GBP_IS_GIT (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}
