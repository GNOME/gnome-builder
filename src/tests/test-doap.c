/* test-doap.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#include <libide-projects.h>

static void
test_load_from_file (void)
{
  IdeDoap *doap;
  GList *list;
  IdeDoapPerson *person;
  GError *error = NULL;
  GFile *file;
  gboolean ret;
  gchar **langs;

  doap = ide_doap_new ();
  g_object_add_weak_pointer (G_OBJECT (doap), (gpointer *)&doap);

  file = g_file_new_for_path (TEST_DATA_DIR"/test.doap");

  ret = ide_doap_load_from_file (doap, file, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_assert_cmpstr (ide_doap_get_name (doap), ==, "Project One");
  g_assert_cmpstr (ide_doap_get_shortdesc (doap), ==, "Short Description of Project1");
  g_assert_cmpstr (ide_doap_get_description (doap), ==, "Long Description");
  g_assert_cmpstr (ide_doap_get_homepage (doap), ==, "https://example.org/");
  g_assert_cmpstr (ide_doap_get_download_page (doap), ==, "https://download.example.org/");
  g_assert_cmpstr (ide_doap_get_bug_database (doap), ==, "https://bugs.example.org/");

  langs = ide_doap_get_languages (doap);
  g_assert (langs != NULL);
  g_assert_cmpstr (langs [0], ==, "C");
  g_assert_cmpstr (langs [1], ==, "JavaScript");
  g_assert_cmpstr (langs [2], ==, "Python");

  list = ide_doap_get_maintainers (doap);
  g_assert (list != NULL);
  g_assert (list->data != NULL);
  g_assert (list->next == NULL);

  person = list->data;
  g_assert_cmpstr (ide_doap_person_get_name (person), ==, "Some Name");
  g_assert_cmpstr (ide_doap_person_get_email (person), ==, "example@example.org");

  g_object_unref (doap);
  g_assert (doap == NULL);

  g_clear_object (&file);
}

gint
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Doap/load_from_file", test_load_from_file);
  return g_test_run ();
}
