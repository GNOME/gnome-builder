/* ide-source-location.h
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#include "util/ide-uri.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_LOCATION (ide_source_location_get_type())

GType              ide_source_location_get_type        (void);
IdeSourceLocation *ide_source_location_ref             (IdeSourceLocation       *self);
void               ide_source_location_unref           (IdeSourceLocation       *self);
IdeSourceLocation *ide_source_location_new             (IdeFile                 *file,
                                                        guint                    line,
                                                        guint                    line_offset,
                                                        guint                    offset);
IdeSourceLocation *ide_source_location_new_for_path    (IdeContext              *context,
                                                        const gchar             *path,
                                                        guint                    line,
                                                        guint                    line_offset);
guint              ide_source_location_get_line        (IdeSourceLocation       *self);
guint              ide_source_location_get_line_offset (IdeSourceLocation       *self);
guint              ide_source_location_get_offset      (IdeSourceLocation       *self);
IdeFile           *ide_source_location_get_file        (IdeSourceLocation       *self);
IdeUri            *ide_source_location_get_uri         (IdeSourceLocation       *self);
gint               ide_source_location_compare         (const IdeSourceLocation *a,
                                                        const IdeSourceLocation *b);
guint              ide_source_location_hash            (IdeSourceLocation       *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSourceLocation, ide_source_location_unref)

G_END_DECLS
