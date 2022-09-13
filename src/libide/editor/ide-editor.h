/* ide-editor.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
#include <libide-code.h>
#include <libide-gui.h>

G_BEGIN_DECLS

IDE_AVAILABLE_IN_ALL
void ide_editor_focus_location (IdeWorkspace  *workspace,
                                PanelPosition *position,
                                IdeLocation   *location);
IDE_AVAILABLE_IN_ALL
void ide_editor_focus_buffer   (IdeWorkspace  *workspace,
                                PanelPosition *position,
                                IdeBuffer     *buffer);

G_END_DECLS
