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

#include <libgit2-glib/ggit.h>
#include <libide-code.h>

G_BEGIN_DECLS

#define GBP_TYPE_GIT_BUFFER_CHANGE_MONITOR (gbp_git_buffer_change_monitor_get_type())

G_DECLARE_FINAL_TYPE (GbpGitBufferChangeMonitor, gbp_git_buffer_change_monitor, GBP, GIT_BUFFER_CHANGE_MONITOR, IdeBufferChangeMonitor)

void gbp_git_buffer_change_monitor_set_repository (GbpGitBufferChangeMonitor *self,
                                                   GgitRepository            *repository);

G_END_DECLS
