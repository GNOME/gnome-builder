/* ide-diagnostic.c
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

#define G_LOG_DOMAIN "ide-diagnostic"

#include "dazzle.h"

#include "files/ide-file.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"

G_DEFINE_BOXED_TYPE (IdeDiagnostic, ide_diagnostic, ide_diagnostic_ref, ide_diagnostic_unref)

DZL_DEFINE_COUNTER (instances, "IdeDiagnostic", "Instances", "Number of IdeDiagnostic")

struct _IdeDiagnostic
{
  volatile gint          ref_count;
  IdeDiagnosticSeverity  severity;
  gchar                 *text;
  IdeSourceLocation     *location;
  GPtrArray             *fixits;
  GPtrArray             *ranges;
  guint                  hash;
};

guint
ide_diagnostic_hash (IdeDiagnostic *self)
{
  guint hash = self->hash;

  if (hash == 0)
    {
      hash ^= g_str_hash (self->text ?: "");
      if (self->location)
        hash ^= ide_source_location_hash (self->location);
      if (self->fixits)
        hash ^= g_int_hash (&self->fixits->len);
      if (self->ranges)
        hash ^= g_int_hash (&self->ranges->len);
    }

  return hash;
}

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
      g_clear_pointer (&self->fixits, g_ptr_array_unref);
      g_free (self);

      DZL_COUNTER_DEC (instances);
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

/**
 * ide_diagnostic_get_text_for_display:
 *
 * This creates a new string that is formatted using the diagnostics line number, column, severity,
 * and message text in the format "line:column: severity: message".
 *
 * This can be convenient when wanting to quickly display a diagnostic such as in a tooltip.
 *
 * Returns: (transfer full): A string containing the text formatted for display.
 */
gchar *
ide_diagnostic_get_text_for_display (IdeDiagnostic *self)
{
  IdeSourceLocation *location;
  const gchar *severity;
  guint line = 0;
  guint column = 0;

  g_return_val_if_fail (self, NULL);

  severity = ide_diagnostic_severity_to_string (self->severity);
  location = ide_diagnostic_get_location (self);
  if (location)
    {
      line = ide_source_location_get_line (location) + 1;
      column = ide_source_location_get_line_offset (location) + 1;
    }

  return g_strdup_printf ("%u:%u: %s: %s", line, column, severity, self->text);
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

/**
 * ide_diagnostic_get_location:
 * @self: An #IdeDiagnostic.
 *
 * Gets the location of a diagnostic.
 *
 * Returns: (transfer none): Gets the location of a diagnostic.
 */
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

/**
 * ide_diagnostic_new:
 * @severity: the severity of the diagnostic
 * @text: the diagnostic message text
 * @location: (nullable): the location of the diagnostic
 *
 * Creates a new diagnostic.
 *
 * If you want to set a range for the diagnostic, see
 * ide_diagnostic_add_range() or ide_diagnostic_take_range().
 *
 * Returns: (transfer full): An #IdeDiagnostic.
 */
IdeDiagnostic *
ide_diagnostic_new (IdeDiagnosticSeverity  severity,
                    const gchar           *text,
                    IdeSourceLocation     *location)
{
  IdeDiagnostic *ret;

  ret = g_new0 (IdeDiagnostic, 1);
  ret->ref_count = 1;
  ret->severity = severity;
  ret->text = g_strdup (text);
  ret->location = location ? ide_source_location_ref (location) : NULL;

  DZL_COUNTER_INC (instances);

  return ret;
}

/**
 * ide_diagnostic_take_fixit:
 * @self: A #IdeDiagnostic.
 * @fixit: (transfer full): An #IdeFixit.
 *
 * Adds the suggested fixit to the diagnostic while transfering ownership
 * of @fixit to @self.
 */
void
ide_diagnostic_take_fixit (IdeDiagnostic *self,
                           IdeFixit      *fixit)
{
  g_return_if_fail (self);
  g_return_if_fail (fixit);

  if (self->fixits == NULL)
    self->fixits = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_fixit_unref);

  g_ptr_array_add (self->fixits, fixit);
}

