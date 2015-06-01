/* rg-ring.h
 *
 * Copyright (C) 2010 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RG_RING_H__
#define __RG_RING_H__

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * rg_ring_append_val:
 * @ring: A #RgRing.
 * @val: A value to append to the #RgRing.
 *
 * Appends a value to the ring buffer.  @val must be a variable as it is
 * referenced to.
 *
 * Returns: None.
 */
#define rg_ring_append_val(ring, val) rg_ring_append_vals(ring, &(val), 1)

/**
 * rg_ring_get_index:
 * @ring: A #RgRing.
 * @type: The type to extract.
 * @i: The index within the #RgRing relative to the current position.
 *
 * Retrieves the value at the given index from the #RgRing.  The value
 * is cast to @type.  You may retrieve a pointer to the value within the
 * array by using &.
 *
 * [[
 * gdouble *v = &rg_ring_get_index(ring, gdouble, 0);
 * gdouble v = rg_ring_get_index(ring, gdouble, 0);
 * ]]
 *
 * Returns: The value at the given index.
 */
#define rg_ring_get_index(ring, type, i) \
  ((((type*)(ring)->data))[((i) + (ring)->pos) % (ring)->len])

typedef struct
{
	guint8 *data;
	guint   len;
	guint   pos;
} RgRing;

GType   rg_ring_get_type    (void);
RgRing *rg_ring_sized_new   (guint           element_size,
                             guint           reserved_size,
                             GDestroyNotify  element_destroy);
guint   rg_ring_append_vals (RgRing         *ring,
                             gconstpointer   data,
                             guint           len);
void    rg_ring_foreach     (RgRing         *ring,
                             GFunc           func,
                             gpointer        user_data);
RgRing *rg_ring_ref         (RgRing         *ring);
void    rg_ring_unref       (RgRing         *ring);

G_END_DECLS

#endif /* __RG_RING_H__ */
