/* test-cpack.c
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#include "c-parse-helper.h"

static void
test_parse_parameters1 (void)
{
  gsize i;
  GSList *ret;
  GSList *iter;

  static const struct
  {
    const gchar *type;
    guint n_star;
    const gchar *name;
    guint ellipsis;
  } result[]=
  {
    { "Item", 1, "a", 0 },
    { "Item", 2, "b", 0 },
    { "gpointer", 0, "u", 0 },
    { "GError", 2, "error", 0 },
    { NULL, 0, NULL, 1}
  };

  ret = parse_parameters ("Item *a , Item **b, gpointer u, GError ** error, ...");
  g_assert_cmpint (5, ==, g_slist_length (ret));

  for (i = 0, iter = ret; i < G_N_ELEMENTS (result); i++, iter = iter->next)
    {
      Parameter *p;

      p = iter->data;
      g_assert_cmpstr (p->type, ==, result[i].type);
      g_assert_cmpint (p->n_star, ==, result[i].n_star);
      g_assert_cmpstr (p->name, ==, result[i].name);
      g_assert_cmpint (p->ellipsis, ==, result[i].ellipsis);
    }

  g_assert (!iter);

  g_slist_foreach (ret, (GFunc)parameter_free, NULL);
  g_slist_free (ret);
}

static void
test_parse_parameters2 (void)
{
  GSList *ret;

  ret = parse_parameters ("abc, def, ghi");
  g_assert (!ret);
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Parser/C/parse_parameters1", test_parse_parameters1);
  g_test_add_func ("/Parser/C/parse_parameters2", test_parse_parameters2);
  return g_test_run ();
}
