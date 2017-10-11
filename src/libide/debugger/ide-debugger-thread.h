/* ide-debugger-thread.h
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

#define IDE_TYPE_DEBUGGER_THREAD (ide_debugger_thread_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerThread, ide_debugger_thread, IDE, DEBUGGER_THREAD, GObject)

struct _IdeDebuggerThreadClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

gint               ide_debugger_thread_compare   (IdeDebuggerThread *a,
                                                  IdeDebuggerThread *b);
IdeDebuggerThread *ide_debugger_thread_new       (const gchar       *id);
const gchar       *ide_debugger_thread_get_id    (IdeDebuggerThread *self);
const gchar       *ide_debugger_thread_get_group (IdeDebuggerThread *self);
void               ide_debugger_thread_set_group (IdeDebuggerThread *self,
                                                  const gchar       *thread_group);

G_END_DECLS
