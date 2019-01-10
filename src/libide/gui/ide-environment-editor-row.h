/* ide-environment-editor-row.h
 *
 * Copyright 2016-2019 Christian Hergert <christian@hergert.me>
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

#include <gtk/gtk.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT_EDITOR_ROW (ide_environment_editor_row_get_type())

G_DECLARE_FINAL_TYPE (IdeEnvironmentEditorRow, ide_environment_editor_row, IDE, ENVIRONMENT_EDITOR_ROW, GtkListBoxRow)

IdeEnvironmentVariable *ide_environment_editor_row_get_variable  (IdeEnvironmentEditorRow *self);
void                    ide_environment_editor_row_set_variable  (IdeEnvironmentEditorRow *self,
                                                                  IdeEnvironmentVariable  *variable);
void                    ide_environment_editor_row_start_editing (IdeEnvironmentEditorRow *self);

G_END_DECLS
