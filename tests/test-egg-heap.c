/* test-egg-heap.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include "egg-heap.h"

typedef struct
{
   gint64 size;
   gpointer pointer;
} Tuple;

static int
cmpint_rev (gconstpointer a,
            gconstpointer b)
{
   return *(const gint *)b - *(const gint *)a;
}

static int
cmpptr_rev (gconstpointer a,
            gconstpointer b)
{
   return GPOINTER_TO_SIZE(*(gpointer *)b) - GPOINTER_TO_SIZE (*(gpointer *)a);
}

static int
cmptuple_rev (gconstpointer a,
              gconstpointer b)
{
   Tuple *at = (Tuple *)a;
   Tuple *bt = (Tuple *)b;

   return bt->size - at->size;
}

static void
test_EggHeap_insert_val_int (void)
{
   EggHeap *heap;
   gint i;

   heap = egg_heap_new (sizeof (gint), cmpint_rev);

   for (i = 0; i < 100000; i++) {
      egg_heap_insert_val (heap, i);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert_cmpint (egg_heap_peek (heap, gint), ==, i);
      egg_heap_extract (heap, NULL);
   }

   egg_heap_unref (heap);
}

static void
test_EggHeap_insert_val_ptr (void)
{
   gconstpointer ptr;
   EggHeap *heap;
   gint i;

   heap = egg_heap_new (sizeof (gpointer), cmpptr_rev);

   for (i = 0; i < 100000; i++) {
      ptr = GINT_TO_POINTER (i);
      egg_heap_insert_val (heap, ptr);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert (egg_heap_peek (heap, gpointer) == GINT_TO_POINTER (i));
      egg_heap_extract (heap, NULL);
   }

   egg_heap_unref (heap);
}

static void
test_EggHeap_insert_val_tuple (void)
{
   Tuple t;
   EggHeap *heap;
   gint i;

   heap = egg_heap_new (sizeof (Tuple), cmptuple_rev);

   for (i = 0; i < 100000; i++) {
      t.pointer = GINT_TO_POINTER (i);
      t.size = i;
      egg_heap_insert_val (heap, t);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert (egg_heap_peek (heap, Tuple).size == i);
      g_assert (egg_heap_peek (heap, Tuple).pointer == GINT_TO_POINTER (i));
      egg_heap_extract (heap, NULL);
   }

   egg_heap_unref (heap);
}

static void
test_EggHeap_extract_int (void)
{
   EggHeap *heap;
   gint removed[5];
   gint i;
   gint v;

   heap = egg_heap_new (sizeof (gint), cmpint_rev);

   for (i = 0; i < 100000; i++) {
      egg_heap_insert_val (heap, i);
   }

   removed [0] = egg_heap_index (heap, gint, 1578); egg_heap_extract_index (heap, 1578, NULL);
   removed [1] = egg_heap_index (heap, gint, 2289); egg_heap_extract_index (heap, 2289, NULL);
   removed [2] = egg_heap_index (heap, gint, 3312); egg_heap_extract_index (heap, 3312, NULL);
   removed [3] = egg_heap_index (heap, gint, 78901); egg_heap_extract_index (heap, 78901, NULL);
   removed [4] = egg_heap_index (heap, gint, 99000); egg_heap_extract_index (heap, 99000, NULL);

   for (i = 0; i < 100000; i++) {
      if (egg_heap_peek (heap, gint) != i) {
         g_assert ((i == removed[0]) ||
                   (i == removed[1]) ||
                   (i == removed[2]) ||
                   (i == removed[3]) ||
                   (i == removed[4]));
      } else {
         egg_heap_extract (heap, &v);
         g_assert_cmpint (v, ==, i);
      }
   }

   g_assert_cmpint (heap->len, ==, 0);

   egg_heap_unref (heap);
}

int
main (gint   argc,
      gchar *argv[])
{
   g_test_init (&argc, &argv, NULL);

   g_test_add_func ("/EggHeap/insert_and_extract<gint>", test_EggHeap_insert_val_int);
   g_test_add_func ("/EggHeap/insert_and_extract<gpointer>", test_EggHeap_insert_val_ptr);
   g_test_add_func ("/EggHeap/insert_and_extract<Tuple>", test_EggHeap_insert_val_tuple);
   g_test_add_func ("/EggHeap/extract_index<int>", test_EggHeap_extract_int);

   return g_test_run ();
}
