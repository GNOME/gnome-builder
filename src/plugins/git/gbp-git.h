/* gbp-git.h
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

#pragma once

#include <libgit2-glib/ggit.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT (gbp_git_get_type())

typedef enum
{
  GBP_GIT_REF_BRANCH = 1 << 0,
  GBP_GIT_REF_TAG    = 1 << 1,
  GBP_GIT_REF_ANY    = GBP_GIT_REF_BRANCH | GBP_GIT_REF_TAG,
} GbpGitRefKind;

typedef struct
{
  gchar         *name;
  GbpGitRefKind  kind : 8;
  guint          is_remote : 1;
} GbpGitRef;

typedef struct
{
  gchar *name;
  guint  is_ignored : 1;
} GbpGitFile;

typedef void (*GbpGitProgress) (const gchar *message,
                                gdouble      progress,
                                gpointer     user_data);

G_DECLARE_FINAL_TYPE (GbpGit, gbp_git, GBP, GIT, GObject)

GbpGit    *gbp_git_new                      (void);
void       gbp_git_set_workdir              (GbpGit                      *self,
                                             GFile                       *workdir);
void       gbp_git_is_ignored_async         (GbpGit                      *self,
                                             const gchar                 *path,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_is_ignored_finish        (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_list_status_async        (GbpGit                      *self,
                                             const gchar                 *directory_or_file,
                                             gboolean                     include_descendants,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
GPtrArray *gbp_git_list_status_finish       (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_list_refs_by_kind_async  (GbpGit                      *self,
                                             GbpGitRefKind                kind,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
GPtrArray *gbp_git_list_refs_by_kind_finish (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_switch_branch_async      (GbpGit                      *self,
                                             const gchar                 *branch_name,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_switch_branch_finish     (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             gchar                      **switch_to_directory,
                                             GError                     **error);
void       gbp_git_clone_url_async          (GbpGit                      *self,
                                             const gchar                 *url,
                                             GFile                       *destination,
                                             GgitCloneOptions            *options,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_clone_url_finish         (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_create_repo_async        (GbpGit                      *self,
                                             GFile                       *in_directory,
                                             gboolean                     bare,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_create_repo_finish       (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_update_submodules_async  (GbpGit                      *self,
                                             GgitSubmoduleUpdateOptions  *options,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_update_submodules_finish (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_read_config_async        (GbpGit                      *self,
                                             const gchar                 *key,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
GVariant  *gbp_git_read_config_finish       (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);
void       gbp_git_update_config_async      (GbpGit                      *self,
                                             gboolean                     global,
                                             const gchar                 *key,
                                             GVariant                    *value,
                                             GCancellable                *cancellable,
                                             GAsyncReadyCallback          callback,
                                             gpointer                     user_data);
gboolean   gbp_git_update_config_finish     (GbpGit                      *self,
                                             GAsyncResult                *result,
                                             GError                     **error);

G_END_DECLS
