/* ipc-git-repository-impl.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ipc-git-repository.h"

G_BEGIN_DECLS

#define IPC_TYPE_GIT_REPOSITORY_IMPL (ipc_git_repository_impl_get_type())

G_DECLARE_FINAL_TYPE (IpcGitRepositoryImpl, ipc_git_repository_impl, IPC, GIT_REPOSITORY_IMPL, IpcGitRepositorySkeleton)

IpcGitRepository *ipc_git_repository_impl_open (GFile   *location,
                                                GError **error);

G_END_DECLS
