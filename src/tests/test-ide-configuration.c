/* test-ide-configuration.c
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#include <ide.h>

static void
test_internal (void)
{
  g_autoptr(IdeConfiguration) configuration = NULL;
  g_autoptr(IdeConfiguration) copy = NULL;
  g_autoptr(GObject) dummy = NULL;

  configuration = g_object_new (IDE_TYPE_CONFIGURATION,
                                "id", "my-configuration",
                                NULL);

  ide_configuration_set_internal_string (configuration, "foo-string", NULL);
  g_assert_cmpstr (ide_configuration_get_internal_string (configuration, "foo-string"), ==, NULL);
  g_assert_cmpint (ide_configuration_get_internal_int (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_int64 (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_boolean (configuration, "foo-string"), ==, FALSE);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-string") == NULL);

  ide_configuration_set_internal_string (configuration, "foo-string", "foo");
  g_assert_cmpstr (ide_configuration_get_internal_string (configuration, "foo-string"), ==, "foo");
  g_assert_cmpint (ide_configuration_get_internal_int (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_int64 (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_boolean (configuration, "foo-string"), ==, FALSE);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-string") == NULL);

  ide_configuration_set_internal_int (configuration, "foo-string", 123);
  g_assert_cmpstr (ide_configuration_get_internal_string (configuration, "foo-string"), ==, NULL);
  g_assert_cmpint (ide_configuration_get_internal_int (configuration, "foo-string"), ==, 123);
  g_assert_cmpint (ide_configuration_get_internal_int64 (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_boolean (configuration, "foo-string"), ==, FALSE);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-string") == NULL);

  ide_configuration_set_internal_int64 (configuration, "foo-string", 123);
  g_assert_cmpstr (ide_configuration_get_internal_string (configuration, "foo-string"), ==, NULL);
  g_assert_cmpint (ide_configuration_get_internal_int (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_int64 (configuration, "foo-string"), ==, 123);
  g_assert_cmpint (ide_configuration_get_internal_boolean (configuration, "foo-string"), ==, FALSE);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-string") == NULL);

  ide_configuration_set_internal_boolean (configuration, "foo-string", TRUE);
  g_assert_cmpstr (ide_configuration_get_internal_string (configuration, "foo-string"), ==, NULL);
  g_assert_cmpint (ide_configuration_get_internal_int (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_int64 (configuration, "foo-string"), ==, 0);
  g_assert_cmpint (ide_configuration_get_internal_boolean (configuration, "foo-string"), ==, TRUE);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-string") == NULL);

  copy = ide_configuration_duplicate (configuration);
  g_assert (copy != NULL);
  g_assert_cmpint (ide_configuration_get_internal_boolean (copy, "foo-string"), ==, TRUE);

  g_object_add_weak_pointer (G_OBJECT (copy), (gpointer *)&copy);
  g_object_unref (copy);
  g_assert (copy == NULL);

  dummy = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_add_weak_pointer (G_OBJECT (dummy), (gpointer *)&dummy);

  ide_configuration_set_internal_object (configuration, "foo-object", dummy);
  g_assert (ide_configuration_get_internal_object (configuration, "foo-object") == dummy);
  g_object_unref (dummy);
  g_assert (dummy != NULL);
  ide_configuration_set_internal_object (configuration, "foo-object", NULL);
  g_assert (dummy == NULL);

  g_object_add_weak_pointer (G_OBJECT (configuration), (gpointer *)&configuration);
  g_object_unref (configuration);
  g_assert (configuration == NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/Configuration/internal", test_internal);
  return g_test_run ();
}
