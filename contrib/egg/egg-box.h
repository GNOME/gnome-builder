/* egg-box.h
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#ifndef EGG_BOX_H
#define EGG_BOX_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_BOX (egg_box_get_type())

G_DECLARE_DERIVABLE_TYPE (EggBox, egg_box, EGG, BOX, GtkBox)

struct _EggBoxClass
{
  GtkBoxClass parent_class;
};

GtkWidget *egg_box_new                   (void);
gint       egg_box_get_max_width_request (EggBox *self);
void       egg_box_set_max_width_request (EggBox *self,
                                          gint    max_width_request);

G_END_DECLS

#endif /* EGG_BOX_H */
