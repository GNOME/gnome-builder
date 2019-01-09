/* ide-source-view-private.h
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-source-view.h"

G_BEGIN_DECLS

void         _ide_source_view_init_shortcuts  (IdeSourceView *self);
const gchar *_ide_source_view_get_mode_name   (IdeSourceView *self);
void         _ide_source_view_set_count       (IdeSourceView *self,
                                               gint           count);
void         _ide_source_view_set_modifier    (IdeSourceView *self,
                                               gunichar       modifier);
GtkTextMark *_ide_source_view_get_scroll_mark (IdeSourceView *self);

G_END_DECLS
