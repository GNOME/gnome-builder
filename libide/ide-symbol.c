/* ide-symbol.c
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

#include "ide-symbol.h"

struct _IdeSymbol
{
  volatile gint ref_count;
  gchar *name;
};

G_DEFINE_BOXED_TYPE (IdeSymbol, ide_symbol, ide_symbol_ref, ide_symbol_unref)

IdeSymbol *
_ide_symbol_new (const gchar *name)
{
  IdeSymbol *ret;

  ret = g_slice_new0 (IdeSymbol);
  ret->ref_count = 1;
  ret->name = g_strdup (name);

  return ret;
}

const gchar *
ide_symbol_get_name (IdeSymbol *self)
{
  g_return_val_if_fail (self, NULL);

  return self->name;
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
      g_free (self->name);
      g_slice_free (IdeSymbol, self);
    }
}
