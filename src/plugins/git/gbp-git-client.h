/* gbp-git-client.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <libide-core.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_CLIENT (gbp_git_client_get_type())

typedef enum
{
  GBP_GIT_REF_BRANCH = 1 << 0,
  GBP_GIT_REF_TAG    = 1 << 1,
  GBP_GIT_REF_ANY    = GBP_GIT_REF_BRANCH | GBP_GIT_REF_TAG,
} GbpGitRefKind;

G_DECLARE_FINAL_TYPE (GbpGitClient, gbp_git_client, GBP, GIT_CLIENT, IdeObject)

GbpGitClient *gbp_git_client_from_context             (IdeContext             *context);
void          gbp_git_client_call_async               (GbpGitClient           *self,
                                                       const gchar            *method,
                                                       GVariant               *params,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_call_finish              (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GVariant              **reply,
                                                       GError                **error);
void          gbp_git_client_is_ignored_async         (GbpGitClient           *self,
                                                       const gchar            *path,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_is_ignored_finish        (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);
void          gbp_git_client_list_status_async        (GbpGitClient           *self,
                                                       const gchar            *directory_or_file,
                                                       gboolean                include_descendants,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
GPtrArray    *gbp_git_client_list_status_finish       (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);
void          gbp_git_client_list_refs_by_kind_async  (GbpGitClient           *self,
                                                       GbpGitRefKind           kind,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
GPtrArray    *gbp_git_client_list_refs_by_kind_finish (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);
void          gbp_git_client_switch_branch_async      (GbpGitClient           *self,
                                                       const gchar            *branch_name,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_switch_branch_finish     (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       gchar                 **switch_to_directory,
                                                       GError                **error);
void          gbp_git_client_clone_url_async          (GbpGitClient           *self,
                                                       const gchar            *url,
                                                       GFile                  *destination,
                                                       const gchar            *branch,
                                                       IdeNotification        *notif,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_clone_url_finish         (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);
void          gbp_git_client_update_submodules_async  (GbpGitClient           *self,
                                                       IdeNotification        *notif,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_update_submodules_finish (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);
GVariant     *gbp_git_client_read_config              (GbpGitClient           *self,
                                                       const gchar            *key,
                                                       GCancellable           *cancellable,
                                                       GError                **error);
gboolean      gbp_git_client_update_config            (GbpGitClient           *self,
                                                       gboolean                global,
                                                       const gchar            *key,
                                                       GVariant               *value,
                                                       GCancellable           *cancellable,
                                                       GError                **error);
void          gbp_git_client_update_config_async      (GbpGitClient           *self,
                                                       gboolean                global,
                                                       const gchar            *key,
                                                       GVariant               *value,
                                                       GCancellable           *cancellable,
                                                       GAsyncReadyCallback     callback,
                                                       gpointer                user_data);
gboolean      gbp_git_client_update_config_finish     (GbpGitClient           *self,
                                                       GAsyncResult           *result,
                                                       GError                **error);

G_END_DECLS
