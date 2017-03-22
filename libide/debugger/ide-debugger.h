/* ide-debugger.h
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

#ifndef IDE_DEBUGGER_H
#define IDE_DEBUGGER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER (ide_debugger_get_type())

G_DECLARE_INTERFACE (IdeDebugger, ide_debugger, IDE, DEBUGGER, IdeObject)

struct _IdeDebuggerInterface
{
  GTypeInterface parent_iface;

  gchar    *(*get_name)        (IdeDebugger *self);
  gboolean  (*supports_runner) (IdeDebugger *self,
                                IdeRunner   *runner,
                                gint        *priority);
};

gchar    *ide_debugger_get_name        (IdeDebugger *self);
gboolean  ide_debugger_supports_runner (IdeDebugger *self,
                                        IdeRunner   *runner,
                                        gint        *priority);

G_END_DECLS

#endif /* IDE_DEBUGGER_H */
