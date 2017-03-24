/* mi2-breakpoint.h
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#ifndef MI2_BREAKPOINT_H
#define MI2_BREAKPOINT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MI2_TYPE_BREAKPOINT (mi2_breakpoint_get_type())

G_DECLARE_FINAL_TYPE (Mi2Breakpoint, mi2_breakpoint, MI2, BREAKPOINT, GObject)

Mi2Breakpoint *mi2_breakpoint_new             (void);
gint           mi2_breakpoint_get_id          (Mi2Breakpoint *self);
void           mi2_breakpoint_set_id          (Mi2Breakpoint *self,
                                               gint           id);
gint           mi2_breakpoint_get_line_offset (Mi2Breakpoint *self);
void           mi2_breakpoint_set_line_offset (Mi2Breakpoint *self,
                                               gint           line_offset);
const gchar   *mi2_breakpoint_get_address     (Mi2Breakpoint *self);
void           mi2_breakpoint_set_address     (Mi2Breakpoint *self,
                                               const gchar   *address);
const gchar   *mi2_breakpoint_get_linespec    (Mi2Breakpoint *self);
void           mi2_breakpoint_set_linespec    (Mi2Breakpoint *self,
                                               const gchar   *linespec);
const gchar   *mi2_breakpoint_get_filename    (Mi2Breakpoint *self);
void           mi2_breakpoint_set_filename    (Mi2Breakpoint *self,
                                               const gchar   *filename);
const gchar   *mi2_breakpoint_get_function    (Mi2Breakpoint *self);
void           mi2_breakpoint_set_function    (Mi2Breakpoint *self,
                                               const gchar   *function);

G_END_DECLS

#endif /* MI2_BREAKPOINT_H */
