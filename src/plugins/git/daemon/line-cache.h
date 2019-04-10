/* line-cache.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

typedef struct _LineCache LineCache;

typedef enum
{
  LINE_MARK_ADDED            = 1 << 0,
  LINE_MARK_REMOVED          = 1 << 1,
  LINE_MARK_CHANGED          = 1 << 2,
  LINE_MARK_PREVIOUS_REMOVED = 1 << 3,
} LineMark;

typedef struct
{
  guint line : 28;
  guint mark : 4;
} LineEntry;

LineCache *line_cache_new              (void);
LineCache *line_cache_new_from_variant (GVariant        *changes);
void       line_cache_free             (LineCache       *self);
void       line_cache_mark_range       (LineCache       *self,
                                        gint             start_line,
                                        gint             end_line,
                                        LineMark         mark);
void       line_cache_foreach_in_range (const LineCache *self,
                                        gint             start_line,
                                        gint             end_line,
                                        GFunc            callback,
                                        gpointer         user_data);
LineMark   line_cache_get_mark         (const LineCache *self,
                                        gint             line);
GVariant  *line_cache_to_variant       (const LineCache *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (LineCache, line_cache_free)

G_END_DECLS
