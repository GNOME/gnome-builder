/* gbp-git-buffer-change-monitor.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-code.h>

#include "daemon/ipc-git-repository.h"

G_BEGIN_DECLS

#define GBP_TYPE_GIT_BUFFER_CHANGE_MONITOR (gbp_git_buffer_change_monitor_get_type())

G_DECLARE_FINAL_TYPE (GbpGitBufferChangeMonitor, gbp_git_buffer_change_monitor, GBP, GIT_BUFFER_CHANGE_MONITOR, IdeBufferChangeMonitor)

IdeBufferChangeMonitor *gbp_git_buffer_change_monitor_new         (IdeBuffer                  *buffer,
                                                                   IpcGitRepository           *repository,
                                                                   GFile                      *file,
                                                                   GCancellable               *cancellable,
                                                                   GError                    **error);
void                    gbp_git_buffer_change_monitor_wait_async  (GbpGitBufferChangeMonitor  *self,
                                                                   GCancellable               *cancellable,
                                                                   GAsyncReadyCallback         callback,
                                                                   gpointer                    user_data);
gboolean                gbp_git_buffer_change_monitor_wait_finish (GbpGitBufferChangeMonitor  *self,
                                                                   GAsyncResult               *result,
                                                                   GError                    **error);

G_END_DECLS
