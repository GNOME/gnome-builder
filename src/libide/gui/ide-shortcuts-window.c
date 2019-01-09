/* ide-shortcuts-window.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-shortcuts-window"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-shortcuts-window-private.h"

struct _IdeShortcutsWindow
{
  GtkShortcutsWindow parent_instance;
};

G_DEFINE_TYPE (IdeShortcutsWindow, ide_shortcuts_window, GTK_TYPE_SHORTCUTS_WINDOW)

static void
ide_shortcuts_window_class_init (IdeShortcutsWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-shortcuts-window.ui");
}

static void
ide_shortcuts_window_init (IdeShortcutsWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
