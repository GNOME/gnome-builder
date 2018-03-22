/* ide-symbol.c
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

#define G_LOG_DOMAIN "ide-symbol"

#include "config.h"

#include <dazzle.h>

#include "diagnostics/ide-source-location.h"
#include "symbols/ide-symbol.h"

struct _IdeSymbol
{
  volatile gint ref_count;

  IdeSymbolKind     kind;
  IdeSymbolFlags    flags;

  gchar             *name;
  IdeSourceLocation *declaration_location;
  IdeSourceLocation *definition_location;
  IdeSourceLocation *canonical_location;
};

G_DEFINE_BOXED_TYPE (IdeSymbol, ide_symbol, ide_symbol_ref, ide_symbol_unref)

DZL_DEFINE_COUNTER (instances, "IdeSymbol", "Instances", "Number of symbol instances")

/**
 * ide_symbol_new:
 * @name: the symbol name
 * @kind: the symbol kind
 * @flags: the symbol flags
 * @declaration_location: (allow-none): the declaration location
 * @definition_location: (allow-none): the definition location
 * @canonical_location: (allow-none): the canonical location
 *
 * Returns: (transfer full): a new #IdeSymbol.
 */
IdeSymbol *
ide_symbol_new (const gchar       *name,
                IdeSymbolKind      kind,
                IdeSymbolFlags     flags,
                IdeSourceLocation *declaration_location,
                IdeSourceLocation *definition_location,
                IdeSourceLocation *canonical_location)
{
  IdeSymbol *ret;

  ret = g_slice_new0 (IdeSymbol);
  ret->ref_count = 1;
  ret->kind = kind;
  ret->flags = flags;
  ret->name = g_strdup (name);

  if (declaration_location)
    ret->declaration_location = ide_source_location_ref (declaration_location);

  if (definition_location)
    ret->definition_location = ide_source_location_ref (definition_location);

  if (canonical_location)
    ret->canonical_location = ide_source_location_ref (canonical_location);

  DZL_COUNTER_INC (instances);

  return ret;
}

const gchar *
ide_symbol_get_name (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);

  return self->name;
}

/**
 * ide_symbol_get_declaration_location:
 *
 * The location of a symbol equates to the declaration of the symbol. In C and C++, this would
 * mean the header location (or forward declaration in a C file before the implementation).
 *
 * If the symbol provider did not register this information, %NULL will be returned.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceLocation or %NULL.
 */
IdeSourceLocation *
ide_symbol_get_declaration_location (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);

  return self->declaration_location;
}

/**
 * ide_symbol_get_definition_location:
 *
 * Like ide_symbol_get_declaration_location() but gets the first declaration (only one can be
 * the definition).
 *
 * Returns: (transfer none) (nullable): An #IdeSourceLocation or %NULL.
 */
IdeSourceLocation *
ide_symbol_get_definition_location (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);

  return self->definition_location;
}

/**
 * ide_symbol_get_canonical_location:
 *
 * Gets the location of the symbols "implementation". In C/C++ languages, you can have multiple
 * declarations by only a single implementation.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceLocation or %NULL.
 */
IdeSourceLocation *
ide_symbol_get_canonical_location (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);

  return self->canonical_location;
}

IdeSymbolKind
ide_symbol_get_kind (IdeSymbol *self)
{
  g_return_val_if_fail (self, 0);

  return self->kind;
}

IdeSymbolFlags
ide_symbol_get_flags (IdeSymbol *self)
{
  g_return_val_if_fail (self, 0);

  return self->flags;
}

IdeSymbol *
ide_symbol_ref (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_symbol_unref (IdeSymbol *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_clear_pointer (&self->declaration_location, ide_source_location_unref);
      g_clear_pointer (&self->definition_location, ide_source_location_unref);
      g_clear_pointer (&self->canonical_location, ide_source_location_unref);
      g_clear_pointer (&self->name, g_free);
      g_slice_free (IdeSymbol, self);

      DZL_COUNTER_DEC (instances);
    }
}

