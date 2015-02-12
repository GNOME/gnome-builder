/* ide-symbol.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_SYMBOL_H
#define IDE_SYMBOL_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL (ide_symbol_get_type())

GType        ide_symbol_get_type (void);
IdeSymbol   *ide_symbol_ref      (IdeSymbol *self);
void         ide_symbol_unref    (IdeSymbol *self);
const gchar *ide_symbol_get_name (IdeSymbol *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSymbol, ide_symbol_unref)

G_END_DECLS

#endif /* IDE_SYMBOL_H */
