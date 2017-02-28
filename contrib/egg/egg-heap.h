/* egg-heap.h
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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

#ifndef EGG_HEAP_H
#define EGG_HEAP_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EGG_TYPE_HEAP            (egg_heap_get_type())
#define egg_heap_insert_val(h,v) egg_heap_insert_vals(h,&(v),1)
#define egg_heap_index(h,t,i)    (((t*)(void*)(h)->data)[i])
#define egg_heap_peek(h,t)       egg_heap_index(h,t,0)

typedef struct _EggHeap EggHeap;

struct _EggHeap
{
  gchar *data;
  gsize  len;
};

GType      egg_heap_get_type      (void);
EggHeap   *egg_heap_new           (guint           element_size,
                                   GCompareFunc    compare_func);
EggHeap   *egg_heap_ref           (EggHeap        *heap);
void       egg_heap_unref         (EggHeap        *heap);
void       egg_heap_insert_vals   (EggHeap        *heap,
                                   gconstpointer   data,
                                   guint           len);
gboolean   egg_heap_extract       (EggHeap        *heap,
                                   gpointer        result);
gboolean   egg_heap_extract_index (EggHeap        *heap,
                                   gsize           index_,
                                   gpointer        result);

G_END_DECLS

#endif /* EGG_HEAP_H */
