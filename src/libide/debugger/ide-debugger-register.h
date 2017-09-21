/* ide-debugger-register.h
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

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_REGISTER (ide_debugger_register_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerRegister, ide_debugger_register, IDE, DEBUGGER_REGISTER, GObject)

struct _IdeDebuggerRegisterClass
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

gint                 ide_debugger_register_compare   (IdeDebuggerRegister *a,
                                                      IdeDebuggerRegister *b);
IdeDebuggerRegister *ide_debugger_register_new       (const gchar *id);
const gchar         *ide_debugger_register_get_id    (IdeDebuggerRegister *self);
const gchar         *ide_debugger_register_get_name  (IdeDebuggerRegister *self);
void                 ide_debugger_register_set_name  (IdeDebuggerRegister *self,
                                                      const gchar         *name);
const gchar         *ide_debugger_register_get_value (IdeDebuggerRegister *self);
void                 ide_debugger_register_set_value (IdeDebuggerRegister *self,
                                                      const gchar         *value);

G_END_DECLS
