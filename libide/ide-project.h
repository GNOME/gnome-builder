/* ide-project.h
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

#ifndef IDE_PROJECT_H
#define IDE_PROJECT_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT (ide_project_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeProject, ide_project, IDE, PROJECT, IdeObject)

struct _IdeProjectClass
{
  IdeObjectClass parent;
};

IdeProjectItem *ide_project_get_root          (IdeProject  *project);
const gchar    *ide_project_get_name          (IdeProject  *project);
IdeFile        *ide_project_get_file_for_path (IdeProject  *project,
                                               const gchar *path);

G_END_DECLS

#endif /* IDE_PROJECT_H */
