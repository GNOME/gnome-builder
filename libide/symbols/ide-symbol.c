/* ide-symbol.c
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

#define G_LOG_DOMAIN "ide-symbol"

#include <egg-counter.h>

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

EGG_DEFINE_COUNTER (instances, "IdeSymbol", "Instances", "Number of symbol instances")

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

  EGG_COUNTER_INC (instances);

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

      EGG_COUNTER_DEC (instances);
    }
}
