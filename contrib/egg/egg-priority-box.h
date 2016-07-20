/* egg-priority-box.h
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

#ifndef EGG_PRIORITY_BOX_H
#define EGG_PRIORITY_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_PRIORITY_BOX (egg_priority_box_get_type())

G_DECLARE_DERIVABLE_TYPE (EggPriorityBox, egg_priority_box, EGG, PRIORITY_BOX, GtkBox)

struct _EggPriorityBoxClass
{
  GtkBoxClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

GtkWidget *egg_priority_box_new (void);

G_END_DECLS

#endif /* EGG_PRIORITY_BOX_H */
