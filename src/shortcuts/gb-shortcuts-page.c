/* gb-shortcuts-page.c
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

#include "gb-shortcuts-page.h"

struct _GbShortcutsPage
{
  GtkBox parent_instance;
};

G_DEFINE_TYPE (GbShortcutsPage, gb_shortcuts_page, GTK_TYPE_BOX)

static void
gb_shortcuts_page_class_init (GbShortcutsPageClass *klass)
{
}

static void
gb_shortcuts_page_init (GbShortcutsPage *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (self), 22);
}
