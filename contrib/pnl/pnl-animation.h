/* pnl-animation.h
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

#if !defined(PNL_INSIDE) && !defined(PNL_COMPILATION)
# error "Only <pnl.h> can be included directly."
#endif

#ifndef PNL_ANIMATION_H
#define PNL_ANIMATION_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define PNL_TYPE_ANIMATION      (pnl_animation_get_type())
#define PNL_TYPE_ANIMATION_MODE (pnl_animation_mode_get_type())

G_DECLARE_FINAL_TYPE (PnlAnimation, pnl_animation, PNL, ANIMATION, GInitiallyUnowned)

typedef enum
{
  PNL_ANIMATION_LINEAR,
  PNL_ANIMATION_EASE_IN_QUAD,
  PNL_ANIMATION_EASE_OUT_QUAD,
  PNL_ANIMATION_EASE_IN_OUT_QUAD,
  PNL_ANIMATION_EASE_IN_CUBIC,
  PNL_ANIMATION_EASE_OUT_CUBIC,
  PNL_ANIMATION_EASE_IN_OUT_CUBIC,

  PNL_ANIMATION_LAST
} PnlAnimationMode;

GType         pnl_animation_mode_get_type (void);
void          pnl_animation_start         (PnlAnimation     *animation);
void          pnl_animation_stop          (PnlAnimation     *animation);
void          pnl_animation_add_property  (PnlAnimation     *animation,
                                           GParamSpec       *pspec,
                                           const GValue     *value);

PnlAnimation *pnl_object_animatev         (gpointer          object,
                                           PnlAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           const gchar      *first_property,
                                           va_list           args);
PnlAnimation* pnl_object_animate          (gpointer          object,
                                           PnlAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           const gchar      *first_property,
                                           ...) G_GNUC_NULL_TERMINATED;
PnlAnimation* pnl_object_animate_full     (gpointer          object,
                                           PnlAnimationMode  mode,
                                           guint             duration_msec,
                                           GdkFrameClock    *frame_clock,
                                           GDestroyNotify    notify,
                                           gpointer          notify_data,
                                           const gchar      *first_property,
                                           ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* PNL_ANIMATION_H */
