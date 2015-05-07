/* ide-project.h
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

#ifndef IDE_PROJECT_H
#define IDE_PROJECT_H

#include "ide-object.h"
#include "ide-project-files.h"

G_BEGIN_DECLS

#define IDE_TYPE_PROJECT (ide_project_get_type())

G_DECLARE_FINAL_TYPE (IdeProject, ide_project, IDE, PROJECT, IdeObject)

IdeProjectItem  *ide_project_get_root           (IdeProject           *self);
const gchar     *ide_project_get_name           (IdeProject           *self);
const gchar     *ide_project_get_id             (IdeProject           *self);
IdeFile         *ide_project_get_file_for_path  (IdeProject           *self,
                                                 const gchar          *path);
IdeFile         *ide_project_get_project_file   (IdeProject           *self,
                                                 GFile                *gfile);
void             ide_project_reader_lock        (IdeProject           *self);
void             ide_project_reader_unlock      (IdeProject           *self);
void             ide_project_writer_lock        (IdeProject           *self);
void             ide_project_writer_unlock      (IdeProject           *self);
void             ide_project_add_file           (IdeProject           *self,
                                                 IdeProjectFile       *file);
IdeProjectFiles *ide_project_get_files          (IdeProject           *self);
void             ide_project_rename_file_async  (IdeProject           *self,
                                                 GFile                *orig_file,
                                                 GFile                *new_file,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
gboolean         ide_project_rename_file_finish (IdeProject           *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);
void             ide_project_trash_file_async   (IdeProject           *self,
                                                 GFile                *file,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
gboolean         ide_project_trash_file_finish  (IdeProject           *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_END_DECLS

#endif /* IDE_PROJECT_H */
