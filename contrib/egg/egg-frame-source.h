/* egg-frame-source.h
 *
 * Copyright (C) 2010-2015 Christian Hergert <christian@hergert.me>
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

#ifndef EGG_FRAME_SOURCE_H
#define EGG_FRAME_SOURCE_H

#include <glib.h>

G_BEGIN_DECLS

guint egg_frame_source_add (guint       frames_per_sec,
                            GSourceFunc callback,
                            gpointer    user_data);

G_END_DECLS

#endif /* EGG_FRAME_SOURCE_H */
