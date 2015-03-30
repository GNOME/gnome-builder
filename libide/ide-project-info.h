/* ide-project-info.h
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

#ifndef IDE_PROJECT_INFO_H
#define IDE_PROJECT_INFO_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_INFO (ide_project_info_get_type())

G_DECLARE_FINAL_TYPE (IdeProjectInfo, ide_project_info, IDE, PROJECT_INFO, GObject)

GFile       *ide_project_info_get_file      (IdeProjectInfo *self);
GFile       *ide_project_info_get_directory (IdeProjectInfo *self);
const gchar *ide_project_info_get_name      (IdeProjectInfo *self);
void         ide_project_info_set_file      (IdeProjectInfo *self,
                                             GFile          *file);
void         ide_project_info_set_directory (IdeProjectInfo *self,
                                             GFile          *directory);
void         ide_project_info_set_name      (IdeProjectInfo *self,
                                             const gchar    *name);

G_END_DECLS

#endif /* IDE_PROJECT_INFO_H */
