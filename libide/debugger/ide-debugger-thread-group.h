/* ide-debugger-thread-group.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_THREAD_GROUP (ide_debugger_thread_group_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerThreadGroup, ide_debugger_thread_group, IDE, DEBUGGER_THREAD_GROUP, GObject)

struct _IdeDebuggerThreadGroupClass
{
  GObjectClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

gint                    ide_debugger_thread_group_compare       (IdeDebuggerThreadGroup *a,
                                                                 IdeDebuggerThreadGroup *b);
IdeDebuggerThreadGroup *ide_debugger_thread_group_new           (const gchar *id);
const gchar            *ide_debugger_thread_group_get_id        (IdeDebuggerThreadGroup *self);
const gchar            *ide_debugger_thread_group_get_pid       (IdeDebuggerThreadGroup *self);
void                    ide_debugger_thread_group_set_pid       (IdeDebuggerThreadGroup *self,
                                                                 const gchar            *pid);
const gchar            *ide_debugger_thread_group_get_exit_code (IdeDebuggerThreadGroup *self);
void                    ide_debugger_thread_group_set_exit_code (IdeDebuggerThreadGroup *self,
                                                                 const gchar            *exit_code);

G_END_DECLS
