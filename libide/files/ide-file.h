/* ide-file.h
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

#ifndef IDE_FILE_H
#define IDE_FILE_H

#include <gtksourceview/gtksource.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE (ide_file_get_type())

G_DECLARE_FINAL_TYPE (IdeFile, ide_file, IDE, FILE, IdeObject)

IdeFile           *ide_file_new                  (IdeContext           *context,
                                                  GFile                *file);
IdeFile           *ide_file_new_for_path         (IdeContext           *context,
                                                  const gchar          *path);
gboolean           ide_file_get_is_temporary     (IdeFile              *self);
guint              ide_file_get_temporary_id     (IdeFile              *self);
GtkSourceLanguage *ide_file_get_language         (IdeFile              *self);
const gchar       *ide_file_get_language_id      (IdeFile              *self);
GFile             *ide_file_get_file             (IdeFile              *self);
guint              ide_file_hash                 (IdeFile              *self);
gboolean           ide_file_equal                (IdeFile              *self,
                                                  IdeFile              *other);
const gchar       *ide_file_get_path             (IdeFile              *self);
void               ide_file_load_settings_async  (IdeFile              *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IdeFileSettings   *ide_file_load_settings_finish (IdeFile              *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
void               ide_file_find_other_async     (IdeFile              *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IdeFile           *ide_file_find_other_finish    (IdeFile              *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
gint               ide_file_compare              (const IdeFile        *a,
                                                  const IdeFile        *b);


G_END_DECLS

#endif /* IDE_FILE_H */
