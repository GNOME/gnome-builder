/* ide-compile-commands.h
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

G_BEGIN_DECLS

#define IDE_TYPE_COMPILE_COMMANDS (ide_compile_commands_get_type())

G_DECLARE_FINAL_TYPE (IdeCompileCommands, ide_compile_commands, IDE, COMPILE_COMMANDS, GObject)

IdeCompileCommands  *ide_compile_commands_new         (void);
gboolean             ide_compile_commands_load        (IdeCompileCommands   *self,
                                                       GFile                *file,
                                                       GCancellable         *cancellable,
                                                       GError              **error);
void                 ide_compile_commands_load_async  (IdeCompileCommands   *self,
                                                       GFile                *file,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
gboolean             ide_compile_commands_load_finish (IdeCompileCommands   *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
gchar              **ide_compile_commands_lookup      (IdeCompileCommands   *self,
                                                       GFile                *file,
                                                       GFile               **directory,
                                                       GError              **error);

G_END_DECLS
