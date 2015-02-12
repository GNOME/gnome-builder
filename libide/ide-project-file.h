/* ide-project-file.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_PROJECT_FILE_H
#define IDE_PROJECT_FILE_H

#include <gio/gio.h>

#include "ide-file.h"
#include "ide-project-item.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT_FILE (ide_project_file_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeProjectFile, ide_project_file, IDE, PROJECT_FILE, IdeProjectItem)

struct _IdeProjectFileClass
{
  GObjectClass parent;
};

GFile       *ide_project_file_get_file      (IdeProjectFile *file);
GFileInfo   *ide_project_file_get_file_info (IdeProjectFile *file);
const gchar *ide_project_file_get_name      (IdeProjectFile *file);

G_END_DECLS

#endif /* IDE_PROJECT_FILE_H */
