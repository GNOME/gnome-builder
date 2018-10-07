/* ide-diagnostics.c
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

#define G_LOG_DOMAIN "ide-diagnostics"

#include "config.h"

#include <dazzle.h>

#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-diagnostics.h"
#include "util/ide-glib.h"

#define DIAGNOSTICS_MAGIC   0x82645329
#define IS_DIAGNOSTICS(ptr) ((ptr)->magic == DIAGNOSTICS_MAGIC)

G_DEFINE_BOXED_TYPE (IdeDiagnostics, ide_diagnostics, ide_diagnostics_ref, ide_diagnostics_unref)

DZL_DEFINE_COUNTER (instances, "IdeDiagnostics", "Instances", "Number of IdeDiagnostics")

struct _IdeDiagnostics
{
  guint          magic;
  GPtrArray     *diagnostics;
};

/**
 * ide_diagnostics_new:
 * @ar: (transfer full) (element-type Ide.Diagnostic) (nullable): an array of #IdeDiagnostic.
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
    ar = g_ptr_array_new ();

  IDE_PTR_ARRAY_SET_FREE_FUNC (ar, ide_diagnostic_unref);

  ret = g_atomic_rc_box_new0 (IdeDiagnostics);
  ret->magic = DIAGNOSTICS_MAGIC;
  ret->diagnostics = ar;

  DZL_COUNTER_INC (instances);

  return ret;
}

IdeDiagnostics *
ide_diagnostics_ref (IdeDiagnostics *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (IS_DIAGNOSTICS (self), NULL);

  return g_atomic_rc_box_acquire (self);
}

static void
ide_diagnostics_finalize (IdeDiagnostics *self)
{
  g_clear_pointer (&self->diagnostics, g_ptr_array_unref);

  DZL_COUNTER_DEC (instances);
}

void
ide_diagnostics_unref (IdeDiagnostics *self)
{
  g_return_if_fail (self);
  g_return_if_fail (IS_DIAGNOSTICS (self));

  g_atomic_rc_box_release_full (self, (GDestroyNotify)ide_diagnostics_finalize);
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
  g_return_if_fail (self != NULL);
  g_return_if_fail (IS_DIAGNOSTICS (self));
  g_return_if_fail (other != NULL);
  g_return_if_fail (IS_DIAGNOSTICS (other));

  if (self->diagnostics == NULL && other->diagnostics == NULL)
    return;

  if (self->diagnostics == NULL)
    self->diagnostics = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_diagnostic_unref);

  if (other->diagnostics != NULL)
    {
      for (guint i = 0; i < other->diagnostics->len; i++)
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
  g_return_val_if_fail (IS_DIAGNOSTICS (self), 0);

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
  g_return_val_if_fail (IS_DIAGNOSTICS (self), NULL);
  g_return_val_if_fail (self->diagnostics, NULL);
  g_return_val_if_fail (index < self->diagnostics->len, NULL);

  return g_ptr_array_index (self->diagnostics, index);
}

void
ide_diagnostics_add (IdeDiagnostics *self,
                     IdeDiagnostic  *diagnostic)
{
  g_return_if_fail (self);
  g_return_if_fail (IS_DIAGNOSTICS (self));
  g_return_if_fail (diagnostic);

  g_ptr_array_add (self->diagnostics, ide_diagnostic_ref (diagnostic));
}

/**
 * ide_diagnostics_take:
 * @self: a #IdeDiagnostics
 * @diagnostic: (transfer full): an #IdeDiagnostic
 *
 * Like ide_diagnostics_add() but steals the reference to @diagnostic.
 *
 * Since: 3.30
 */
void
ide_diagnostics_take (IdeDiagnostics *self,
                      IdeDiagnostic  *diagnostic)
{
  g_return_if_fail (self);
  g_return_if_fail (IS_DIAGNOSTICS (self));
  g_return_if_fail (diagnostic);

  g_ptr_array_add (self->diagnostics, diagnostic);
}
