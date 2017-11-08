/* ide-project-file.h
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

#include <gio/gio.h>

#include "ide-version-macros.h"

#include "files/ide-file.h"
#include "projects/ide-project-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_FILE (ide_project_file_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeProjectFile, ide_project_file, IDE, PROJECT_FILE, IdeProjectItem)

struct _IdeProjectFileClass
{
  IdeProjectItemClass parent;
};

IDE_AVAILABLE_IN_ALL
GFile       *ide_project_file_get_file         (IdeProjectFile *self);
IDE_AVAILABLE_IN_ALL
GFileInfo   *ide_project_file_get_file_info    (IdeProjectFile *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_project_file_get_name         (IdeProjectFile *self);
IDE_AVAILABLE_IN_ALL
const gchar *ide_project_file_get_path         (IdeProjectFile *self);
IDE_AVAILABLE_IN_ALL
gboolean     ide_project_file_get_is_directory (IdeProjectFile *self);

G_END_DECLS
