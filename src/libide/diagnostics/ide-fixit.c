/* ide-fixit.c
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
 */

#define G_LOG_DOMAIN "ide-fixit"

#include "config.h"

#include <dazzle.h>

#include "diagnostics/ide-fixit.h"
#include "diagnostics/ide-source-range.h"

G_DEFINE_BOXED_TYPE (IdeFixit, ide_fixit, ide_fixit_ref, ide_fixit_unref)

struct _IdeFixit
{
  IdeSourceRange *range;
  gchar          *text;
};

DZL_DEFINE_COUNTER (instances, "IdeFixit", "Instances", "Number of fixit instances")

IdeFixit *
ide_fixit_new (IdeSourceRange *source_range,
               const gchar    *replacement_text)
{
  IdeFixit *self;

  g_return_val_if_fail (source_range, NULL);
  g_return_val_if_fail (replacement_text, NULL);

  self = g_atomic_rc_box_new0 (IdeFixit);
  self->range = ide_source_range_ref (source_range);
  self->text = g_strdup (replacement_text);

  DZL_COUNTER_INC (instances);

  return self;
}

static void
ide_fixit_destroy (IdeFixit *self)
{
  g_clear_pointer (&self->range, ide_source_range_unref);
  g_clear_pointer (&self->text, g_free);

  DZL_COUNTER_DEC (instances);
}

IdeFixit *
ide_fixit_ref (IdeFixit *self)
{
  g_return_val_if_fail (self, NULL);

  return g_atomic_rc_box_acquire (self);
}

void
ide_fixit_unref (IdeFixit *self)
{
  g_return_if_fail (self);

  g_atomic_rc_box_release_full (self, (GDestroyNotify)ide_fixit_destroy);
}

void
ide_fixit_apply (IdeFixit *self)
{
}

/**
 * ide_fixit_get_text:
 * @self: an #IdeFixit.
 *
 * Gets the text to replace the source range with.
 *
 * Returns: A string with the replacement text.
 *
 * Since: 3.32
 */
const gchar *
ide_fixit_get_text (IdeFixit *self)
{
  g_return_val_if_fail (self, NULL);

  return self->text;
}

/**
 * ide_fixit_get_range:
 * @self: an #IdeFixit.
 *
 * Gets the range for the replacement text. The range is non inclusive of the
 * end location. [a,b)
 *
 * Returns: (transfer none): An #IdeSourceRange.
 *
 * Since: 3.32
 */
IdeSourceRange *
ide_fixit_get_range (IdeFixit *self)
{
  g_return_val_if_fail (self, NULL);

  return self->range;
}

/**
 * ide_fixit_to_variant:
 * @self: a #IdeFixit
 *
 * Creates a #GVariant to represent a fixit.
 *
 * This function will never return a floating variant.
 *
 * Returns: (transfer full): a #GVariant
 *
 * Since: 3.32
 */
GVariant *
ide_fixit_to_variant (const IdeFixit *self)
{
  GVariantDict dict;
  g_autoptr(GVariant) vrange = NULL;

  g_return_val_if_fail (self != NULL, NULL);

  g_variant_dict_init (&dict, NULL);

  g_variant_dict_insert (&dict, "text", "s", self->text ?: "");

  if ((vrange = ide_source_range_to_variant (self->range)))
    g_variant_dict_insert_value (&dict, "range", vrange);

  return g_variant_take_ref (g_variant_dict_end (&dict));
}

/**
 * ide_fixit_new_from_variant:
 * @variant: (nullable): a #GVariant
 *
 * Creates a new #IdeFixit from the variant.
 *
 * If @variant is %NULL, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): an #IdeFixit or %NULL
 *
 * Since: 3.32
 */
IdeFixit *
ide_fixit_new_from_variant (GVariant *variant)
{
  g_autoptr(GVariant) unboxed = NULL;
  g_autoptr(GVariant) vrange = NULL;
  GVariantDict dict;
  IdeSourceRange *range = NULL;
  const gchar *text;
  IdeFixit *self = NULL;

  if (variant == NULL)
    return NULL;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    variant = unboxed = g_variant_get_variant (variant);

  g_variant_dict_init (&dict, variant);

  if (!g_variant_dict_lookup (&dict, "text", "&s", &text))
    text = "";

  if ((vrange = g_variant_dict_lookup_value (&dict, "range", NULL)))
    {
      if (!(range = ide_source_range_new_from_variant (vrange)))
        goto failed;
    }

  self = ide_fixit_new (range, text);

failed:

  g_variant_dict_clear (&dict);

  return self;
}
