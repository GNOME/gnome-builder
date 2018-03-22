/* ide-file.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_FILE (ide_file_get_type())

#define IDE_FILE_ATTRIBUTE_POSITION "metadata::libide-position"

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFile, ide_file, IDE, FILE, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeFile           *ide_file_new                  (IdeContext           *context,
                                                  GFile                *file);
IDE_AVAILABLE_IN_ALL
IdeFile           *ide_file_new_for_path         (IdeContext           *context,
                                                  const gchar          *path);
IDE_AVAILABLE_IN_ALL
gboolean           ide_file_get_is_temporary     (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
guint              ide_file_get_temporary_id     (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
GtkSourceLanguage *ide_file_get_language         (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_file_get_language_id      (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
GFile             *ide_file_get_file             (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
guint              ide_file_hash                 (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
gboolean           ide_file_equal                (IdeFile              *self,
                                                  IdeFile              *other);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_file_get_path             (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
void               ide_file_load_settings_async  (IdeFile              *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeFileSettings   *ide_file_load_settings_finish (IdeFile              *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
IDE_AVAILABLE_IN_3_28
IdeFileSettings   *ide_file_peek_settings        (IdeFile              *self);
IDE_AVAILABLE_IN_ALL
void               ide_file_find_other_async     (IdeFile              *self,
                                                  GCancellable         *cancellable,
                                                  GAsyncReadyCallback   callback,
                                                  gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeFile           *ide_file_find_other_finish    (IdeFile              *self,
                                                  GAsyncResult         *result,
                                                  GError              **error);
IDE_AVAILABLE_IN_ALL
gint               ide_file_compare              (const IdeFile        *a,
                                                  const IdeFile        *b);
const gchar       *_ide_file_get_content_type    (IdeFile              *self) G_GNUC_INTERNAL;
void               _ide_file_set_content_type    (IdeFile              *self,
                                                  const gchar          *content_type) G_GNUC_INTERNAL;
GtkSourceFile     *_ide_file_get_source_file     (IdeFile              *self) G_GNUC_INTERNAL;


G_END_DECLS
