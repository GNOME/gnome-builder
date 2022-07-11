/* ide-unsaved-file.h
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

IDE_AVAILABLE_IN_ALL
GType           ide_unsaved_file_get_type      (void);
IDE_AVAILABLE_IN_ALL
IdeUnsavedFile *ide_unsaved_file_ref           (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
void            ide_unsaved_file_unref         (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
GBytes         *ide_unsaved_file_get_content   (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
GFile          *ide_unsaved_file_get_file      (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
gint64          ide_unsaved_file_get_sequence  (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_unsaved_file_get_temp_path (IdeUnsavedFile  *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_unsaved_file_persist       (IdeUnsavedFile  *self,
                                                GCancellable    *cancellable,
                                                GError         **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeUnsavedFile, ide_unsaved_file_unref)

G_END_DECLS
