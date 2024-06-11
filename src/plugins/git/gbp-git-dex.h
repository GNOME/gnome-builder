/*
 * gbp-git-dex.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <libdex.h>

#include "ipc-git-repository.h"

G_BEGIN_DECLS

static inline void
ipc_git_repository_list_status_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr(DexPromise) promise = user_data;
  g_autoptr(GVariant) files = NULL;
  GError *error = NULL;

  if (!ipc_git_repository_call_list_status_finish (IPC_GIT_REPOSITORY (object), &files, result, &error))
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_variant (promise, g_steal_pointer (&files));
}

static inline DexFuture *
ipc_git_repository_list_status (IpcGitRepository *repository,
                                GgitStatusOption  status_option,
                                const char       *path)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  ipc_git_repository_call_list_status (repository,
                                       status_option,
                                       path,
                                       dex_promise_get_cancellable (promise),
                                       ipc_git_repository_list_status_cb,
                                       dex_ref (promise));
  return DEX_FUTURE (promise);
}

G_END_DECLS
