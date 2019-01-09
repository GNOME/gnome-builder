/* test-vcs-uri.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#include <libide-vcs.h>

typedef struct
{
  const gchar *uri;
  const gchar *expected_scheme;
  const gchar *expected_user;
  const gchar *expected_host;
  const gchar *expected_path;
  guint        expected_port;
  const gchar *canonical;
} UriTest;

static void
test_sample_uris (void)
{
  static const UriTest sample_uris[] = {
    { "ssh://user@host.xz:22/path/to/repo.git/", "ssh", "user", "host.xz", "/path/to/repo.git/", 22, "ssh://user@host.xz:22/path/to/repo.git/" },
    { "ssh://user@host.xz/path/to/repo.git/", "ssh", "user", "host.xz", "/path/to/repo.git/", 0, "ssh://user@host.xz/path/to/repo.git/" },
    { "ssh://host.xz:1234/path/to/repo.git/", "ssh", NULL,"host.xz", "/path/to/repo.git/", 1234, "ssh://host.xz:1234/path/to/repo.git/" },
    { "ssh://host.xz/path/to/repo.git/", "ssh", NULL,"host.xz", "/path/to/repo.git/", 0, "ssh://host.xz/path/to/repo.git/" },
    { "ssh://user@host.xz/path/to/repo.git/", "ssh", "user","host.xz", "/path/to/repo.git/", 0, "ssh://user@host.xz/path/to/repo.git/" },
    { "ssh://host.xz/path/to/repo.git/", "ssh", NULL,"host.xz", "/path/to/repo.git/", 0, "ssh://host.xz/path/to/repo.git/" },
    { "ssh://user@host.xz/~user/path/to/repo.git/", "ssh", "user", "host.xz", "~user/path/to/repo.git/", 0, "ssh://user@host.xz/~user/path/to/repo.git/" },
    { "ssh://host.xz/~user/path/to/repo.git/", "ssh", NULL,"host.xz", "~user/path/to/repo.git/", 0, "ssh://host.xz/~user/path/to/repo.git/" },
    { "ssh://user@host.xz/~/path/to/repo.git", "ssh", "user", "host.xz", "~/path/to/repo.git",0, "ssh://user@host.xz/~/path/to/repo.git" },
    { "ssh://host.xz/~/path/to/repo.git", "ssh", NULL, "host.xz", "~/path/to/repo.git", 0, "ssh://host.xz/~/path/to/repo.git" },
    { "user@host.xz:/path/to/repo.git/", "ssh", "user", "host.xz", "/path/to/repo.git/", 0, "user@host.xz:/path/to/repo.git/" },
    { "host.xz:/path/to/repo.git/", "ssh", NULL, "host.xz", "/path/to/repo.git/", 0, "host.xz:/path/to/repo.git/" },
    { "user@host.xz:~user/path/to/repo.git/", "ssh", "user", "host.xz", "~user/path/to/repo.git/", 0, "user@host.xz:~user/path/to/repo.git/" },
    { "host.xz:~user/path/to/repo.git/", "ssh", NULL, "host.xz", "~user/path/to/repo.git/", 0, "host.xz:~user/path/to/repo.git/" },
    { "user@host.xz:path/to/repo.git", "ssh", "user", "host.xz", "~/path/to/repo.git", 0, "user@host.xz:path/to/repo.git" },
    { "host.xz:path/to/repo.git", "ssh", NULL, "host.xz", "~/path/to/repo.git", 0, "host.xz:path/to/repo.git" },
    { "rsync://host.xz/path/to/repo.git/", "rsync", NULL, "host.xz", "/path/to/repo.git/", 0, "rsync://host.xz/path/to/repo.git/" },
    { "git://host.xz/path/to/repo.git/", "git", NULL, "host.xz", "/path/to/repo.git/", 0, "git://host.xz/path/to/repo.git/" },
    { "git://host.xz/~user/path/to/repo.git/", "git", NULL, "host.xz", "~user/path/to/repo.git/", 0, "git://host.xz/~user/path/to/repo.git/" },
    { "http://host.xz/path/to/repo.git/", "http", NULL, "host.xz", "/path/to/repo.git/", 0, "http://host.xz/path/to/repo.git/" },
    { "https://host.xz/path/to/repo.git/", "https", NULL, "host.xz", "/path/to/repo.git/", 0, "https://host.xz/path/to/repo.git/" },
    { "/path/to/repo.git/", "file", NULL, NULL, "/path/to/repo.git/", 0, "/path/to/repo.git/" },
    { "path/to/repo.git/", "file", NULL, NULL, "path/to/repo.git/", 0, "path/to/repo.git/" },
    { "~/path/to/repo.git", "file", NULL, NULL, "~/path/to/repo.git", 0, "~/path/to/repo.git" },
    { "file:///path/to/repo.git/", "file", NULL, NULL, "/path/to/repo.git/", 0, "file:///path/to/repo.git/" },
    { "file://~/path/to/repo.git/", "file", NULL, NULL, "~/path/to/repo.git/", 0, "file://~/path/to/repo.git/" },
    { "git@github.com:example/example.git", "ssh", "git", "github.com", "~/example/example.git", 0, "git@github.com:example/example.git" },
    { NULL }
  };
  guint i;

  for (i = 0; sample_uris [i].uri; i++)
    {
      g_autoptr(IdeVcsUri) uri = NULL;
      g_autofree gchar *to_string = NULL;

      uri = ide_vcs_uri_new (sample_uris [i].uri);

      if (uri == NULL)
        g_error ("Failed to parse %s\n", sample_uris [i].uri);

#if 0
      g_print ("\n%s (%u)\n"
               "  scheme: %s\n"
               "    user: %s\n"
               "    host: %s\n"
               "    port: %u\n"
               "    path: %s\n",
               sample_uris [i].uri, i,
               ide_vcs_uri_get_scheme (uri),
               ide_vcs_uri_get_user (uri),
               ide_vcs_uri_get_host (uri),
               ide_vcs_uri_get_port (uri),
               ide_vcs_uri_get_path (uri));
#endif

      g_assert (uri != NULL);
      g_assert_cmpstr (sample_uris [i].expected_scheme, ==, ide_vcs_uri_get_scheme (uri));
      g_assert_cmpstr (sample_uris [i].expected_user, ==, ide_vcs_uri_get_user (uri));
      g_assert_cmpstr (sample_uris [i].expected_host, ==, ide_vcs_uri_get_host (uri));
      g_assert_cmpstr (sample_uris [i].expected_path, ==, ide_vcs_uri_get_path (uri));
      g_assert_cmpint (sample_uris [i].expected_port, ==, ide_vcs_uri_get_port (uri));

      to_string = ide_vcs_uri_to_string (uri);
      g_assert_cmpstr (sample_uris [i].canonical, ==, to_string);
    }
}

gint
main (gint argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/VcsUri/sample_uris", test_sample_uris);
  return g_test_run ();
}
