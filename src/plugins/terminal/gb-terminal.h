/* gb-terminal.h
 *
 * Copyright © 2016 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <vte/vte.h>

G_BEGIN_DECLS

#define GB_TYPE_TERMINAL            (gb_terminal_get_type())
#define GB_TERMINAL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TERMINAL, GbTerminal))
#define GB_TERMINAL_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_TERMINAL, GbTerminal const))
#define GB_TERMINAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_TERMINAL, GbTerminalClass))
#define GB_IS_TERMINAL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_TERMINAL))
#define GB_IS_TERMINAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_TERMINAL))
#define GB_TERMINAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_TERMINAL, GbTerminalClass))

typedef struct _GbTerminal      GbTerminal;
typedef struct _GbTerminalClass GbTerminalClass;

GType gb_terminal_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GbTerminal, g_object_unref)

G_END_DECLS
