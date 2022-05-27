/* ide-vcs-monitor.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_VCS_INSIDE) && !defined (IDE_VCS_COMPILATION)
# error "Only <libide-vcs.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-vcs.h"
#include "ide-vcs-file-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_VCS_MONITOR (ide_vcs_monitor_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeVcsMonitor, ide_vcs_monitor, IDE, VCS_MONITOR, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeVcsMonitor  *ide_vcs_monitor_from_context (IdeContext    *context);
IDE_AVAILABLE_IN_ALL
IdeVcsFileInfo *ide_vcs_monitor_ref_info     (IdeVcsMonitor *self,
                                              GFile         *file);
IDE_AVAILABLE_IN_ALL
GFile          *ide_vcs_monitor_ref_root     (IdeVcsMonitor *self);
IDE_AVAILABLE_IN_ALL
void            ide_vcs_monitor_set_root     (IdeVcsMonitor *self,
                                              GFile         *file);
IDE_AVAILABLE_IN_ALL
IdeVcs         *ide_vcs_monitor_ref_vcs      (IdeVcsMonitor *self);
IDE_AVAILABLE_IN_ALL
void            ide_vcs_monitor_set_vcs      (IdeVcsMonitor *self,
                                              IdeVcs        *vcs);
IDE_AVAILABLE_IN_ALL
guint64         ide_vcs_monitor_get_sequence (IdeVcsMonitor *self);

G_END_DECLS
