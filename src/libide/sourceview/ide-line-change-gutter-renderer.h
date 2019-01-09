/* ide-line-change-gutter-renderer.h
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

#include <gtksourceview/gtksource.h>
#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER (ide_line_change_gutter_renderer_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeLineChangeGutterRenderer, ide_line_change_gutter_renderer, IDE, LINE_CHANGE_GUTTER_RENDERER, GtkSourceGutterRenderer)

G_END_DECLS
