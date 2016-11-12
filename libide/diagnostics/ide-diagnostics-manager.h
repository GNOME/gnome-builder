/* ide-diagnostics-manager.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_DIAGNOSTICS_MANAGER_H
#define IDE_DIAGNOSTICS_MANAGER_H

#include <gio/gio.h>

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICS_MANAGER (ide_diagnostics_manager_get_type())

G_DECLARE_FINAL_TYPE (IdeDiagnosticsManager, ide_diagnostics_manager, IDE, DIAGNOSTICS_MANAGER, IdeObject)

gboolean        ide_diagnostics_manager_get_busy                 (IdeDiagnosticsManager *self);
IdeDiagnostics *ide_diagnostics_manager_get_diagnostics_for_file (IdeDiagnosticsManager *self,
                                                                  GFile                 *file);
guint           ide_diagnostics_manager_get_sequence_for_file    (IdeDiagnosticsManager *self,
                                                                  GFile                 *file);
void            ide_diagnostics_manager_update_group_by_file     (IdeDiagnosticsManager *self,
                                                                  IdeBuffer             *buffer,
                                                                  GFile                 *new_file);

G_END_DECLS

#endif /* IDE_DIAGNOSTICS_MANAGER_H */
