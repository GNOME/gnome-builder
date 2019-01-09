/* gstyle-slidein.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GSTYLE_TYPE_SLIDEIN_DIRECTION_TYPE (gstyle_slidein_type_get_type())

typedef enum
{
  GSTYLE_SLIDEIN_DIRECTION_TYPE_NONE,
  GSTYLE_SLIDEIN_DIRECTION_TYPE_RIGHT,
  GSTYLE_SLIDEIN_DIRECTION_TYPE_LEFT,
  GSTYLE_SLIDEIN_DIRECTION_TYPE_UP,
  GSTYLE_SLIDEIN_DIRECTION_TYPE_DOWN,
} GstyleSlideinDirectionType;

#define GSTYLE_TYPE_SLIDEIN (gstyle_slidein_get_type())

G_DECLARE_FINAL_TYPE (GstyleSlidein, gstyle_slidein, GSTYLE, SLIDEIN, GtkEventBox)

GType                        gstyle_slidein_type_get_type                    (void);

GtkWidget                   *gstyle_slidein_new                              (void);
void                         gstyle_slidein_set_slide_margin                 (GstyleSlidein               *self,
                                                                              guint                        slide_margin);
guint                        gstyle_slidein_get_slide_margin                 (GstyleSlidein               *self);
void                         gstyle_slidein_set_duration                     (GstyleSlidein               *self,
                                                                              gdouble                      duration);
gdouble                      gstyle_slidein_get_duration                     (GstyleSlidein               *self);
void                         gstyle_slidein_reset_duration                   (GstyleSlidein               *self);
gboolean                     gstyle_slidein_get_animation_state              (GstyleSlidein               *self,
                                                                              gboolean                    *direction);
void                         gstyle_slidein_add_slide                        (GstyleSlidein               *self,
                                                                              GtkWidget                   *slide);
void                         gstyle_slidein_remove_slide                     (GstyleSlidein               *self);
gboolean                     gstyle_slidein_reveal_slide                     (GstyleSlidein               *self,
                                                                              gboolean                     reveal);

GstyleSlideinDirectionType   gstyle_slidein_get_direction_type               (GstyleSlidein               *self);
gboolean                     gstyle_slidein_get_interpolate_size             (GstyleSlidein               *self);
gboolean                     gstyle_slidein_get_revealed                     (GstyleSlidein               *self);
gdouble                      gstyle_slidein_get_slide_fraction               (GstyleSlidein               *self);

void                         gstyle_slidein_set_direction_type               (GstyleSlidein               *self,
                                                                              GstyleSlideinDirectionType   direction_type);
void                         gstyle_slidein_set_interpolate_size             (GstyleSlidein               *self,
                                                                              gboolean                     interpolate_size);
void                         gstyle_slidein_set_slide_fraction               (GstyleSlidein               *self,
                                                                              gdouble                      slide_fraction);

G_END_DECLS
