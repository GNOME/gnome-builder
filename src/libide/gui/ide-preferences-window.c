/* ide-preferences-window.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-window"

#include "config.h"

#include "ide-preferences-window.h"

struct _IdePreferencesWindow
{
  DzlApplicationWindow parent_window;
};

G_DEFINE_TYPE (IdePreferencesWindow, ide_preferences_window, DZL_TYPE_APPLICATION_WINDOW)

static void
ide_preferences_window_class_init (IdePreferencesWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-preferences-window.ui");
}

static void
ide_preferences_window_init (IdePreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
