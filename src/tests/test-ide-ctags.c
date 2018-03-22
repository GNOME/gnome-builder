/* test-ide-ctags.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "ctags/ide-ctags-index.h"

static GMainLoop *main_loop;

static void
init_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  GAsyncInitable *initable = (GAsyncInitable *)object;
  IdeCtagsIndex *index = (IdeCtagsIndex *)object;
  const IdeCtagsIndexEntry *entries;
  gsize n_entries = 0xFFFFFFFF;
  GError *error = NULL;
  gboolean ret;
  gsize i;

  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));

  ret = g_async_initable_init_finish (initable, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);
  g_assert (index != NULL);
  g_assert (IDE_IS_CTAGS_INDEX (index));

  g_assert_cmpint (815, ==, ide_ctags_index_get_size (index));

  entries = ide_ctags_index_lookup (index, "__NOTHING_SHOULD_MATCH_THIS__", &n_entries);
  g_assert_cmpint (n_entries, ==, 0);
  g_assert (entries == NULL);

  entries = ide_ctags_index_lookup (index, "IdeBuildResult", &n_entries);
  g_assert_cmpint (n_entries, ==, 2);
  g_assert (entries != NULL);
  for (i = 0; i < 2; i++)
    g_assert_cmpstr (entries [i].name, ==, "IdeBuildResult");

  entries = ide_ctags_index_lookup (index, "IdeDiagnosticProvider.functions", &n_entries);
  g_assert_cmpint (n_entries, ==, 1);
  g_assert (entries != NULL);
  g_assert_cmpstr (entries->name, ==, "IdeDiagnosticProvider.functions");
  g_assert_cmpint (entries->kind, ==, IDE_CTAGS_INDEX_ENTRY_ANCHOR);

  entries = ide_ctags_index_lookup_prefix (index, "Ide", &n_entries);
  g_assert_cmpint (n_entries, ==, 815);
  g_assert (entries != NULL);
  for (i = 0; i < 815; i++)
    g_assert (g_str_has_prefix (entries [i].name, "Ide"));

  g_main_loop_quit (main_loop);
}

static void
test_ctags_basic (void)
{
  IdeCtagsIndex *index;
  GFile *test_file;
  gchar *path;

  main_loop = g_main_loop_new (NULL, FALSE);

  path = g_build_filename (TEST_DATA_DIR, "project1", "tags", NULL);
  test_file = g_file_new_for_path (path);

  index = ide_ctags_index_new (test_file);

  g_async_initable_init_async (G_ASYNC_INITABLE (index),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               init_cb,
                               NULL);

  g_main_loop_run (main_loop);

  g_object_unref (index);
  g_free (path);
  g_object_unref (test_file);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Ide/CTags/basic", test_ctags_basic);
  return g_test_run ();
}
