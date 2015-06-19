/* ide-symbol.h
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

#ifndef IDE_SYMBOL_H
#define IDE_SYMBOL_H

#include "ide-types.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL (ide_symbol_get_type())

typedef enum
{
  IDE_SYMBOL_NONE,
  IDE_SYMBOL_SCALAR,
  IDE_SYMBOL_CLASS,
  IDE_SYMBOL_FUNCTION,
  IDE_SYMBOL_METHOD,
  IDE_SYMBOL_STRUCT,
  IDE_SYMBOL_UNION,
  IDE_SYMBOL_FIELD,
  IDE_SYMBOL_ENUM,
  IDE_SYMBOL_ENUM_VALUE,
  IDE_SYMBOL_VARIABLE,
} IdeSymbolKind;

typedef enum
{
  IDE_SYMBOL_FLAGS_NONE          = 0,
  IDE_SYMBOL_FLAGS_IS_STATIC     = 1 << 0,
  IDE_sYMBOL_FLAGS_IS_MEMBER     = 1 << 1,
  IDE_SYMBOL_FLAGS_IS_DEPRECATED = 1 << 2,
} IdeSymbolFlags;

GType              ide_symbol_get_type                 (void);
IdeSymbol         *ide_symbol_ref                      (IdeSymbol *self);
void               ide_symbol_unref                    (IdeSymbol *self);
IdeSymbolKind      ide_symbol_get_kind                 (IdeSymbol *self);
IdeSymbolFlags     ide_symbol_get_flags                (IdeSymbol *self);
const gchar       *ide_symbol_get_name                 (IdeSymbol *self);
IdeSourceLocation *ide_symbol_get_canonical_location   (IdeSymbol *self);
IdeSourceLocation *ide_symbol_get_declaration_location (IdeSymbol *self);
IdeSourceLocation *ide_symbol_get_definition_location  (IdeSymbol *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSymbol, ide_symbol_unref)

G_END_DECLS

#endif /* IDE_SYMBOL_H */
