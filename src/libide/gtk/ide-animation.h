/* ide-animation.h
 *
 * Copyright (C) 2010-2016 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (IDE_GTK_INSIDE) && !defined (IDE_GTK_COMPILATION)
# error "Only <libide-gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_ANIMATION (ide_animation_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeAnimation, ide_animation, IDE, ANIMATION, GInitiallyUnowned)

typedef enum _IdeAnimationMode
{
  IDE_ANIMATION_LINEAR,
  IDE_ANIMATION_EASE_IN_QUAD,
  IDE_ANIMATION_EASE_OUT_QUAD,
  IDE_ANIMATION_EASE_IN_OUT_QUAD,
  IDE_ANIMATION_EASE_IN_CUBIC,
  IDE_ANIMATION_EASE_OUT_CUBIC,
  IDE_ANIMATION_EASE_IN_OUT_CUBIC,

  IDE_ANIMATION_LAST
} IdeAnimationMode;

IDE_AVAILABLE_IN_ALL
void          ide_animation_start              (IdeAnimation     *animation);
IDE_AVAILABLE_IN_ALL
void          ide_animation_stop               (IdeAnimation     *animation);
IDE_AVAILABLE_IN_ALL
void          ide_animation_add_property       (IdeAnimation     *animation,
                                                GParamSpec       *pspec,
                                                const GValue     *value);
IDE_AVAILABLE_IN_ALL
IdeAnimation *ide_object_animatev              (gpointer          object,
                                                IdeAnimationMode  mode,
                                                guint             duration_msec,
                                                GdkFrameClock    *frame_clock,
                                                const gchar      *first_property,
                                                va_list           args);
IDE_AVAILABLE_IN_ALL
IdeAnimation* ide_object_animate               (gpointer          object,
                                                IdeAnimationMode  mode,
                                                guint             duration_msec,
                                                GdkFrameClock    *frame_clock,
                                                const gchar      *first_property,
                                                ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
IdeAnimation* ide_object_animate_full          (gpointer          object,
                                                IdeAnimationMode  mode,
                                                guint             duration_msec,
                                                GdkFrameClock    *frame_clock,
                                                GDestroyNotify    notify,
                                                gpointer          notify_data,
                                                const gchar      *first_property,
                                                ...) G_GNUC_NULL_TERMINATED;
IDE_AVAILABLE_IN_ALL
guint         ide_animation_calculate_duration (GdkMonitor       *monitor,
                                                gdouble           from_value,
                                                gdouble           to_value);

G_END_DECLS
