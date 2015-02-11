/* ide-file.h
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

#ifndef IDE_FILE_H
#define IDE_FILE_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE (ide_file_get_type())

G_DECLARE_FINAL_TYPE (IdeFile, ide_file, IDE, FILE, IdeObject)

struct _IdeFile
{
  IdeObject parent_instance;
};

IdeLanguage     *ide_file_get_language      (IdeFile *self);
GFile           *ide_file_get_file          (IdeFile *self);
const gchar     *ide_file_get_project_path  (IdeFile *self);
//IdeFileSettings *ide_file_get_file_settings (IdeFile *self);

G_END_DECLS

#endif /* IDE_FILE_H */
