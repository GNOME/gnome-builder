/* egg-column-layout.h
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

#ifndef EGG_COLUMN_LAYOUT_H
#define EGG_COLUMN_LAYOUT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_COLUMN_LAYOUT (egg_column_layout_get_type())

G_DECLARE_DERIVABLE_TYPE (EggColumnLayout, egg_column_layout, EGG, COLUMN_LAYOUT, GtkContainer)

struct _EggColumnLayoutClass
{
  GtkContainerClass parent;
};

GtkWidget *egg_column_layout_new                (void);
guint      egg_column_layout_get_max_columns    (EggColumnLayout *self);
void       egg_column_layout_set_max_columns    (EggColumnLayout *self,
                                                 guint            max_columns);
gint       egg_column_layout_get_column_width   (EggColumnLayout *self);
void       egg_column_layout_set_column_width   (EggColumnLayout *self,
                                                 gint             column_width);
gint       egg_column_layout_get_column_spacing (EggColumnLayout *self);
void       egg_column_layout_set_column_spacing (EggColumnLayout *self,
                                                 gint             column_spacing);
gint       egg_column_layout_get_row_spacing    (EggColumnLayout *self);
void       egg_column_layout_set_row_spacing    (EggColumnLayout *self,
                                                 gint             row_spacing);

G_END_DECLS

#endif /* EGG_COLUMN_LAYOUT_H */
