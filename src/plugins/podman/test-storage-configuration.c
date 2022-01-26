/* test-storage-configuration.c
 *
 * Copyright 2022 Günther Wagner <info@gunibert.de>
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

#include <glib.h>
#include "gbp-podman-runtime-private.h"

static void
test_toml_parsing (void)
{
  char *parsed = _gbp_podman_runtime_parse_toml_line ((char *)"graphroot = \"/etc/containers/storage.conf\"");
  g_assert_cmpstr (parsed, ==, "/etc/containers/storage.conf");
  g_free (parsed);

  parsed = _gbp_podman_runtime_parse_toml_line ((char *)"graphroot=\"/etc/containers/storage.conf\"");
  g_assert_cmpstr (parsed, ==, "/etc/containers/storage.conf");
  g_free (parsed);
}

static void
test_parse_storage_config (void)
{
  g_autofree char *testfile = g_test_build_filename (G_TEST_DIST, "testdata", "storage.conf", NULL);
  char *path = _gbp_podman_runtime_parse_storage_configuration (testfile, 0);
  g_assert_cmpstr (path, ==, "/var/lib/containers/storage/");
  g_free (path);

  path = _gbp_podman_runtime_parse_storage_configuration (testfile, 1);
  g_assert_cmpstr (path, ==, "/home/user/.local/share/containers/");
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func("/podman/toml_parsing", test_toml_parsing);
  g_test_add_func("/podman/parse_storage_configuration", test_parse_storage_config);

  return g_test_run ();
}
