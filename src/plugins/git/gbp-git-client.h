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

#include "daemon/ipc-git-service.h"

G_BEGIN_DECLS

#define GBP_TYPE_GIT_CLIENT (gbp_git_client_get_type())

G_DECLARE_FINAL_TYPE (GbpGitClient, gbp_git_client, GBP, GIT_CLIENT, IdeObject)

GbpGitClient  *gbp_git_client_from_context       (IdeContext           *context);
IpcGitService *gbp_git_client_get_service        (GbpGitClient         *self,
                                                  GCancellable         *cancellable,
                                                  GError              **error);
void           gbp_git_client_get_service_async  (GbpGitClient         *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IpcGitService *gbp_git_client_get_service_finish (GbpGitClient         *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);

G_END_DECLS
