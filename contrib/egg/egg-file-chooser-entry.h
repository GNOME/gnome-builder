/* egg-file-chooser-entry.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_FILE_CHOOSER_ENTRY_H
#define EGG_FILE_CHOOSER_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_FILE_CHOOSER_ENTRY (egg_file_chooser_entry_get_type())

G_DECLARE_DERIVABLE_TYPE (EggFileChooserEntry, egg_file_chooser_entry, EGG, FILE_CHOOSER_ENTRY, GtkBin)

struct _EggFileChooserEntryClass
{
  GtkBinClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GtkWidget *egg_file_chooser_entry_new      (const gchar          *title,
                                            GtkFileChooserAction  action);
GFile     *egg_file_chooser_entry_get_file (EggFileChooserEntry *self);
void       egg_file_chooser_entry_set_file (EggFileChooserEntry *self,
                                            GFile               *file);

G_END_DECLS

#endif /* EGG_FILE_CHOOSER_ENTRY_H */
