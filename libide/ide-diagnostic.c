/* ide-diagnostic.c
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

#include "ide-diagnostic.h"
#include "ide-internal.h"
#include "ide-source-location.h"
#include "ide-source-range.h"

G_DEFINE_BOXED_TYPE (IdeDiagnostic, ide_diagnostic,
                     ide_diagnostic_ref, ide_diagnostic_unref)

struct _IdeDiagnostic
{
  volatile gint          ref_count;
  IdeDiagnosticSeverity  severity;
  gchar                 *text;
  IdeSourceLocation     *location;
  GPtrArray             *ranges;
};

IdeDiagnostic *
ide_diagnostic_ref (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
ide_diagnostic_unref (IdeDiagnostic *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_clear_pointer (&self->location, ide_source_location_unref);
      g_clear_pointer (&self->text, g_free);
      g_clear_pointer (&self->ranges, g_ptr_array_unref);
      g_slice_free (IdeDiagnostic, self);
    }
}

IdeDiagnosticSeverity
ide_diagnostic_get_severity (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, IDE_DIAGNOSTIC_IGNORED);

  return self->severity;
}

const gchar *
ide_diagnostic_get_text (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, NULL);

  return self->text;
}

guint
ide_diagnostic_get_num_ranges (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, 0);

  return self->ranges ? self->ranges->len : 0;
}

/**
 * ide_diagnostic_get_range:
 *
 * Retrieves the range found at @index. It is a programming error to call this
 * function with a value greater or equal to ide_diagnostic_get_num_ranges().
 *
 * Returns: (transfer none) (nullable): An #IdeSourceRange
 */
IdeSourceRange *
ide_diagnostic_get_range (IdeDiagnostic *self,
                          guint          index)
{
  g_return_val_if_fail (self, NULL);

  if (self->ranges)
    {
      if (index < self->ranges->len)
        return g_ptr_array_index (self->ranges, index);
    }

  return NULL;
}

IdeSourceLocation *
ide_diagnostic_get_location (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, NULL);

  if (self->location)
    return self->location;

  if (self->ranges && self->ranges->len > 0)
    {
      IdeSourceRange *range;

      range = ide_diagnostic_get_range (self, 0);
      return ide_source_range_get_begin (range);
    }

  return NULL;
}

IdeDiagnostic *
_ide_diagnostic_new (IdeDiagnosticSeverity  severity,
                     const gchar           *text,
                     IdeSourceLocation     *location)
{
  IdeDiagnostic *ret;

  ret = g_slice_new0 (IdeDiagnostic);
  ret->ref_count = 1;
  ret->severity = severity;
  ret->text = g_strdup (text);
  ret->location = location ? ide_source_location_ref (location) : NULL;

  return ret;
}

void
_ide_diagnostic_take_range (IdeDiagnostic  *self,
                            IdeSourceRange *range)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  if (!self->ranges)
    {
      self->ranges = g_ptr_array_new ();
      g_ptr_array_set_free_func (self->ranges,
                                 (GDestroyNotify)ide_source_range_unref);
    }

  g_ptr_array_add (self->ranges, range);
}

void
_ide_diagnostic_add_range (IdeDiagnostic  *self,
                           IdeSourceRange *range)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  _ide_diagnostic_take_range (self, ide_source_range_ref (range));
}

GType
ide_diagnostic_severity_get_type (void)
{
  static gsize type_id;

  if (g_once_init_enter (&type_id))
    {
      gsize _type_id;
      static const GEnumValue values[] = {
        { IDE_DIAGNOSTIC_IGNORED, "IDE_DIAGNOSTIC_IGNORED", "IGNORED" },
        { IDE_DIAGNOSTIC_NOTE, "IDE_DIAGNOSTIC_NOTE", "NOTE" },
        { IDE_DIAGNOSTIC_WARNING, "IDE_DIAGNOSTIC_WARNING", "WARNING" },
        { IDE_DIAGNOSTIC_ERROR, "IDE_DIAGNOSTIC_ERROR", "ERROR" },
        { IDE_DIAGNOSTIC_FATAL, "IDE_DIAGNOSTIC_FATAL", "FATAL" },
        { 0 }
      };

      _type_id = g_enum_register_static ("IdeDiagnosticSeverity", values);
      g_once_init_leave (&type_id, _type_id);
    }

  return type_id;
}
