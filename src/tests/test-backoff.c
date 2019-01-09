/* test-backoff.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>
#include <math.h>

static void
test_backoff_basic (void)
{
  IdeBackoff backoff;
  guint next;
  guint expected = 100;

  ide_backoff_init (&backoff, 100, G_MAXUINT);

  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, G_MAXUINT);
  g_assert_cmpint (backoff.cur_delay, ==, 100);
  g_assert_cmpint (backoff.n_failures, ==, 0);

  for (guint i = 0; i < 25; i++)
    {
      ide_backoff_failed (&backoff, &next);

      g_assert_cmpint (next, >=, expected);

      expected *= 2;

      g_assert_cmpint (backoff.min_delay, ==, 100);
      g_assert_cmpint (backoff.max_delay, ==, G_MAXUINT);
      g_assert_cmpint (backoff.cur_delay, ==, expected);
      g_assert_cmpint (backoff.n_failures, ==, i + 1);
    }

  ide_backoff_failed (&backoff, &next);

  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, G_MAXUINT);
  g_assert_cmpint (backoff.cur_delay, ==, G_MAXUINT);
  g_assert_cmpint (backoff.n_failures, ==, 26);

  ide_backoff_succeeded (&backoff);

  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, G_MAXUINT);
  g_assert_cmpint (backoff.cur_delay, ==, 100);
  g_assert_cmpint (backoff.n_failures, ==, 0);
}

static void
test_backoff_max (void)
{
  IdeBackoff backoff;
  guint next;

  ide_backoff_init (&backoff, 100, 300);

  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, 300);
  g_assert_cmpint (backoff.cur_delay, ==, 100);
  g_assert_cmpint (backoff.n_failures, ==, 0);

  ide_backoff_failed (&backoff, &next);
  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, 300);
  g_assert_cmpint (backoff.cur_delay, ==, 200);
  g_assert_cmpint (backoff.n_failures, ==, 1);
  g_assert_cmpint (next, >=, 100);
  g_assert_cmpint (next, <, 300);

  ide_backoff_failed (&backoff, &next);
  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, 300);
  g_assert_cmpint (backoff.cur_delay, ==, 300);
  g_assert_cmpint (backoff.n_failures, ==, 2);
  g_assert_cmpint (next, >=, 200);
  g_assert_cmpint (next, <=, 300);

  ide_backoff_failed (&backoff, &next);
  g_assert_cmpint (backoff.min_delay, ==, 100);
  g_assert_cmpint (backoff.max_delay, ==, 300);
  g_assert_cmpint (backoff.cur_delay, ==, 300);
  g_assert_cmpint (backoff.n_failures, ==, 3);
  g_assert_cmpint (next, >=, 200);
  g_assert_cmpint (next, <=, 300);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Backoff/basic", test_backoff_basic);
  g_test_add_func ("/Ide/Backoff/max", test_backoff_max);
  g_test_run ();
}
