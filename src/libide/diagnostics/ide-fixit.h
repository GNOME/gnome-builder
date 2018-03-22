/* ide-fixit.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "ide-types.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_FIXIT (ide_fixit_get_type())

IDE_AVAILABLE_IN_ALL
IdeFixit       *ide_fixit_new       (IdeSourceRange *source_range,
                                     const gchar    *replacement_text);
IDE_AVAILABLE_IN_ALL
GType           ide_fixit_get_type  (void);
IDE_AVAILABLE_IN_ALL
IdeFixit       *ide_fixit_ref       (IdeFixit       *self);
IDE_AVAILABLE_IN_ALL
void            ide_fixit_unref     (IdeFixit       *self);
IDE_AVAILABLE_IN_ALL
void            ide_fixit_apply     (IdeFixit       *self);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_fixit_get_text  (IdeFixit       *self);
IDE_AVAILABLE_IN_ALL
IdeSourceRange *ide_fixit_get_range (IdeFixit       *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeFixit, ide_fixit_unref)

G_END_DECLS
