/* gstyle-color-item.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gstyle-color-item"

#include "gstyle-color-item.h"

G_DEFINE_BOXED_TYPE (GstyleColorItem, gstyle_color_item, gstyle_color_item_ref, gstyle_color_item_unref)

/**
 * gstyle_color_item_get_color:
 * @self: a #GstyleColorItem
 *
 * Get the #GstyleColor inside the #GstyleColorItem.
 *
 * Returns: (transfer none): a #GstyleColor.
 *
 */
const GstyleColor *
gstyle_color_item_get_color (GstyleColorItem *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->color;
}

/**
 * gstyle_color_item_set_color:
 * @self: a #GstyleColorItem
 * @color: (nullable): a #GstyleColor or %NULL
 *
 * Set the #GstyleColor inside the #GstyleColorItem.
 *
 */
void
gstyle_color_item_set_color (GstyleColorItem *self,
                             GstyleColor     *color)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GSTYLE_IS_COLOR (color));

  g_clear_object (&self->color);
  self->color = g_object_ref (color);
}

/**
 * gstyle_color_item_get_start:
 * @self: a #GstyleColorItem
 *
 * Get the start position of the #GstyleColorItem.
 *
 * Returns: a position in bytes, in the analysed buffer, starting from offset 0.
 *
 */
guint
gstyle_color_item_get_start (GstyleColorItem *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->start;
}

/**
 * gstyle_color_item_get_len:
 * @self: a #GstyleColorItem
 *
 * Get the size of the #GstyleColorItem.
 *
 * Returns: the #GstyleColorItem size in bytes.
 *
 */
guint
gstyle_color_item_get_len (GstyleColorItem *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->len;
}

/**
 * gstyle_color_item_copy:
 * @self: a #GstyleColorItem
 *
 * Makes a deep copy of a #GstyleColorItem
 *
 * The result must be freed with gstyle_color_item_free()
 * or un-refed with gstyle_color_item_unref().
 *
 * Returns: (transfer full): A newly allocated #GstyleColorItem, with the same contents as @self
 *
 */
GstyleColorItem *
gstyle_color_item_copy (GstyleColorItem *self)
{
  GstyleColorItem *item;
  GstyleColor *src_color;

  g_return_val_if_fail (self != NULL, NULL);

  item = g_slice_dup (GstyleColorItem, self);

  src_color = (GstyleColor *)gstyle_color_item_get_color (self);
  if (src_color != NULL && GSTYLE_IS_COLOR (src_color))
    self->color = gstyle_color_copy (src_color);

  return item;
}

/**
 * gstyle_color_item_free:
 * @self: a #GstyleColorItem
 *
 * Free a #GstyleColorItem created with gstyle_color_item_copy().
 *
 */
void
gstyle_color_item_free (GstyleColorItem *self)
{
  g_return_if_fail (self != NULL);
  g_assert_cmpint (self->ref_count, ==, 0);

  if (self->color != NULL && GSTYLE_IS_COLOR (self->color))
    g_object_unref (self->color);

  g_slice_free (GstyleColorItem, self);
}

/**
 * gstyle_color_item_new:
 * @color: (nullable): a #GstyleColor or NULL
 * @start: start offset of the item
 * @len: length of the item
 *
 * Return a new #GstyleColorItem.
 * It must be freed with gstyle_color_item_free().
 *
 * Returns: a #GstyleColorItem.
 *
 */
GstyleColorItem *
gstyle_color_item_new (GstyleColor *color,
                       gint         start,
                       gint         len)
{
  GstyleColorItem *item;

  g_return_val_if_fail (GSTYLE_IS_COLOR (color) || color == NULL, NULL);

  item = g_slice_new0 (GstyleColorItem);
  item->ref_count = 1;
  item->start = start;
  item->len = len;

  if (color != NULL)
    item->color = g_object_ref (color);

  return item;
}

/**
 * gstyle_color_item_ref:
 * @self: An #GstyleColorItem
 *
 * Increments the reference count of @self by one.
 *
 * Returns: (transfer none): @self
 */
GstyleColorItem *
gstyle_color_item_ref (GstyleColorItem *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gstyle_color_item_unref:
 * @self: (transfer none): An #GstyleColorItem
 *
 * Decrements the reference count of @GstyleColorItem by one, freeing the structure
 * when the reference count reaches zero.
 */
void
gstyle_color_item_unref (GstyleColorItem *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gstyle_color_item_free (self);
}
