/*
 * ipc-git-blame-impl.h
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#pragma once

#include <gio/gio.h>
#include <libgit2-glib/ggit.h>

#include "ipc-git-blame.h"

G_BEGIN_DECLS

#define IPC_TYPE_GIT_BLAME_IMPL (ipc_git_blame_impl_get_type())

G_DECLARE_FINAL_TYPE (IpcGitBlameImpl, ipc_git_blame_impl, IPC, GIT_BLAME_IMPL, IpcGitBlameSkeleton)

IpcGitBlame *ipc_git_blame_impl_new   (GgitRepository         *repository,
                                       const gchar            *path);
void         ipc_git_blame_impl_reset (IpcGitBlameImpl        *self);

G_END_DECLS
