/* ipc-git-index-monitor.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define IPC_TYPE_GIT_INDEX_MONITOR (ipc_git_index_monitor_get_type())

G_DECLARE_FINAL_TYPE (IpcGitIndexMonitor, ipc_git_index_monitor, IPC, GIT_INDEX_MONITOR, GObject)

IpcGitIndexMonitor *ipc_git_index_monitor_new (GFile *location);

G_END_DECLS
