/* ide-heap.h
 *
 * Copyright 2014-2022 Christian Hergert <christian@hergert.me>
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

#pragma once

#if !defined (IDE_IO_INSIDE) && !defined (IDE_IO_COMPILATION)
# error "Only <libide-io.h> can be included directly."
#endif

#include <libide-core.h>

G_BEGIN_DECLS

#define IDE_TYPE_HEAP            (ide_heap_get_type())
#define ide_heap_insert_val(h,v) ide_heap_insert_vals(h,&(v),1)
#define ide_heap_index(h,t,i)    (((t*)(void*)(h)->data)[i])
#define ide_heap_peek(h,t)       ide_heap_index(h,t,0)

typedef struct _IdeHeap
{
  char  *data;
  gsize  len;
} IdeHeap;

IDE_AVAILABLE_IN_ALL
GType      ide_heap_get_type      (void);
IDE_AVAILABLE_IN_ALL
IdeHeap   *ide_heap_new           (guint           element_size,
                                   GCompareFunc    compare_func);
IDE_AVAILABLE_IN_ALL
IdeHeap   *ide_heap_ref           (IdeHeap        *heap);
IDE_AVAILABLE_IN_ALL
void       ide_heap_unref         (IdeHeap        *heap);
IDE_AVAILABLE_IN_ALL
void       ide_heap_insert_vals   (IdeHeap        *heap,
                                   gconstpointer   data,
                                   guint           len);
IDE_AVAILABLE_IN_ALL
gboolean   ide_heap_extract       (IdeHeap        *heap,
                                   gpointer        result);
IDE_AVAILABLE_IN_ALL
gboolean   ide_heap_extract_index (IdeHeap        *heap,
                                   gsize           index_,
                                   gpointer        result);

G_END_DECLS
