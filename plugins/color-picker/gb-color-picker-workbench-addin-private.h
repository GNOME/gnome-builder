/* gb-color-picker-workbench-addin-private.h
 *
 * Copyright (C) 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#ifndef GB_COLOR_PICKER_WORKBENCH_ADDIN_PRIVATE_H
#define GB_COLOR_PICKER_WORKBENCH_ADDIN_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

struct _GbColorPickerWorkbenchAddin
{
  GObject                parent_instance;

  GHashTable            *views;
  IdeWorkbench          *workbench;
  IdeEditorPerspective  *editor;
  IdeLayoutView         *active_view;
  GtkWidget             *dock;
  GtkWidget             *color_panel;
  GbColorPickerPrefs    *prefs;

  guint                  dock_count;
  guint                  monitor_count;
};

G_END_DECLS

#endif /* GB_COLOR_PICKER_WORKBENCH_ADDIN_PRIVATE_H */
