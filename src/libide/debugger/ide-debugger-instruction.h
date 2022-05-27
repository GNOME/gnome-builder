/* ide-debugger-instruction.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-core.h>

#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_INSTRUCTION (ide_debugger_instruction_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerInstruction, ide_debugger_instruction, IDE, DEBUGGER_INSTRUCTION, GObject)

struct _IdeDebuggerInstructionClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[4];
};

IDE_AVAILABLE_IN_ALL
IdeDebuggerInstruction *ide_debugger_instruction_new          (IdeDebuggerAddress      address);
IDE_AVAILABLE_IN_ALL
IdeDebuggerAddress      ide_debugger_instruction_get_address  (IdeDebuggerInstruction *self);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_instruction_get_function (IdeDebuggerInstruction *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_instruction_set_function (IdeDebuggerInstruction *self,
                                                               const gchar            *function);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_instruction_get_display  (IdeDebuggerInstruction *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_instruction_set_display  (IdeDebuggerInstruction *self,
                                                               const gchar            *display);

G_END_DECLS
