/* test-ide-uri.c
 *
 * Copyright 2016 Christian Hergert <christian@hergert.me>
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
#include <string.h>

static void
test_uri_file (void)
{
  static struct {
    const gchar *input;
    const gchar *output;
  } uris[] = {
    { "file:///tmp/foo.txt", "file:///tmp/foo.txt" },
    { "file:///tmp/foo.txt#a=1", "file:///tmp/foo.txt#a=1" },
    { "file:///tmp", "file:///tmp" },
#if 0 /* Broken */
    { "file:///tmp/foo/var///baz", "file:///tmp/foo/var/baz" },
#endif
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (uris); i++)
    {
      GError *error = NULL;
      g_autoptr(IdeUri) uri = ide_uri_new (uris[i].input, 0, &error);

      g_assert_no_error (error);

      if (uris[i].output)
        {
          g_autofree gchar *str = NULL;

          g_assert (uri != NULL);

          str = ide_uri_to_string (uri, 0);
          g_assert_cmpstr (str, ==, uris[i].output);
        }
    }

  /* Test creation from file, when there is no # fragment */
  for (i = 0; i < G_N_ELEMENTS (uris); i++)
    {
      if (uris[i].output && !strchr (uris[i].input, '#'))
        {
          g_autoptr(GFile) file = g_file_new_for_uri (uris[i].input);
          g_autoptr(IdeUri) uri = ide_uri_new_from_file (file);
          g_autofree gchar *str = NULL;

          g_assert (uri != NULL);

          str = ide_uri_to_string (uri, 0);
          g_assert_cmpstr (str, ==, uris[i].output);
        }
    }
}

static void
test_uri_sftp (void)
{
  static struct {
    const gchar *input;
    const gchar *output;
  } uris[] = {
    { "sftp://127.0.0.1:1234/foo/bar/#baz", "sftp://127.0.0.1:1234/foo/bar/#baz" },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (uris); i++)
    {
      GError *error = NULL;
      g_autoptr(IdeUri) uri = ide_uri_new (uris[i].input, 0, &error);

      g_assert_no_error (error);

      if (uris[i].output)
        {
          g_autofree gchar *str = NULL;

          g_assert (uri != NULL);

          str = ide_uri_to_string (uri, 0);
          g_assert_cmpstr (str, ==, uris[i].output);
        }
    }
}

static void
test_uri_smb (void)
{
  static struct {
    const gchar *input;
    const gchar *output;
  } uris[] = {
    { "smb://homie/foo/bar/", "smb://homie/foo/bar/" },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (uris); i++)
    {
      GError *error = NULL;
      g_autoptr(IdeUri) uri = ide_uri_new (uris[i].input, 0, &error);

      g_assert_no_error (error);

      if (uris[i].output)
        {
          g_autofree gchar *str = NULL;

          g_assert (uri != NULL);

          str = ide_uri_to_string (uri, 0);
          g_assert_cmpstr (str, ==, uris[i].output);
        }
    }
}

gint
main (int argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Ide/Uri/file", test_uri_file);
  g_test_add_func ("/Ide/Uri/sftp", test_uri_sftp);
  g_test_add_func ("/Ide/Uri/smb", test_uri_smb);

  return g_test_run ();
}
