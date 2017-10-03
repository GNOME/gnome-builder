/* ide-diagnostics.c
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

#define G_LOG_DOMAIN "ide-diagnostics"

#include "dazzle.h"

#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics.h"

G_DEFINE_BOXED_TYPE (IdeDiagnostics, ide_diagnostics, ide_diagnostics_ref, ide_diagnostics_unref)

DZL_DEFINE_COUNTER (instances, "IdeDiagnostics", "Instances", "Number of IdeDiagnostics")

struct _IdeDiagnostics
{
  volatile gint  ref_count;
  GPtrArray     *diagnostics;
};

/**
 * ide_diagnostics_new:
 * @ar: (transfer container) (element-type Ide.Diagnostic) (allow-none): an array of #IdeDiagnostic.
 *
 * Creates a new #IdeDiagnostics container structure for @ar.
 * Ownership of @ar is transfered to the resulting structure.
 *
 * Returns: (transfer full): A newly allocated #IdeDiagnostics.
 */
IdeDiagnostics *
ide_diagnostics_new (GPtrArray *ar)
{
  IdeDiagnostics *ret;

  if (ar == NULL)
    ar = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);

  ret = g_slice_new0 (IdeDiagnostics);
  ret->ref_count = 1;
  ret->diagnostics = ar;

  DZL_COUNTER_INC (instances);

  return ret;
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
      g_clear_pointer (&self->diagnostics, g_ptr_array_unref);
      g_slice_free (IdeDiagnostics, self);

      DZL_COUNTER_DEC (instances);
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

  if (!self->diagnostics)
    {
      self->diagnostics = g_ptr_array_new_with_free_func (
        (GDestroyNotify)ide_diagnostic_unref);
    }

  if (other->diagnostics)
    {
      for (i = 0; i < other->diagnostics->len; i++)
        {
          IdeDiagnostic *diag;

          diag = g_ptr_array_index (other->diagnostics, i);
          g_ptr_array_add (self->diagnostics, ide_diagnostic_ref (diag));
        }
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

  return self->diagnostics ? self->diagnostics->len : 0;
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

void
ide_diagnostics_add (IdeDiagnostics *self,
                     IdeDiagnostic  *diagnostic)
{
  g_assert (self != NULL);
  g_assert (diagnostic != NULL);

  g_ptr_array_add (self->diagnostics, ide_diagnostic_ref (diagnostic));
}
