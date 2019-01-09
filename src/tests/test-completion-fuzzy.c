/* test-completion-fuzzy.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include <libide-sourceview.h>

static void
test_fuzzy_match (void)
{
  static const struct {
    const gchar *haystack;
    const gchar *casefold;
  } success[] = {
    { "endianness", "end" },
    { "Endianness", "end" },
    { "Endianness", "End" },
    { "GtkWidget", "gtkw" },
  };
  static const struct {
    const gchar *haystack;
    const gchar *casefold;
  } failure[] = {
    { "endianness", "z" },
    { "Endianness", "Endj" },
    { "Endianness", "endk" },
  };
  guint priority;
  gboolean r;

  for (guint i = 0; i < G_N_ELEMENTS (success); i++)
    {
      r = ide_completion_fuzzy_match (success[i].haystack,
                                      success[i].casefold,
                                      &priority);
      g_assert_true (r);
    }

  for (guint i = 0; i < G_N_ELEMENTS (failure); i++)
    {
      r = ide_completion_fuzzy_match (failure[i].haystack,
                                      failure[i].casefold,
                                      &priority);
      g_assert_false (r);
    }
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Completion/fuzzy_match", test_fuzzy_match);
  return g_test_run ();
}
