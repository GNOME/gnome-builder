/* gb-project-file.h
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

#ifndef GB_PROJECT_FILE_H
#define GB_PROJECT_FILE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GB_TYPE_PROJECT_FILE (gb_project_file_get_type())

G_DECLARE_FINAL_TYPE (GbProjectFile, gb_project_file, GB, PROJECT_FILE, GObject)

GbProjectFile *gb_project_file_new                       (GFile         *directory,
                                                          GFileInfo     *file_info);
GFile         *gb_project_file_get_file                  (GbProjectFile *self);
void           gb_project_file_set_file                  (GbProjectFile *self,
                                                          GFile         *file);
GFileInfo     *gb_project_file_get_file_info             (GbProjectFile *self);
void           gb_project_file_set_file_info             (GbProjectFile *self,
                                                          GFileInfo     *file_info);
gboolean       gb_project_file_get_is_directory          (GbProjectFile *self);
const gchar   *gb_project_file_get_display_name          (GbProjectFile *self);
const gchar   *gb_project_file_get_icon_name             (GbProjectFile *self);
gint           gb_project_file_compare_directories_first (GbProjectFile *a,
                                                          GbProjectFile *b);
gint           gb_project_file_compare                   (GbProjectFile *a,
                                                          GbProjectFile *b);

G_END_DECLS

#endif /* GB_PROJECT_FILE_H */
