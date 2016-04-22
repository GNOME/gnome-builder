/* egg-simple-label.h
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

#ifndef EGG_SIMPLE_LABEL_H
#define EGG_SIMPLE_LABEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
 * This widget has one very simple purpose. Allow updating a simple
 * amount of text without causing resizes to propagate up the widget
 * hierarchy. Therefore, it only supports a very minimal amount of
 * features. The label text, and the width-chars to use for sizing.
 */

#define EGG_TYPE_SIMPLE_LABEL (egg_simple_label_get_type())

G_DECLARE_FINAL_TYPE (EggSimpleLabel, egg_simple_label, EGG, SIMPLE_LABEL, GtkWidget)

GtkWidget   *egg_simple_label_new             (const gchar    *label);
const gchar *egg_simple_label_get_label       (EggSimpleLabel *self);
void         egg_simple_label_set_label       (EggSimpleLabel *self,
                                               const gchar    *label);
gint         egg_simple_label_get_width_chars (EggSimpleLabel *self);
void         egg_simple_label_set_width_chars (EggSimpleLabel *self,
                                               gint            width_chars);
gfloat       egg_simple_label_get_xalign      (EggSimpleLabel *self);
void         egg_simple_label_set_xalign      (EggSimpleLabel *self,
                                               gfloat          xalign);

G_END_DECLS

#endif /* EGG_SIMPLE_LABEL_H */
