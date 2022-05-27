/* ide-debugger-frame.h
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

#define IDE_TYPE_DEBUGGER_FRAME (ide_debugger_frame_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeDebuggerFrame, ide_debugger_frame, IDE, DEBUGGER_FRAME, GObject)

struct _IdeDebuggerFrameClass
{
  GObjectClass parent;

  /*< private >*/
  gpointer _reserved[4];
};

IDE_AVAILABLE_IN_ALL
IdeDebuggerFrame    *ide_debugger_frame_new          (void);
IDE_AVAILABLE_IN_ALL
IdeDebuggerAddress   ide_debugger_frame_get_address  (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_address  (IdeDebuggerFrame    *self,
                                                      IdeDebuggerAddress   address);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_frame_get_file     (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_file     (IdeDebuggerFrame    *self,
                                                      const gchar         *file);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_frame_get_function (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_function (IdeDebuggerFrame    *self,
                                                      const gchar         *function);
IDE_AVAILABLE_IN_ALL
const gchar * const *ide_debugger_frame_get_args     (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_args     (IdeDebuggerFrame    *self,
                                                      const gchar * const *args);
IDE_AVAILABLE_IN_ALL
const gchar         *ide_debugger_frame_get_library  (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_library  (IdeDebuggerFrame    *self,
                                                      const gchar         *library);
IDE_AVAILABLE_IN_ALL
guint                ide_debugger_frame_get_depth    (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_depth    (IdeDebuggerFrame    *self,
                                                      guint                depth);
IDE_AVAILABLE_IN_ALL
guint                ide_debugger_frame_get_line     (IdeDebuggerFrame    *self);
IDE_AVAILABLE_IN_ALL
void                 ide_debugger_frame_set_line     (IdeDebuggerFrame    *self,
                                                      guint                line);

G_END_DECLS
