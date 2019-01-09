/* ide-source-range.h
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

#include "ide-version-macros.h"

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_RANGE (ide_source_range_get_type())

IDE_AVAILABLE_IN_3_32
GType              ide_source_range_get_type        (void);
IDE_AVAILABLE_IN_3_32
IdeSourceRange    *ide_source_range_ref              (IdeSourceRange       *self);
IDE_AVAILABLE_IN_3_32
void               ide_source_range_unref            (IdeSourceRange       *self);
IDE_AVAILABLE_IN_3_32
IdeSourceLocation *ide_source_range_get_begin        (IdeSourceRange       *self);
IDE_AVAILABLE_IN_3_32
IdeSourceLocation *ide_source_range_get_end          (IdeSourceRange       *self);
IDE_AVAILABLE_IN_3_32
IdeSourceRange    *ide_source_range_new              (IdeSourceLocation    *begin,
                                                      IdeSourceLocation    *end);
IDE_AVAILABLE_IN_3_32
IdeSourceRange    *ide_source_range_new_from_variant (GVariant             *variant);
IDE_AVAILABLE_IN_3_32
GVariant          *ide_source_range_to_variant       (const IdeSourceRange *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSourceRange, ide_source_range_unref)

G_END_DECLS
