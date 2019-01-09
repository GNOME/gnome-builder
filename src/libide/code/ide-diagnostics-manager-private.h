/* ide-diagnostics-manager-private.h
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

#include "ide-buffer.h"
#include "ide-diagnostics-manager.h"

G_BEGIN_DECLS

void _ide_diagnostics_manager_file_opened      (IdeDiagnosticsManager *self,
                                                GFile                 *file,
                                                const gchar           *lang_id);
void _ide_diagnostics_manager_file_closed      (IdeDiagnosticsManager *self,
                                                GFile                 *file);
void _ide_diagnostics_manager_language_changed (IdeDiagnosticsManager *self,
                                                GFile                 *file,
                                                const gchar           *lang_id);
void _ide_diagnostics_manager_file_changed     (IdeDiagnosticsManager *self,
                                                GFile                 *file,
                                                GBytes                *contents,
                                                const gchar           *lang_id);

G_END_DECLS
