/* ide-breakpoint.h
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

#ifndef IDE_BREAKPOINT_H
#define IDE_BREAKPOINT_H

#include <gio/gio.h>

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BREAKPOINT (ide_breakpoint_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBreakpoint, ide_breakpoint, IDE, BREAKPOINT, GObject)

struct _IdeBreakpointClass
{
  GObjectClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeBreakpoint *ide_breakpoint_new             (void);
const gchar   *ide_breakpoint_get_id          (IdeBreakpoint *self);
void           ide_breakpoint_set_id          (IdeBreakpoint *self,
                                               const gchar   *id);
const gchar   *ide_breakpoint_get_address     (IdeBreakpoint *self);
void           ide_breakpoint_set_address     (IdeBreakpoint *self,
                                               const gchar   *address);
GFile         *ide_breakpoint_get_file        (IdeBreakpoint *self);
void           ide_breakpoint_set_file        (IdeBreakpoint *self,
                                               GFile         *file);
guint          ide_breakpoint_get_line        (IdeBreakpoint *self);
void           ide_breakpoint_set_line        (IdeBreakpoint *self,
                                               guint          line);
guint          ide_breakpoint_get_line_offset (IdeBreakpoint *self);
void           ide_breakpoint_set_line_offset (IdeBreakpoint *self,
                                               guint          line);
gboolean       ide_breakpoint_get_enabled     (IdeBreakpoint *self);
void           ide_breakpoint_set_enabled     (IdeBreakpoint *self,
                                               gboolean       enabled);
gboolean       ide_breakpoint_get_transient   (IdeBreakpoint *self);
void           ide_breakpoint_set_transient   (IdeBreakpoint *self,
                                               gboolean       transient);

G_END_DECLS

#endif /* IDE_BREAKPOINT_H */
