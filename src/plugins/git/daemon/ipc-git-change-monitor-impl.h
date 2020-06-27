/* ipc-git-change-monitor-impl.h
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

#include <libgit2-glib/ggit.h>

#include "ipc-git-change-monitor.h"

G_BEGIN_DECLS

#define IPC_TYPE_GIT_CHANGE_MONITOR_IMPL (ipc_git_change_monitor_impl_get_type())

G_DECLARE_FINAL_TYPE (IpcGitChangeMonitorImpl, ipc_git_change_monitor_impl, IPC, GIT_CHANGE_MONITOR_IMPL, IpcGitChangeMonitorSkeleton)

IpcGitChangeMonitor *ipc_git_change_monitor_impl_new   (GgitRepository          *repository,
                                                        const gchar             *path);
void                 ipc_git_change_monitor_impl_reset (IpcGitChangeMonitorImpl *self);

G_END_DECLS
