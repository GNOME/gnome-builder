/* ide-run-commands.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_FOUNDRY_INSIDE) && !defined (IDE_FOUNDRY_COMPILATION)
# error "Only <libide-foundry.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-run-command.h"

G_BEGIN_DECLS

#define IDE_TYPE_RUN_COMMANDS (ide_run_commands_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeRunCommands, ide_run_commands, IDE, RUN_COMMANDS, IdeObject)

IDE_AVAILABLE_IN_ALL
IdeRunCommands *ide_run_commands_from_context (IdeContext        *context);
IDE_AVAILABLE_IN_ALL
GListModel     *ide_run_commands_list_by_kind (IdeRunCommands    *self,
                                               IdeRunCommandKind  kind);
IDE_AVAILABLE_IN_ALL
IdeRunCommand  *ide_run_commands_dup_by_id    (IdeRunCommands    *self,
                                               const char        *id);

G_END_DECLS
