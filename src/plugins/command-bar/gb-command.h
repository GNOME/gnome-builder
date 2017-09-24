/* gb-command.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib-object.h>

#include "gb-command-result.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND            (gb_command_get_type())

G_DECLARE_DERIVABLE_TYPE (GbCommand, gb_command, GB, COMMAND, GObject)

struct _GbCommandClass
{
  GObjectClass parent;

  GbCommandResult *(*execute) (GbCommand *command);
};

GbCommand       *gb_command_new      (void);
GbCommandResult *gb_command_execute  (GbCommand *command);

G_END_DECLS
