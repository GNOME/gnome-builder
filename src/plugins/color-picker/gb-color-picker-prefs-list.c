/* gb-color-picker-prefs-list.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gb-color-picker-prefs-list.h"

struct _GbColorPickerPrefsList
{
  GtkListBox parent_instance;
};

G_DEFINE_TYPE (GbColorPickerPrefsList, gb_color_picker_prefs_list, GTK_TYPE_LIST_BOX)

GbColorPickerPrefsList *
gb_color_picker_prefs_list_new (void)
{
  return g_object_new (GB_TYPE_COLOR_PICKER_PREFS_LIST, NULL);
}

static void
gb_color_picker_prefs_list_row_activated (GtkListBox      *listbox,
                                          GtkListBoxRow   *row)
{
  GtkWidget *child;

  g_assert (GTK_IS_LIST_BOX (listbox));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  child = gtk_bin_get_child (GTK_BIN (row));
  if (child != NULL)
    gtk_widget_activate (child);
}

static void
gb_color_picker_prefs_list_class_init (GbColorPickerPrefsListClass *klass)
{
  GtkListBoxClass *listbox_class = GTK_LIST_BOX_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  listbox_class->row_activated = gb_color_picker_prefs_list_row_activated;

  gtk_widget_class_set_css_name (widget_class, "gbcolorpickerprefslist");
}

static void
gb_color_picker_prefs_list_init (GbColorPickerPrefsList *self)
{
  g_object_set (G_OBJECT (self), "selection-mode", GTK_SELECTION_NONE, NULL);
}
