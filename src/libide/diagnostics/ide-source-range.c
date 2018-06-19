/* ide-source-range.c
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

#define G_LOG_DOMAIN "ide-source-range"

#include "config.h"

#include <dazzle.h>

#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "files/ide-file.h"

G_DEFINE_BOXED_TYPE (IdeSourceRange, ide_source_range, ide_source_range_ref, ide_source_range_unref)

DZL_DEFINE_COUNTER (instances, "IdeSourceRange", "Instances", "Number of IdeSourceRange instances.")

struct _IdeSourceRange
{
  volatile gint      ref_count;
  IdeSourceLocation *begin;
  IdeSourceLocation *end;
};

IdeSourceRange *
ide_source_range_new (IdeSourceLocation *begin,
                      IdeSourceLocation *end)
{
  IdeSourceRange *ret;

  g_return_val_if_fail (begin, NULL);
  g_return_val_if_fail (end, NULL);
  g_return_val_if_fail (ide_file_equal (ide_source_location_get_file (begin),
                                        ide_source_location_get_file (end)),
                        NULL);

  ret = g_slice_new0 (IdeSourceRange);
  ret->ref_count = 1;
  ret->begin = ide_source_location_ref (begin);
  ret->end = ide_source_location_ref (end);

  DZL_COUNTER_INC (instances);

  return ret;
}

/**
 * ide_source_range_get_begin:
 *
 * Gets the beginning of the source range.
 *
 * Returns: (transfer none): An #IdeSourceLocation.
 */
IdeSourceLocation *
ide_source_range_get_begin (IdeSourceRange *self)
{
  g_return_val_if_fail (self, NULL);

  return self->begin;
}

/**
 * ide_source_range_get_end:
 *
 * Gets the end of the source range.
 *
 * Returns: (transfer none): An #IdeSourceLocation.
 */
IdeSourceLocation *
ide_source_range_get_end (IdeSourceRange *self)
{
  g_return_val_if_fail (self, NULL);

  return self->end;
}

/**
 * ide_source_range_ref:
 * @self: An #IdeSourceRange
 *
 * Increments the reference count of @self by one. When you are done with
 * @self, release it by calling ide_source_range_unref().
 *
 * Returns: (transfer full): @self
 */
IdeSourceRange *
ide_source_range_ref (IdeSourceRange *self)
{
  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * ide_source_range_unref:
 * @self: (transfer full): An #IdeSourceRange
 *
 * Decrements the reference count of @self by one.
 */
void
ide_source_range_unref (IdeSourceRange *self)
{
  g_return_if_fail (self);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      ide_source_location_unref (self->begin);
      ide_source_location_unref (self->end);
      g_slice_free (IdeSourceRange, self);

      DZL_COUNTER_DEC (instances);
    }
}

/**
 * ide_source_range_to_variant:
 * @self: a #IdeSourceRange
 *
 * Creates a variant to represent the range.
 *
 * This function will never return a floating variant.
 *
 * Returns: (transfer full): a #GVariant
 *
 * Since: 3.30
 */
GVariant *
ide_source_range_to_variant (const IdeSourceRange *self)
{
  GVariantDict dict;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  if (self->begin)
    {
      g_autoptr(GVariant) begin = NULL;

      if ((begin = ide_source_location_to_variant (self->begin)))
        g_variant_dict_insert_value (&dict, "begin", begin);
    }

  if (self->end)
    {
      g_autoptr(GVariant) end = NULL;

      if ((end = ide_source_location_to_variant (self->end)))
        g_variant_dict_insert_value (&dict, "end", end);
    }

  return g_variant_ref_sink (g_variant_dict_end (&dict));
}

IdeSourceRange *
ide_source_range_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) vbegin = NULL;
  g_autoptr(GVariant) vend = NULL;
  g_autoptr(IdeSourceLocation) begin = NULL;
  g_autoptr(IdeSourceLocation) end = NULL;
  IdeSourceRange *self = NULL;
  GVariantDict dict;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  g_variant_dict_init (&dict, variant);

  if (!(vbegin = g_variant_dict_lookup_value (&dict, "begin", NULL)) ||
      !(begin = ide_source_location_new_from_variant (vbegin)))
    goto failure;

  if (!(vend = g_variant_dict_lookup_value (&dict, "end", NULL)) ||
      !(end = ide_source_location_new_from_variant (vend)))
    goto failure;

  self = ide_source_range_new (begin, end);

  g_variant_dict_clear (&dict);

failure:

  return self;
}
