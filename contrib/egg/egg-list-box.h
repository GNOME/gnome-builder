/* egg-list-box.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_LIST_BOX_H
#define EGG_LIST_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_LIST_BOX (egg_list_box_get_type())

G_DECLARE_DERIVABLE_TYPE (EggListBox, egg_list_box, EGG, LIST_BOX, GtkListBox)

struct _EggListBoxClass
{
  GtkListBoxClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

EggListBox  *egg_list_box_new               (GType        row_type,
                                             const gchar *property_name);
GType        egg_list_box_get_row_type      (EggListBox  *self);
const gchar *egg_list_box_get_property_name (EggListBox  *self);
GListModel  *egg_list_box_get_model         (EggListBox  *self);
void         egg_list_box_set_model         (EggListBox  *self,
                                             GListModel  *model);

G_END_DECLS

#endif /* EGG_LIST_BOX_H */