/**
 * ide_diagnostic_take_range:
 * @self: A #IdeDiagnostic.
 * @range: (transfer full): An #IdeSourceRange.
 *
 * Steals the ownership of @range and adds to the diagnostic.
 *
 * This saves multiple atomic references of @range which could be expensive
 * if you are doing lots of diagnostics.
 */
void
ide_diagnostic_take_range (IdeDiagnostic  *self,
                           IdeSourceRange *range)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  if (self->ranges == NULL)
    self->ranges = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_source_range_unref);

  g_ptr_array_add (self->ranges, range);

  if (self->location == NULL)
    self->location = ide_source_location_ref (ide_source_range_get_begin (range));
}

/**
 * ide_diagnostic_add_range:
 * @self: An #IdeDiagnostic.
 * @range: An #IdeSourceRange.
 *
 * Adds the range to the diagnostic. This allows diagnostic tools to highlight
 * the errored text appropriately.
 */
void
ide_diagnostic_add_range (IdeDiagnostic  *self,
                          IdeSourceRange *range)
{
  g_return_if_fail (self);
  g_return_if_fail (range);

  ide_diagnostic_take_range (self, ide_source_range_ref (range));
}

const gchar *
ide_diagnostic_severity_to_string (IdeDiagnosticSeverity severity)
{
  switch (severity)
    {
    case IDE_DIAGNOSTIC_IGNORED:
      return "ignored";

    case IDE_DIAGNOSTIC_NOTE:
      return "note";

    case IDE_DIAGNOSTIC_DEPRECATED:
      return "deprecated";

    case IDE_DIAGNOSTIC_WARNING:
      return "warning";

    case IDE_DIAGNOSTIC_ERROR:
      return "error";

    case IDE_DIAGNOSTIC_FATAL:
      return "fatal";

    default:
      return "unknown";
    }
}

guint
ide_diagnostic_get_num_fixits (IdeDiagnostic *self)
{
  g_return_val_if_fail (self, 0);

  return (self->fixits != NULL) ? self->fixits->len : 0;
}

/**
 * ide_diagnostic_get_fixit:
 * @self: A #IdeDiagnostic.
 * @index: The index of the fixit.
 *
 * Gets the fixit denoted by @index. This value should be less than the value
 * returned from ide_diagnostic_get_num_fixits().
 *
 * Returns: (transfer none): An #IdeFixit.
 */
IdeFixit *
ide_diagnostic_get_fixit (IdeDiagnostic *self,
                          guint          index)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->fixits, NULL);
  g_return_val_if_fail (index < self->fixits->len, NULL);

  return g_ptr_array_index (self->fixits, index);
}

gint
ide_diagnostic_compare (const IdeDiagnostic *a,
                        const IdeDiagnostic *b)
{
  gint ret;

  g_assert (a != NULL);
  g_assert (b != NULL);

  /* Severity is 0..N where N is more important. So reverse comparator. */
  if (0 != (ret = (gint)b->severity - (gint)a->severity))
    return ret;

  if (a->location && b->location)
    {
      if (0 != (ret = ide_source_location_compare (a->location, b->location)))
        return ret;
    }

  return g_strcmp0 (a->text, b->text);
}

/**
 * ide_diagnostic_get_file:
 *
 * This is a helper to simplify the process of determining what file
 * the diagnostic is within. It is equivalent to getting the source
 * location and looking at the file.
 *
 * Returns: (nullable) (transfer none): A #GFile or %NULL.
 */
GFile *
ide_diagnostic_get_file (IdeDiagnostic *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  if (self->location != NULL)
    {
      IdeFile *file = ide_source_location_get_file (self->location);

      if (file != NULL)
        return ide_file_get_file (file);
    }

  return NULL;
}
