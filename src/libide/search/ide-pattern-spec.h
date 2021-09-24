/* ide-pattern-spec.h
 *
 * Copyright (C) 2015-2021 Christian Hergert <christian@hergert.me>
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

#pragma once

#include <libide-core.h>

G_BEGIN_DECLS

typedef struct _IdePatternSpec IdePatternSpec;

#define IDE_TYPE_PATTERN_SPEC (ide_pattern_spec_get_type())

IDE_AVAILABLE_IN_ALL
GType           ide_pattern_spec_get_type (void);
IDE_AVAILABLE_IN_ALL
IdePatternSpec *ide_pattern_spec_new      (const gchar    *keywords);
IDE_AVAILABLE_IN_ALL
IdePatternSpec *ide_pattern_spec_ref      (IdePatternSpec *self);
IDE_AVAILABLE_IN_ALL
void            ide_pattern_spec_unref    (IdePatternSpec *self);
IDE_AVAILABLE_IN_ALL
gboolean        ide_pattern_spec_match    (IdePatternSpec *self,
                                           const gchar     *haystack);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_pattern_spec_get_text (IdePatternSpec *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdePatternSpec, ide_pattern_spec_unref)

G_END_DECLS
