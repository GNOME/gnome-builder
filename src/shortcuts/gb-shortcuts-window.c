/* gb-shortcuts-window.c
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

#include "gb-shortcuts-window.h"

struct _GbShortcutsWindow
{
  GbShortcutsDialog parent_instance;
};

G_DEFINE_TYPE (GbShortcutsWindow, gb_shortcuts_window, GB_TYPE_SHORTCUTS_DIALOG)

static void
gb_shortcuts_window_class_init (GbShortcutsWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/gb-shortcuts-window.ui");
}

static void
gb_shortcuts_window_init (GbShortcutsWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
