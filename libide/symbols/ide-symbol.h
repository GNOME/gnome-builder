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
  IDE_SYMBOL_ALIAS,
  IDE_SYMBOL_ARRAY,
  IDE_SYMBOL_BOOLEAN,
  IDE_SYMBOL_CLASS,
  IDE_SYMBOL_CONSTANT,
  IDE_SYMBOL_CONSTRUCTOR,
  IDE_SYMBOL_ENUM,
  IDE_SYMBOL_ENUM_VALUE,
  IDE_SYMBOL_FIELD,
  IDE_SYMBOL_FILE,
  IDE_SYMBOL_FUNCTION,
  IDE_SYMBOL_HEADER,
  IDE_SYMBOL_INTERFACE,
  IDE_SYMBOL_MACRO,
  IDE_SYMBOL_METHOD,
  IDE_SYMBOL_MODULE,
  IDE_SYMBOL_NAMESPACE,
  IDE_SYMBOL_NUMBER,
  IDE_SYMBOL_PACKAGE,
  IDE_SYMBOL_PROPERTY,
  IDE_SYMBOL_SCALAR,
  IDE_SYMBOL_STRING,
  IDE_SYMBOL_STRUCT,
  IDE_SYMBOL_TEMPLATE,
  IDE_SYMBOL_UNION,
  IDE_SYMBOL_VARIABLE,
  IDE_SYMBOL_KEYWORD,
  IDE_SYMBOL_UI_ATTRIBUTES,
  IDE_SYMBOL_UI_CHILD,
  IDE_SYMBOL_UI_ITEM,
  IDE_SYMBOL_UI_MENU,
  IDE_SYMBOL_UI_MENU_ATTRIBUTE,
  IDE_SYMBOL_UI_OBJECT,
  IDE_SYMBOL_UI_PACKING,
  IDE_SYMBOL_UI_PROPERTY,
  IDE_SYMBOL_UI_SECTION,
  IDE_SYMBOL_UI_SIGNAL,
  IDE_SYMBOL_UI_STYLE,
  IDE_SYMBOL_UI_STYLE_CLASS,
  IDE_SYMBOL_UI_SUBMENU,
  IDE_SYMBOL_UI_TEMPLATE,
  IDE_SYMBOL_XML_ATTRIBUTE,
  IDE_SYMBOL_XML_DECLARATION,
  IDE_SYMBOL_XML_ELEMENT,
  IDE_SYMBOL_XML_COMMENT,
  IDE_SYMBOL_XML_CDATA,
} IdeSymbolKind;

typedef enum
{
  IDE_SYMBOL_FLAGS_NONE          = 0,
  IDE_SYMBOL_FLAGS_IS_STATIC     = 1 << 0,
  IDE_SYMBOL_FLAGS_IS_MEMBER     = 1 << 1,
  IDE_SYMBOL_FLAGS_IS_DEPRECATED = 1 << 2,
  IDE_SYMBOL_FLAGS_IS_DEFINITION = 1 << 3
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
IdeSymbol         *ide_symbol_new                      (const gchar           *name,
                                                        IdeSymbolKind          kind,
                                                        IdeSymbolFlags         flags,
                                                        IdeSourceLocation     *declaration_location,
                                                        IdeSourceLocation     *definition_location,
                                                        IdeSourceLocation     *canonical_location);
const gchar       *ide_symbol_kind_get_icon_name       (IdeSymbolKind          kind);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeSymbol, ide_symbol_unref)

G_END_DECLS

#endif /* IDE_SYMBOL_H */
