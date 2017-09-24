/* gb-new-file-popover.h
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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_NEW_FILE_POPOVER (gb_new_file_popover_get_type())

G_DECLARE_FINAL_TYPE (GbNewFilePopover, gb_new_file_popover, GB, NEW_FILE_POPOVER, GtkPopover)

GFileType  gb_new_file_popover_get_file_type (GbNewFilePopover *self);
void       gb_new_file_popover_set_file_type (GbNewFilePopover *self,
                                              GFileType         file_type);
void       gb_new_file_popover_set_directory (GbNewFilePopover *self,
                                              GFile            *directory);
GFile     *gb_new_file_popover_get_directory (GbNewFilePopover *self);

G_END_DECLS
