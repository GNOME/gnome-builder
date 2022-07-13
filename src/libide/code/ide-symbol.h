/* ide-symbol.h
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

#if !defined (IDE_CODE_INSIDE) && !defined (IDE_CODE_COMPILATION)
# error "Only <libide-code.h> can be included directly."
#endif

#include <libide-core.h>

#include "ide-code-types.h"

G_BEGIN_DECLS

typedef enum
{
  IDE_SYMBOL_KIND_NONE,
  IDE_SYMBOL_KIND_ALIAS,
  IDE_SYMBOL_KIND_ARRAY,
  IDE_SYMBOL_KIND_BOOLEAN,
  IDE_SYMBOL_KIND_CLASS,
  IDE_SYMBOL_KIND_CONSTANT,
  IDE_SYMBOL_KIND_CONSTRUCTOR,
  IDE_SYMBOL_KIND_ENUM,
  IDE_SYMBOL_KIND_ENUM_VALUE,
  IDE_SYMBOL_KIND_FIELD,
  IDE_SYMBOL_KIND_FILE,
  IDE_SYMBOL_KIND_FUNCTION,
  IDE_SYMBOL_KIND_HEADER,
  IDE_SYMBOL_KIND_INTERFACE,
  IDE_SYMBOL_KIND_MACRO,
  IDE_SYMBOL_KIND_METHOD,
  IDE_SYMBOL_KIND_MODULE,
  IDE_SYMBOL_KIND_NAMESPACE,
  IDE_SYMBOL_KIND_NUMBER,
  IDE_SYMBOL_KIND_PACKAGE,
  IDE_SYMBOL_KIND_PROPERTY,
  IDE_SYMBOL_KIND_SCALAR,
  IDE_SYMBOL_KIND_STRING,
  IDE_SYMBOL_KIND_STRUCT,
  IDE_SYMBOL_KIND_TEMPLATE,
  IDE_SYMBOL_KIND_UNION,
  IDE_SYMBOL_KIND_VARIABLE,
  IDE_SYMBOL_KIND_KEYWORD,
  IDE_SYMBOL_KIND_UI_ATTRIBUTES,
  IDE_SYMBOL_KIND_UI_CHILD,
  IDE_SYMBOL_KIND_UI_ITEM,
  IDE_SYMBOL_KIND_UI_MENU,
  IDE_SYMBOL_KIND_UI_MENU_ATTRIBUTE,
  IDE_SYMBOL_KIND_UI_OBJECT,
  IDE_SYMBOL_KIND_UI_PACKING,
  IDE_SYMBOL_KIND_UI_PROPERTY,
  IDE_SYMBOL_KIND_UI_SECTION,
  IDE_SYMBOL_KIND_UI_SIGNAL,
  IDE_SYMBOL_KIND_UI_STYLE,
  IDE_SYMBOL_KIND_UI_STYLE_CLASS,
  IDE_SYMBOL_KIND_UI_SUBMENU,
  IDE_SYMBOL_KIND_UI_TEMPLATE,
  IDE_SYMBOL_KIND_XML_ATTRIBUTE,
  IDE_SYMBOL_KIND_XML_DECLARATION,
  IDE_SYMBOL_KIND_XML_ELEMENT,
  IDE_SYMBOL_KIND_XML_COMMENT,
  IDE_SYMBOL_KIND_XML_CDATA,

  IDE_SYMBOL_KIND_OBJECT,
  IDE_SYMBOL_KIND_EVENT,
  IDE_SYMBOL_KIND_OPERATOR,
  IDE_SYMBOL_KIND_TYPE_PARAM,

  IDE_SYMBOL_KIND_LAST
} IdeSymbolKind;

typedef enum
{
  IDE_SYMBOL_FLAGS_NONE          = 0,
  IDE_SYMBOL_FLAGS_IS_STATIC     = 1 << 0,
  IDE_SYMBOL_FLAGS_IS_MEMBER     = 1 << 1,
  IDE_SYMBOL_FLAGS_IS_DEPRECATED = 1 << 2,
  IDE_SYMBOL_FLAGS_IS_DEFINITION = 1 << 3
} IdeSymbolFlags;

#define IDE_TYPE_SYMBOL (ide_symbol_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSymbol, ide_symbol, IDE, SYMBOL, GObject)

struct _IdeSymbolClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[16];
};

IDE_AVAILABLE_IN_ALL
IdeSymbol      *ide_symbol_new                 (const gchar    *name,
                                                IdeSymbolKind   kind,
                                                IdeSymbolFlags  flags,
                                                IdeLocation    *location,
                                                IdeLocation    *header_location);
IDE_AVAILABLE_IN_ALL
IdeSymbolKind   ide_symbol_get_kind            (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolFlags  ide_symbol_get_flags           (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
const gchar    *ide_symbol_get_name            (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
IdeLocation    *ide_symbol_get_location        (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
IdeLocation    *ide_symbol_get_header_location (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
IdeSymbol      *ide_symbol_new_from_variant    (GVariant       *variant);
IDE_AVAILABLE_IN_ALL
GVariant       *ide_symbol_to_variant          (IdeSymbol      *self);
IDE_AVAILABLE_IN_ALL
const char     *ide_symbol_kind_get_icon_name  (IdeSymbolKind   kind);
IDE_AVAILABLE_IN_ALL
GIcon          *ide_symbol_kind_get_gicon      (IdeSymbolKind   kind);

G_END_DECLS
