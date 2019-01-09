/* ide-environment-editor.h
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
#include <libide-core.h>
#include <libide-threading.h>

G_BEGIN_DECLS

#define IDE_TYPE_ENVIRONMENT_EDITOR (ide_environment_editor_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeEnvironmentEditor, ide_environment_editor, IDE, ENVIRONMENT_EDITOR, GtkListBox)

IDE_AVAILABLE_IN_3_32
GtkWidget      *ide_environment_editor_new             (void);
IDE_AVAILABLE_IN_3_32
IdeEnvironment *ide_environment_editor_get_environment (IdeEnvironmentEditor *self);
IDE_AVAILABLE_IN_3_32
void            ide_environment_editor_set_environment (IdeEnvironmentEditor *self,
                                                        IdeEnvironment       *environment);

G_END_DECLS
