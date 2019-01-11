/* ide-diagnostics-manager.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-code-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_DIAGNOSTICS_MANAGER (ide_diagnostics_manager_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeDiagnosticsManager, ide_diagnostics_manager, IDE, DIAGNOSTICS_MANAGER, IdeObject)

IDE_AVAILABLE_IN_3_32
IdeDiagnosticsManager *ide_diagnostics_manager_from_context             (IdeContext            *context);
IDE_AVAILABLE_IN_3_32
gboolean               ide_diagnostics_manager_get_busy                 (IdeDiagnosticsManager *self);
IDE_AVAILABLE_IN_3_32
IdeDiagnostics        *ide_diagnostics_manager_get_diagnostics_for_file (IdeDiagnosticsManager *self,
                                                                         GFile                 *file);
IDE_AVAILABLE_IN_3_32
guint                  ide_diagnostics_manager_get_sequence_for_file    (IdeDiagnosticsManager *self,
                                                                         GFile                 *file);
IDE_AVAILABLE_IN_3_32
void                   ide_diagnostics_manager_rediagnose               (IdeDiagnosticsManager *self,
                                                                         IdeBuffer             *buffer);

G_END_DECLS
