/* ide-recent-projects.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_RECENT_PROJECTS_H
#define IDE_RECENT_PROJECTS_H

#include "ide-project-info.h"

G_BEGIN_DECLS

#define IDE_TYPE_RECENT_PROJECTS (ide_recent_projects_get_type())

#define IDE_RECENT_PROJECTS_GROUP                 "X-GNOME-Builder-Project"
#define IDE_RECENT_PROJECTS_LANGUAGE_GROUP_PREFIX "X-GNOME-Builder-Language:"
#define IDE_RECENT_PROJECTS_BUILD_SYSTEM_GROUP_PREFIX "X-GNOME-Builder-Build-System:"
#define IDE_RECENT_PROJECTS_BOOKMARK_FILENAME     "recent-projects.xbel"

G_DECLARE_FINAL_TYPE (IdeRecentProjects, ide_recent_projects, IDE, RECENT_PROJECTS, GObject)

IdeRecentProjects *ide_recent_projects_new              (void);
GPtrArray         *ide_recent_projects_get_projects     (IdeRecentProjects    *self);
gboolean           ide_recent_projects_get_busy         (IdeRecentProjects    *self);
void               ide_recent_projects_remove           (IdeRecentProjects    *self,
                                                         GList                *project_infos);
void               ide_recent_projects_discover_async   (IdeRecentProjects    *self,
                                                         gboolean              recent_only,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean           ide_recent_projects_discover_finish  (IdeRecentProjects    *self,
                                                         GAsyncResult         *result,
                                                         GError              **error);

G_END_DECLS

#endif /* IDE_RECENT_PROJECTS_H */
