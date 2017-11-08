/* ide-diagnostics-manager.h
 *
 * Copyright © 2016 Christian Hergert <chergert@redhat.com>
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

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICS_MANAGER (ide_diagnostics_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeDiagnosticsManager, ide_diagnostics_manager, IDE, DIAGNOSTICS_MANAGER, IdeObject)

IDE_AVAILABLE_IN_ALL
gboolean        ide_diagnostics_manager_get_busy                 (IdeDiagnosticsManager *self);
IDE_AVAILABLE_IN_ALL
IdeDiagnostics *ide_diagnostics_manager_get_diagnostics_for_file (IdeDiagnosticsManager *self,
                                                                  GFile                 *file);
IDE_AVAILABLE_IN_ALL
guint           ide_diagnostics_manager_get_sequence_for_file    (IdeDiagnosticsManager *self,
                                                                  GFile                 *file);
IDE_AVAILABLE_IN_ALL
void            ide_diagnostics_manager_update_group_by_file     (IdeDiagnosticsManager *self,
                                                                  IdeBuffer             *buffer,
                                                                  GFile                 *new_file);

G_END_DECLS
