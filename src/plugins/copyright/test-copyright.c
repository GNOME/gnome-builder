/* test-copyright.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include "gbp-copyright-util.h"

#define TEST_YEAR "2042"

static void
test_update_copyright (void)
{
  static const struct {
    const char *input;
    const char *output; /* NULL indicates no change requested */
  } copyright_year_tests[] = {
    { "1234", "1234-"TEST_YEAR },
    { " 1234-", " 1234-"TEST_YEAR },
    { "-1234", "-1234-"TEST_YEAR }, /* odd, but expected */
    { "-", NULL },
    { "", NULL },
    { "# Copyright 2019 Foo", "# Copyright 2019-"TEST_YEAR" Foo" },
    { "# Copyright "TEST_YEAR" Foo", NULL },
    { "# Copyright -"TEST_YEAR" Foo", NULL },
    { "/* Copyright "TEST_YEAR"- Foo */", NULL },
    { "# Copyright 2019- Foo", "# Copyright 2019-"TEST_YEAR" Foo" },
    { "# Copyright - ", NULL },
  };

  for (guint i = 0; i < G_N_ELEMENTS (copyright_year_tests); i++)
    {
      g_autofree char *replaced = gbp_update_copyright (copyright_year_tests[i].input, TEST_YEAR);

      g_assert_cmpstr (replaced, ==, copyright_year_tests[i].output);
    }
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Plugins/Copyright/update_copyright", test_update_copyright);
  return g_test_run ();
}
