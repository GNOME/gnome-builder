/* ide-buffer-addin.h
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

#include "ide-version-macros.h"

#include "buffers/ide-buffer.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER_ADDIN (ide_buffer_addin_get_type())

G_DECLARE_INTERFACE (IdeBufferAddin, ide_buffer_addin, IDE, BUFFER_ADDIN, GObject)

struct _IdeBufferAddinInterface
{
  GTypeInterface parent_iface;

  void (*load)   (IdeBufferAddin    *self,
                  IdeBuffer         *buffer);
  void (*unload) (IdeBufferAddin    *self,
                  IdeBuffer         *buffer);
};

IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_load                (IdeBufferAddin *self,
                                                      IdeBuffer      *buffer);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_unload              (IdeBufferAddin *self,
                                                      IdeBuffer      *buffer);
IDE_AVAILABLE_IN_ALL
IdeBufferAddin *ide_buffer_addin_find_by_module_name (IdeBuffer      *buffer,
                                                      const gchar    *module_name);

G_END_DECLS
