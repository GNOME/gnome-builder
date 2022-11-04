/* test-ctags.c
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

#include <gio/gio.h>

#include "ide-ctags-index.h"

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
  g_assert_nonnull (index);
  g_assert_true (IDE_IS_CTAGS_INDEX (index));

  g_assert_cmpint (28, ==, ide_ctags_index_get_size (index));

  entries = ide_ctags_index_lookup (index, "__NOTHING_SHOULD_MATCH_THIS__", &n_entries);
  g_assert_cmpint (n_entries, ==, 0);
  g_assert_null (entries);

  entries = ide_ctags_index_lookup (index, "G_LOG_DOMAIN", &n_entries);
  g_assert_cmpint (n_entries, ==, 1);
  g_assert_nonnull (entries);
  for (i = 0; i < 1; i++)
    g_assert_cmpstr (entries [i].name, ==, "G_LOG_DOMAIN");

  entries = ide_ctags_index_lookup (index, "bug_buddy_init", &n_entries);
  g_assert_cmpint (n_entries, ==, 2);
  g_assert_nonnull (entries);
  for (i = 0; i < 1; i++)
    g_assert_cmpstr (entries [i].name, ==, "bug_buddy_init");

  entries = ide_ctags_index_lookup_prefix (index, "G_DEFINE_", &n_entries);
  g_assert_cmpint (n_entries, ==, 16);
  g_assert_nonnull (entries);
  for (i = 0; i < 16; i++)
    {
      gboolean r = g_str_has_prefix (entries [i].name, "G_DEFINE_");
      g_assert_true (r);
    }

  g_main_loop_quit (main_loop);
}

static void
test_ctags_basic (void)
{
  IdeCtagsIndex *index;
  GFile *test_file;
  gchar *path;

  main_loop = g_main_loop_new (NULL, FALSE);

  path = g_build_filename (TEST_DATA_DIR, "../../plugins/ctags", "test-tags", NULL);
  test_file = g_file_new_for_path (path);

  index = ide_ctags_index_new (test_file, NULL, 0);

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
