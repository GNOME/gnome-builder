/* egg-slider.h
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

#ifndef EGG_SLIDER_H
#define EGG_SLIDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EGG_TYPE_SLIDER          (egg_slider_get_type())
#define EGG_TYPE_SLIDER_POSITION (egg_slider_position_get_type())

G_DECLARE_DERIVABLE_TYPE (EggSlider, egg_slider, EGG, SLIDER, GtkContainer)

typedef enum
{
  EGG_SLIDER_NONE,
  EGG_SLIDER_TOP,
  EGG_SLIDER_RIGHT,
  EGG_SLIDER_BOTTOM,
  EGG_SLIDER_LEFT,
} EggSliderPosition;

struct _EggSliderClass
{
  GtkContainerClass parent_instance;
};

GType              egg_slider_position_get_type (void);
GtkWidget         *egg_slider_new               (void);
void               egg_slider_add_slider        (EggSlider         *self,
                                                 GtkWidget         *widget,
                                                 EggSliderPosition  position);
EggSliderPosition  egg_slider_get_position      (EggSlider         *self);
void               egg_slider_set_position      (EggSlider         *self,
                                                 EggSliderPosition  position);

G_END_DECLS

#endif /* EGG_SLIDER_H */
