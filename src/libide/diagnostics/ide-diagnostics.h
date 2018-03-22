/* ide-diagnostics.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICS (ide_diagnostics_get_type())

IDE_AVAILABLE_IN_ALL
GType           ide_diagnostics_get_type (void);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostics_ref      (IdeDiagnostics *self);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_unref    (IdeDiagnostics *self);
IDE_AVAILABLE_IN_ALL
gsize           ide_diagnostics_get_size (IdeDiagnostics *self);
IDE_AVAILABLE_IN_ALL
IdeDiagnostic  *ide_diagnostics_index    (IdeDiagnostics *self,
                                          gsize           index);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_merge    (IdeDiagnostics *self,
                                          IdeDiagnostics *other);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostics_new      (GPtrArray      *ar);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_add      (IdeDiagnostics *self,
                                          IdeDiagnostic  *diagnostic);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeDiagnostics, ide_diagnostics_unref)

G_END_DECLS
