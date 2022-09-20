/* ide-recent-projects.h
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

#include <libide-core.h>
#include <libide-projects.h>

G_BEGIN_DECLS

#define IDE_TYPE_RECENT_PROJECTS (ide_recent_projects_get_type())

#define IDE_RECENT_PROJECTS_GROUP                          "X-GNOME-Builder-Project"
#define IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX          "X-GNOME-Builder-Language:"
#define IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX      "X-GNOME-Builder-Build-System:"
#define IDE_RECENT_PROJECTS_BUILD_SYSTEM_HINT_GROUP_PREFIX "X-GNOME-Builder-Build-System-Hint:"
#define IDE_RECENT_PROJECTS_DIRECTORY                      "X-GNOME-Builder-Directory:"
#define IDE_RECENT_PROJECTS_BOOKMARK_FILENAME              "recent-projects.xbel"

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRecentProjects, ide_recent_projects, IDE, RECENT_PROJECTS, GObject)

IDE_AVAILABLE_IN_ALL
IdeRecentProjects *ide_recent_projects_get_default       (void);
IDE_AVAILABLE_IN_ALL
IdeRecentProjects *ide_recent_projects_new               (void);
IDE_AVAILABLE_IN_ALL
void               ide_recent_projects_invalidate        (IdeRecentProjects *self);
IDE_AVAILABLE_IN_ALL
void               ide_recent_projects_remove            (IdeRecentProjects *self,
                                                          GList             *project_infos);
IDE_AVAILABLE_IN_ALL
gchar             *ide_recent_projects_find_by_directory (IdeRecentProjects *self,
                                                          const gchar       *directory);

G_END_DECLS
