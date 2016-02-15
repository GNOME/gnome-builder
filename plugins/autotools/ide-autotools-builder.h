/* ide-autotools-builder.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_AUTOTOOLS_BUILDER_H
#define IDE_AUTOTOOLS_BUILDER_H

#include "ide-builder.h"

G_BEGIN_DECLS

#define IDE_TYPE_AUTOTOOLS_BUILDER (ide_autotools_builder_get_type())

G_DECLARE_FINAL_TYPE (IdeAutotoolsBuilder, ide_autotools_builder, IDE, AUTOTOOLS_BUILDER, IdeBuilder)

GFile    *ide_autotools_builder_get_build_directory (IdeAutotoolsBuilder  *self);
gboolean  ide_autotools_builder_get_needs_bootstrap (IdeAutotoolsBuilder  *self);

G_END_DECLS

#endif /* IDE_AUTOTOOLS_BUILDER_H */
