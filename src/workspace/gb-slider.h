/* gb-slider.h
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

#ifndef GB_SLIDER_H
#define GB_SLIDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SLIDER          (gb_slider_get_type())
#define GB_TYPE_SLIDER_POSITION (gb_slider_position_get_type())

G_DECLARE_DERIVABLE_TYPE (GbSlider, gb_slider, GB, SLIDER, GtkContainer)

typedef enum
{
  GB_SLIDER_NONE,
  GB_SLIDER_TOP,
  GB_SLIDER_RIGHT,
  GB_SLIDER_BOTTOM,
  GB_SLIDER_LEFT,
} GbSliderPosition;

struct _GbSliderClass
{
  GtkContainerClass parent_instance;
};

GType             gb_slider_position_get_type (void);
GtkWidget        *gb_slider_new               (void);
void              gb_slider_add_slider        (GbSlider        *self,
                                               GtkWidget       *widget,
                                               GtkPositionType  position);
GbSliderPosition  gb_slider_get_position      (GbSlider         *self);
void              gb_slider_set_position      (GbSlider         *self,
                                               GbSliderPosition  position);

G_END_DECLS

#endif /* GB_SLIDER_H */
