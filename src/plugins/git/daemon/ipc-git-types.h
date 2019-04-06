/* ipc-git-types.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef enum
{
  IPC_GIT_REF_BRANCH = 0,
  IPC_GIT_REF_TAG    = 1,
} IpcGitRefKind;

typedef enum
{
  IPC_GIT_COMMIT_FLAGS_NONE     = 0,
  IPC_GIT_COMMIT_FLAGS_AMEND    = 1 << 0,
  IPC_GIT_COMMIT_FLAGS_SIGNOFF  = 1 << 1,
  IPC_GIT_COMMIT_FLAGS_GPG_SIGN = 1 << 2,
} IpcGitCommitFlags;

typedef enum
{
  IPC_GIT_PUSH_FLAGS_NONE   = 0,
  IPC_GIT_PUSH_FLAGS_ATOMIC = 1 << 0,
} IpcGitPushFlags;

G_END_DECLS
