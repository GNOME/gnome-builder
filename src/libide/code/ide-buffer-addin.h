/* ide-buffer-addin.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUFFER_ADDIN (ide_buffer_addin_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeBufferAddin, ide_buffer_addin, IDE, BUFFER_ADDIN, GObject)

struct _IdeBufferAddinInterface
{
  GTypeInterface parent_iface;

  void     (*load)                 (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer);
  void     (*unload)               (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer);
  void     (*file_loaded)          (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer,
                                    GFile                *file);
  void     (*save_file)            (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer,
                                    GFile                *file);
  void     (*file_saved)           (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer,
                                    GFile                *file);
  void     (*language_set)         (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer,
                                    const gchar          *language_id);
  void     (*change_settled)       (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer);
  void     (*style_scheme_changed) (IdeBufferAddin       *self,
                                    IdeBuffer            *buffer);
  void     (*settle_async)         (IdeBufferAddin       *self,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data);
  gboolean (*settle_finish)        (IdeBufferAddin       *self,
                                    GAsyncResult         *result,
                                    GError              **error);
};

IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_load                 (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_unload               (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_file_loaded          (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer,
                                                       GFile                *file);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_save_file            (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer,
                                                       GFile                *file);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_file_saved           (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer,
                                                       GFile                *file);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_language_set         (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer,
                                                       const gchar          *language_id);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_change_settled       (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_style_scheme_changed (IdeBufferAddin       *self,
                                                       IdeBuffer            *buffer);
IDE_AVAILABLE_IN_ALL
void            ide_buffer_addin_settle_async         (IdeBufferAddin       *self,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);
IDE_AVAILABLE_IN_ALL
gboolean        ide_buffer_addin_settle_finish        (IdeBufferAddin       *self,
                                                       GAsyncResult         *result,
                                                       GError              **error);
IDE_AVAILABLE_IN_ALL
IdeBufferAddin *ide_buffer_addin_find_by_module_name  (IdeBuffer            *buffer,
                                                       const gchar          *module_name);

G_END_DECLS
