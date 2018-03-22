/* gb-command-manager.h
 *
 * Copyright 2014 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "gb-command.h"
#include "gb-command-provider.h"

G_BEGIN_DECLS

#define GB_TYPE_COMMAND_MANAGER (gb_command_manager_get_type())

G_DECLARE_FINAL_TYPE (GbCommandManager, gb_command_manager, GB, COMMAND_MANAGER, GObject)

GbCommandManager  *gb_command_manager_new          (void);
GbCommand         *gb_command_manager_lookup       (GbCommandManager  *manager,
                                                    const gchar       *command_text);
gchar            **gb_command_manager_complete     (GbCommandManager  *manager,
                                                    const gchar       *initial_command_text);
void               gb_command_manager_add_provider (GbCommandManager  *manager,
                                                    GbCommandProvider *provider);

G_END_DECLS
