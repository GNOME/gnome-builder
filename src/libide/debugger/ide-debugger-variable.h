/* ide-debugger-variable.h
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

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_VARIABLE (ide_debugger_variable_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerVariable, ide_debugger_variable, IDE, DEBUGGER_VARIABLE, GObject)

struct _IdeDebuggerVariableClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeDebuggerVariable *ide_debugger_variable_new              (const gchar         *name);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_variable_get_name         (IdeDebuggerVariable *self);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_variable_get_type_name    (IdeDebuggerVariable *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_variable_set_type_name    (IdeDebuggerVariable *self,
                                                             const gchar         *type_name);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_variable_get_value        (IdeDebuggerVariable *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_variable_set_value        (IdeDebuggerVariable *self,
                                                             const gchar         *value);
IDE_AVAILABLE_IN_ALL
gboolean             ide_debugger_variable_get_has_children (IdeDebuggerVariable *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_variable_set_has_children (IdeDebuggerVariable *self,
                                                             gboolean             has_children);

G_END_DECLS
