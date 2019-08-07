/* ide-command.h
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#if !defined (IDE_GUI_INSIDE) && !defined (IDE_GUI_COMPILATION)
# error "Only <libide-gui.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_COMMAND (ide_command_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_INTERFACE (IdeCommand, ide_command, IDE, COMMAND, IdeObject)

struct _IdeCommandInterface
{
  GTypeInterface parent_iface;

  gchar    *(*get_title)     (IdeCommand           *self);
  gchar    *(*get_subtitle)  (IdeCommand           *self);
  void      (*run_async)     (IdeCommand           *self,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);
  gboolean  (*run_finish)    (IdeCommand           *self,
                              GAsyncResult         *result,
                              GError              **error);
  gint      (*get_priority)  (IdeCommand           *self);
  GIcon    *(*get_icon)      (IdeCommand           *self);
};

IDE_AVAILABLE_IN_3_34
GIcon    *ide_command_get_icon     (IdeCommand           *self);
IDE_AVAILABLE_IN_3_34
gint      ide_command_get_priority (IdeCommand           *self);
IDE_AVAILABLE_IN_3_32
gchar    *ide_command_get_title    (IdeCommand           *self);
IDE_AVAILABLE_IN_3_32
gchar    *ide_command_get_subtitle (IdeCommand           *self);
IDE_AVAILABLE_IN_3_32
void      ide_command_run_async    (IdeCommand           *self,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data);
IDE_AVAILABLE_IN_3_32
gboolean  ide_command_run_finish   (IdeCommand           *self,
                                    GAsyncResult         *result,
                                    GError              **error);

G_END_DECLS