const gchar *
ide_symbol_kind_get_icon_name (IdeSymbolKind kind)
{
  const gchar *icon_name = NULL;

  switch (kind)
    {
    case IDE_SYMBOL_ALIAS:
      icon_name = "lang-typedef-symbolic";
      break;

    case IDE_SYMBOL_CLASS:
      icon_name = "lang-class-symbolic";
      break;

    case IDE_SYMBOL_ENUM:
      icon_name = "lang-enum-symbolic";
      break;

    case IDE_SYMBOL_ENUM_VALUE:
      icon_name = "lang-enum-value-symbolic";
      break;

    case IDE_SYMBOL_FUNCTION:
      icon_name = "lang-function-symbolic";
      break;

    case IDE_SYMBOL_PACKAGE:
      icon_name = "lang-include-symbolic";
      break;

    case IDE_SYMBOL_MACRO:
      icon_name = "lang-define-symbolic";
      break;

    case IDE_SYMBOL_METHOD:
      icon_name = "lang-method-symbolic";
      break;

    case IDE_SYMBOL_NAMESPACE:
      icon_name = "lang-namespace-symbolic";
      break;

    case IDE_SYMBOL_STRUCT:
      icon_name = "lang-struct-symbolic";
      break;

    case IDE_SYMBOL_SCALAR:
    case IDE_SYMBOL_FIELD:
    case IDE_SYMBOL_VARIABLE:
      icon_name = "lang-variable-symbolic";
      break;

    case IDE_SYMBOL_UNION:
      icon_name = "lang-union-symbolic";
      break;

    case IDE_SYMBOL_ARRAY:
    case IDE_SYMBOL_BOOLEAN:
    case IDE_SYMBOL_CONSTANT:
    case IDE_SYMBOL_CONSTRUCTOR:
    case IDE_SYMBOL_FILE:
    case IDE_SYMBOL_HEADER:
    case IDE_SYMBOL_INTERFACE:
    case IDE_SYMBOL_MODULE:
    case IDE_SYMBOL_NUMBER:
    case IDE_SYMBOL_NONE:
    case IDE_SYMBOL_PROPERTY:
    case IDE_SYMBOL_STRING:
    case IDE_SYMBOL_TEMPLATE:
    case IDE_SYMBOL_KEYWORD:
      icon_name = NULL;
      break;

    case IDE_SYMBOL_UI_ATTRIBUTES:
      icon_name = "ui-attributes-symbolic";
      break;

    case IDE_SYMBOL_UI_CHILD:
      icon_name = "ui-child-symbolic";
      break;

    case IDE_SYMBOL_UI_ITEM:
      icon_name = "ui-item-symbolic";
      break;

    case IDE_SYMBOL_UI_MENU:
      icon_name = "ui-menu-symbolic";
      break;

    case IDE_SYMBOL_UI_OBJECT:
      icon_name = "ui-object-symbolic";
      break;

    case IDE_SYMBOL_UI_PACKING:
      icon_name = "ui-packing-symbolic";
      break;

    case IDE_SYMBOL_UI_PROPERTY:
      icon_name = "ui-property-symbolic";
      break;

    case IDE_SYMBOL_UI_SECTION:
      icon_name = "ui-section-symbolic";
      break;

    case IDE_SYMBOL_UI_SIGNAL:
      icon_name = "ui-signal-symbolic";
      break;

    case IDE_SYMBOL_UI_STYLE:
      icon_name = "ui-style-symbolic";
      break;

    case IDE_SYMBOL_UI_SUBMENU:
      icon_name = "ui-submenu-symbolic";
      break;

    case IDE_SYMBOL_UI_TEMPLATE:
      icon_name = "ui-template-symbolic";
      break;

    case IDE_SYMBOL_XML_ATTRIBUTE:
      icon_name = "xml-attribute-symbolic";
      break;

    case IDE_SYMBOL_XML_CDATA:
      icon_name = "xml-cdata-symbolic";
      break;

    case IDE_SYMBOL_XML_COMMENT:
      icon_name = "xml-comment-symbolic";
      break;

    case IDE_SYMBOL_XML_DECLARATION:
      icon_name = "xml-declaration-symbolic";
      break;

    case IDE_SYMBOL_XML_ELEMENT:
      icon_name = "xml-element-symbolic";
      break;

    case IDE_SYMBOL_UI_MENU_ATTRIBUTE:
    case IDE_SYMBOL_UI_STYLE_CLASS:
      icon_name = NULL;
      break;

    default:
      icon_name = NULL;
      break;
    }

  return icon_name;
}
