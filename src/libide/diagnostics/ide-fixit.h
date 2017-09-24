/* ide-fixit.h
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

#ifndef IDE_FIXIT_H
#define IDE_FIXIT_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_FIXIT (ide_fixit_get_type())

IdeFixit       *ide_fixit_new       (IdeSourceRange *source_range,
                                     const gchar    *replacement_text);
GType           ide_fixit_get_type  (void);
IdeFixit       *ide_fixit_ref       (IdeFixit       *self);
void            ide_fixit_unref     (IdeFixit       *self);
void            ide_fixit_apply     (IdeFixit       *self);
const gchar    *ide_fixit_get_text  (IdeFixit       *self);
IdeSourceRange *ide_fixit_get_range (IdeFixit       *self);

G_END_DECLS

#endif /* IDE_FIXIT_H */
