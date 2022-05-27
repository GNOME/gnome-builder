/* ide-debugger-library.h
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

#define IDE_TYPE_DEBUGGER_LIBRARY (ide_debugger_library_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerLibrary, ide_debugger_library, IDE, DEBUGGER_LIBRARY, GObject)

struct _IdeDebuggerLibraryClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
gint                ide_debugger_library_compare         (IdeDebuggerLibrary            *a,
                                                          IdeDebuggerLibrary            *b);
IDE_AVAILABLE_IN_ALL
IdeDebuggerLibrary *ide_debugger_library_new             (const gchar                   *id);
IDE_AVAILABLE_IN_ALL
const gchar        *ide_debugger_library_get_id          (IdeDebuggerLibrary            *self);
IDE_AVAILABLE_IN_ALL
GPtrArray          *ide_debugger_library_get_ranges      (IdeDebuggerLibrary            *self);
IDE_AVAILABLE_IN_ALL
void                ide_debugger_library_add_range       (IdeDebuggerLibrary            *self,
                                                          const IdeDebuggerAddressRange *range);
IDE_AVAILABLE_IN_ALL
const gchar        *ide_debugger_library_get_host_name   (IdeDebuggerLibrary            *self);
IDE_AVAILABLE_IN_ALL
void                ide_debugger_library_set_host_name   (IdeDebuggerLibrary            *self,
                                                          const gchar                   *host_name);
IDE_AVAILABLE_IN_ALL
const gchar        *ide_debugger_library_get_target_name (IdeDebuggerLibrary            *self);
IDE_AVAILABLE_IN_ALL
void                ide_debugger_library_set_target_name (IdeDebuggerLibrary            *self,
                                                          const gchar                   *target_name);

G_END_DECLS
