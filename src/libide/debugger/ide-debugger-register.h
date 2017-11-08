/* ide-debugger-register.h
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

#include <glib-object.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_REGISTER (ide_debugger_register_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerRegister, ide_debugger_register, IDE, DEBUGGER_REGISTER, GObject)

struct _IdeDebuggerRegisterClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IDE_AVAILABLE_IN_ALL
gint                 ide_debugger_register_compare   (IdeDebuggerRegister *a,
                                                      IdeDebuggerRegister *b);
IDE_AVAILABLE_IN_ALL
IdeDebuggerRegister *ide_debugger_register_new       (const gchar *id);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_register_get_id    (IdeDebuggerRegister *self);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_register_get_name  (IdeDebuggerRegister *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_register_set_name  (IdeDebuggerRegister *self,
                                                      const gchar         *name);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_register_get_value (IdeDebuggerRegister *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_register_set_value (IdeDebuggerRegister *self,
                                                      const gchar         *value);

G_END_DECLS
