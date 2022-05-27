/* ide-debugger-thread-group.h
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

#define IDE_TYPE_DEBUGGER_THREAD_GROUP (ide_debugger_thread_group_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerThreadGroup, ide_debugger_thread_group, IDE, DEBUGGER_THREAD_GROUP, GObject)

struct _IdeDebuggerThreadGroupClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[4];
};

IDE_AVAILABLE_IN_ALL
gint                    ide_debugger_thread_group_compare       (IdeDebuggerThreadGroup *a,
                                                                 IdeDebuggerThreadGroup *b);
IDE_AVAILABLE_IN_ALL
IdeDebuggerThreadGroup *ide_debugger_thread_group_new           (const gchar            *id);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_thread_group_get_id        (IdeDebuggerThreadGroup *self);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_thread_group_get_pid       (IdeDebuggerThreadGroup *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_thread_group_set_pid       (IdeDebuggerThreadGroup *self,
                                                                 const gchar            *pid);
IDE_AVAILABLE_IN_ALL
const gchar            *ide_debugger_thread_group_get_exit_code (IdeDebuggerThreadGroup *self);
IDE_AVAILABLE_IN_ALL
void                    ide_debugger_thread_group_set_exit_code (IdeDebuggerThreadGroup *self,
                                                                 const gchar            *exit_code);

G_END_DECLS
