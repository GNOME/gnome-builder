/* ide-path.h
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#include "ide-path-element.h"

G_BEGIN_DECLS

#define IDE_TYPE_PATH (ide_path_get_type())

IDE_AVAILABLE_IN_3_34
G_DECLARE_FINAL_TYPE (IdePath, ide_path, IDE, PATH, GObject)

IDE_AVAILABLE_IN_3_34
IdePath        *ide_path_new            (GPtrArray *elements);
IDE_AVAILABLE_IN_3_34
IdePath        *ide_path_get_parent     (IdePath   *self);
IDE_AVAILABLE_IN_3_34
guint           ide_path_get_n_elements (IdePath   *self);
IDE_AVAILABLE_IN_3_34
IdePathElement *ide_path_get_element    (IdePath   *self,
                                         guint      position);
IDE_AVAILABLE_IN_3_34
gboolean        ide_path_has_prefix     (IdePath   *self,
                                         IdePath   *prefix);
IDE_AVAILABLE_IN_3_34
gboolean        ide_path_is_root        (IdePath   *self);

G_END_DECLS
