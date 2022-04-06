/* ide-file-chooser-entry.h
 *
 * Copyright (C) 2016-2022 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_FILE_CHOOSER_ENTRY (ide_file_chooser_entry_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeFileChooserEntry, ide_file_chooser_entry, IDE, FILE_CHOOSER_ENTRY, GtkWidget)

IDE_AVAILABLE_IN_ALL
GtkWidget           *ide_file_chooser_entry_new       (const gchar          *title,
                                                       GtkFileChooserAction  action);
IDE_AVAILABLE_IN_ALL
GFile               *ide_file_chooser_entry_get_file  (IdeFileChooserEntry  *self);
IDE_AVAILABLE_IN_ALL
void                 ide_file_chooser_entry_set_file  (IdeFileChooserEntry  *self,
                                                       GFile                *file);
IDE_AVAILABLE_IN_ALL
GtkEntry            *ide_file_chooser_entry_get_entry (IdeFileChooserEntry  *self);

G_END_DECLS
