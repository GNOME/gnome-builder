/* ide-diagnostics.c
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

#include "ide-diagnostics.h"

struct _IdeDiagnostics
{
  volatile gint  ref_count;
  GPtrArray     *diagnostics;
};

GType
ide_diagnostics_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      gsize _type_id;

      _type_id = g_boxed_type_register_static (
          "IdeDiagnostics",
          (GBoxedCopyFunc)ide_diagnostics_ref,
          (GBoxedFreeFunc)ide_diagnostics_unref);

      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}

IdeDiagnostics *
ide_diagnostics_ref (IdeDiagnostics *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_diagnostics_unref (IdeDiagnostics *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_ptr_array_unref (self->diagnostics);
      g_slice_free (IdeDiagnostics, self);
    }
}

/**
 * ide_diagnostics_merge:
 *
 * Copies the contents of @other into @self.
 *
 * This is performed by taking a reference to the immutable #IdeDiagnostic
 * instances.
 */
void
ide_diagnostics_merge (IdeDiagnostics *self,
                       IdeDiagnostics *other)
{
  gsize i;

  g_return_if_fail (self);
  g_return_if_fail (other);

  for (i = 0; i < other->diagnostics->len; i++)
    {
      IdeDiagnostic *diag;

      diag = g_ptr_array_index (other->diagnostics, i);
      g_ptr_array_add (self->diagnostics, ide_diagnostic_ref (diag));
    }
}

/**
 * ide_diagnostics_get_size:
 *
 * Retrieves the number of diagnostics that can be accessed via
 * ide_diagnostics_index().
 *
 * Returns: The number of diagnostics in @self.
 */
gsize
ide_diagnostics_get_size (IdeDiagnostics *self)
{
  g_return_val_if_fail (self, 0);
  g_return_val_if_fail (self->diagnostics, 0);

  return self->diagnostics->len;
}

/**
 * ide_diagnostics_index:
 *
 * Retrieves the diagnostic at @index.
 *
 * Returns: (transfer none): An #IdeDiagnostic.
 */
IdeDiagnostic *
ide_diagnostics_index (IdeDiagnostics *self,
                       gsize           index)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->diagnostics, NULL);
  g_return_val_if_fail (index < self->diagnostics->len, NULL);

  return g_ptr_array_index (self->diagnostics, index);
}
