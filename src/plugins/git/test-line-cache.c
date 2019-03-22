/* line-cache.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "line-cache.h"

static void
test_basic (void)
{
  LineCache *lc1 = line_cache_new ();
  LineCache *lc2;
  GVariant *v;

  line_cache_mark_range (lc1, 0, 10, LINE_MARK_ADDED);
  line_cache_mark_range (lc1, 100, 200, LINE_MARK_REMOVED);

  v = line_cache_to_variant (lc1);
  g_assert_nonnull (v);

  lc2 = line_cache_new_from_variant (v);
  g_assert_nonnull (lc2);

  g_assert_cmpint (lc1->lines->len, ==, 110);
  g_assert_cmpint (lc2->lines->len, ==, 110);

  g_assert_true (0 == memcmp (lc1->lines->data,
                              lc2->lines->data,
                              sizeof (LineEntry) * lc1->lines->len));
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Plugins/Git/LineCache/basic", test_basic);
  return g_test_run ();
}
