/* ide-directory-reaper.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_DIRECTORY_REAPER_H
#define IDE_DIRECTORY_REAPER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_DIRECTORY_REAPER (ide_directory_reaper_get_type())

G_DECLARE_FINAL_TYPE (IdeDirectoryReaper, ide_directory_reaper, IDE, DIRECTORY_REAPER, GObject)

IdeDirectoryReaper *ide_directory_reaper_new               (void);
void                ide_directory_reaper_add_directory     (IdeDirectoryReaper   *self,
                                                            GFile                *directory,
                                                            GTimeSpan             min_age);
void                ide_directory_reaper_add_file          (IdeDirectoryReaper   *self,
                                                            GFile                *file,
                                                            GTimeSpan             min_age);
void                ide_directory_reaper_add_glob          (IdeDirectoryReaper   *self,
                                                            GFile                *directory,
                                                            const gchar          *glob,
                                                            GTimeSpan             min_age);
gboolean            ide_directory_reaper_execute           (IdeDirectoryReaper   *self,
                                                            GCancellable         *cancellable,
                                                            GError              **error);
void                ide_directory_reaper_execute_async     (IdeDirectoryReaper   *self,
                                                            GCancellable         *cancellable,
                                                            GAsyncReadyCallback   callback,
                                                            gpointer              user_data);
gboolean            ide_directory_reaper_execute_finish    (IdeDirectoryReaper   *self,
                                                            GAsyncResult         *result,
                                                            GError              **error);

G_END_DECLS

#endif /* IDE_DIRECTORY_REAPER_H */
