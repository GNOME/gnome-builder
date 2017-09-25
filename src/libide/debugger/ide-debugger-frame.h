/* ide-debugger-frame.h
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

#include "ide-debugger-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DEBUGGER_FRAME (ide_debugger_frame_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeDebuggerFrame, ide_debugger_frame, IDE, DEBUGGER_FRAME, GObject)

struct _IdeDebuggerFrameClass
{
  GObjectClass parent;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IdeDebuggerFrame    *ide_debugger_frame_new          (void);
IdeDebuggerAddress   ide_debugger_frame_get_address  (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_address  (IdeDebuggerFrame    *self,
                                                      IdeDebuggerAddress   address);
const gchar         *ide_debugger_frame_get_file     (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_file     (IdeDebuggerFrame    *self,
                                                      const gchar         *file);
const gchar         *ide_debugger_frame_get_function (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_function (IdeDebuggerFrame    *self,
                                                      const gchar         *function);
const gchar * const *ide_debugger_frame_get_args     (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_args     (IdeDebuggerFrame    *self,
                                                      const gchar * const *args);
const gchar         *ide_debugger_frame_get_library  (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_library  (IdeDebuggerFrame    *self,
                                                      const gchar         *library);
guint                ide_debugger_frame_get_depth    (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_depth    (IdeDebuggerFrame    *self,
                                                      guint                depth);
guint                ide_debugger_frame_get_line     (IdeDebuggerFrame    *self);
void                 ide_debugger_frame_set_line     (IdeDebuggerFrame    *self,
                                                      guint                line);

G_END_DECLS
