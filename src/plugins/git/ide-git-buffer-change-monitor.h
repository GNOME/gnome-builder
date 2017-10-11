/* ide-git-buffer-change-monitor.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
 */

#pragma once

#include <ide.h>

G_BEGIN_DECLS

#define IDE_TYPE_GIT_BUFFER_CHANGE_MONITOR (ide_git_buffer_change_monitor_get_type())

G_DECLARE_FINAL_TYPE (IdeGitBufferChangeMonitor, ide_git_buffer_change_monitor,
                      IDE, GIT_BUFFER_CHANGE_MONITOR, IdeBufferChangeMonitor)

G_END_DECLS
