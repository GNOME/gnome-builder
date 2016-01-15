/* ide-perspective-switcher.c
 *
 * Copyright (C) 2016 Christian Hergert <christian@hergert.me>
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

#include "ide-perspective-switcher.h"

struct _IdePerspectiveSwitcher
{
  GtkStackSwitcher parent_instance;
};

G_DEFINE_TYPE (IdePerspectiveSwitcher, ide_perspective_switcher, GTK_TYPE_STACK_SWITCHER)

static void
ide_perspective_switcher_class_init (IdePerspectiveSwitcherClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_css_name (widget_class, "perspectiveswitcher");
}

static void
ide_perspective_switcher_init (IdePerspectiveSwitcher *self)
{
}
