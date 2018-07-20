/* test-gi-version.c
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "test-ide-gi-version"

#include "test-gi-common.h"
#include "test-gi-utils.h"

#include <ide.h>

#include "application/ide-application-tests.h"

#include "../../../plugins/gnome-builder-plugins.h"

#include "../../../plugins/gi/ide-gi-types.h"
#include "../../../plugins/gi/ide-gi-repository.h"
#include "../../../plugins/gi/ide-gi-objects.h"

#include <string.h>

static IdeGiRepository *global_repository = NULL;

G_LOCK_DEFINE (results);

#define QUERY_LOOPS 100
#define MAX_QUERY_THREADS 4

static void
compare_complete_prefix_results (GPtrArray *results)
{
  GArray *ar_a;
  GArray *ar_b;

  if (results->len < 2)
    return;

  ar_a = g_ptr_array_index (results, 0);
  for (guint i = 1; i < results->len; i++)
    {
      ar_b = g_ptr_array_index (results, i);
      g_assert (ar_a->len == ar_b->len);

      for (guint j = 0; j < ar_a->len; j++)
        {
          IdeGiCompletePrefixItem *item_a = &g_array_index (ar_a, IdeGiCompletePrefixItem, i);
          IdeGiCompletePrefixItem *item_b = &g_array_index (ar_b, IdeGiCompletePrefixItem, i);

          g_assert_cmpstr (item_a->word, ==, item_b->word);
          g_assert (item_a->type == item_b->type);
          g_assert_cmpint (item_a->major_version, ==, item_b->major_version);
          g_assert_cmpint (item_a->minor_version, ==, item_b->minor_version);
          g_assert (item_a->ns == item_b->ns);
        }
    }
}

static void
complete_prefix_func (gpointer data,
                      gpointer user_data)
{
  IdeGiVersion *version = (IdeGiVersion *)user_data;
  GPtrArray *results = (GPtrArray *)data;
  IdeGiPrefixType flags;
  GArray *ar;

  flags = IDE_GI_PREFIX_TYPE_NAMESPACE |
          IDE_GI_PREFIX_TYPE_SYMBOL |
          IDE_GI_PREFIX_TYPE_IDENTIFIER |
          IDE_GI_PREFIX_TYPE_GTYPE |
          IDE_GI_PREFIX_TYPE_PACKAGE;

  for (guint i = 0; i < QUERY_LOOPS - 1; i++)
    {
      G_GNUC_UNUSED g_autoptr(GArray) tmp_ar;
      tmp_ar = ide_gi_version_complete_prefix (version, NULL, flags, FALSE, FALSE, "");
    }

  ar = ide_gi_version_complete_prefix (version, NULL, flags, FALSE, FALSE, "");

  G_LOCK (results);
  g_ptr_array_add (results, ar);
  G_UNLOCK (results);
}

static void
compare_complete_gtype_results (GPtrArray *results)
{
  GArray *ar_a;
  GArray *ar_b;

  if (results->len < 2)
    return;

  ar_a = g_ptr_array_index (results, 0);
  for (guint i = 1; i < results->len; i++)
    {
      ar_b = g_ptr_array_index (results, i);
      g_assert (ar_a->len == ar_b->len);

      for (guint j = 0; j < ar_a->len; j++)
        {
          IdeGiCompleteGtypeItem *item_a = &g_array_index (ar_a, IdeGiCompleteGtypeItem, i);
          IdeGiCompleteGtypeItem *item_b = &g_array_index (ar_b, IdeGiCompleteGtypeItem, i);

          g_assert_cmpstr (item_a->word, ==, item_b->word);
          g_assert (item_a->ns == item_b->ns);
          g_assert_cmpint (item_a->major_version, ==, item_b->major_version);
          g_assert_cmpint (item_a->minor_version, ==, item_b->minor_version);
          g_assert (item_a->object_type == item_b->object_type);
          g_assert (item_a->object_offset == item_b->object_offset);
          g_assert (item_a->is_buildable == item_b->is_buildable);
        }
    }
}

static void
complete_gtype_func (gpointer data,
                     gpointer user_data)
{
  IdeGiVersion *version = (IdeGiVersion *)user_data;
  GPtrArray *results = (GPtrArray *)data;
  GArray *ar;
  IdeGiCompleteRootFlags flags;

  flags = IDE_GI_COMPLETE_ROOT_ALIAS |
          IDE_GI_COMPLETE_ROOT_CLASS |
          IDE_GI_COMPLETE_ROOT_CONSTANT |
          IDE_GI_COMPLETE_ROOT_ENUM |
          IDE_GI_COMPLETE_ROOT_FIELD |
          IDE_GI_COMPLETE_ROOT_FUNCTION |
          IDE_GI_COMPLETE_ROOT_INTERFACE |
          IDE_GI_COMPLETE_ROOT_RECORD |
          IDE_GI_COMPLETE_ROOT_UNION;

  for (guint i = 0; i < QUERY_LOOPS - 1; i++)
    {
      G_GNUC_UNUSED g_autoptr(GArray) tmp_ar;
      tmp_ar = ide_gi_version_complete_gtype (version, NULL, flags, FALSE, "");
    }

  ar = ide_gi_version_complete_gtype (version, NULL, flags, FALSE, "");

  G_LOCK (results);
  g_ptr_array_add (results, ar);
  G_UNLOCK (results);
}

static void
compare_complete_root_objects_results (GPtrArray *results)
{
  GArray *ar_a;
  GArray *ar_b;

  if (results->len < 2)
    return;

  ar_a = g_ptr_array_index (results, 0);
  for (guint i = 1; i < results->len; i++)
    {
      ar_b = g_ptr_array_index (results, i);
      g_assert (ar_a->len == ar_b->len);

      for (guint j = 0; j < ar_a->len; j++)
        {
          IdeGiCompleteObjectItem *item_a = &g_array_index (ar_a, IdeGiCompleteObjectItem, i);
          IdeGiCompleteObjectItem *item_b = &g_array_index (ar_b, IdeGiCompleteObjectItem, i);

          g_assert_cmpstr (item_a->word, ==, item_b->word);
          g_assert (item_a->type == item_b->type);
        }
    }
}

static void
complete_root_objects_func (gpointer data,
                            gpointer user_data)
{
  IdeGiVersion *version = (IdeGiVersion *)user_data;
  GPtrArray *results = (GPtrArray *)data;
  GArray *final_ar;
  g_autoptr(GArray) ns_ar = NULL;
  IdeGiCompleteRootFlags flags;

  flags = IDE_GI_COMPLETE_ROOT_ALIAS |
          IDE_GI_COMPLETE_ROOT_CLASS |
          IDE_GI_COMPLETE_ROOT_CONSTANT |
          IDE_GI_COMPLETE_ROOT_ENUM |
          IDE_GI_COMPLETE_ROOT_FIELD |
          IDE_GI_COMPLETE_ROOT_FUNCTION |
          IDE_GI_COMPLETE_ROOT_INTERFACE |
          IDE_GI_COMPLETE_ROOT_RECORD |
          IDE_GI_COMPLETE_ROOT_UNION;

  final_ar = g_array_new (FALSE, TRUE, sizeof (IdeGiCompleteObjectItem));
  g_array_set_clear_func (final_ar, (GDestroyNotify)ide_gi_complete_object_item_clear);

  ns_ar = ide_gi_version_complete_prefix (version, NULL, IDE_GI_PREFIX_TYPE_NAMESPACE, FALSE, FALSE, "");
  for (guint i =0; i < ns_ar->len; i++)
    {
      g_autoptr (GArray) ar = NULL;
      IdeGiCompletePrefixItem  *item = &g_array_index (ns_ar, IdeGiCompletePrefixItem, i);

      for (guint l = 0; l < QUERY_LOOPS - 1; l++)
        {
          G_GNUC_UNUSED g_autoptr(GArray) tmp_ar;
          tmp_ar = ide_gi_version_complete_root_objects (version, NULL, item->ns, flags, FALSE, "");
        }

      ar = ide_gi_version_complete_root_objects (version, NULL, item->ns, flags, FALSE, "");
      for (guint j = 0; j < ar->len; j++)
        g_array_append_val (final_ar, g_array_index (ar, IdeGiCompleteObjectItem, j));

      /* We want to keep word and object field content alive here */
      g_array_set_clear_func (ar, NULL);
    }

  G_LOCK (results);
  g_ptr_array_add (results, final_ar);
  G_UNLOCK (results);
}

static void
test_threaded_query_cb (gpointer      source_object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GThreadPool *pool;
  g_autoptr(GTask) task = user_data;
  g_autoptr(IdeGiVersion) version = NULL;
  g_autoptr(GError) error = NULL;
  GPtrArray *results;
  G_GNUC_UNUSED guint64 start_time;

  g_assert (G_IS_TASK (task));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  global_repository = test_gi_common_setup_finish (result, &error);
  g_assert (IDE_IS_GI_REPOSITORY (global_repository));

  version = ide_gi_repository_get_current_version (global_repository);
  g_assert (IDE_IS_GI_VERSION (version));

  /* Here we assume that the order of items returned by those functions
   * stay the same between two calls.
   */

  start_time = g_get_monotonic_time ();

  /* Test ide_gi_version_complete_prefix */
  results = g_ptr_array_new_with_free_func ((GDestroyNotify)g_array_unref);
  pool = g_thread_pool_new (complete_prefix_func, version, MAX_QUERY_THREADS, TRUE, &error);
  for (guint i = 1; i <= MAX_QUERY_THREADS; i++)
    g_thread_pool_push (pool, results, &error);

  g_thread_pool_free (pool, FALSE, TRUE);
  compare_complete_prefix_results (results);
  g_ptr_array_unref (results);

  /* Test ide_gi_version_complete_gtype */
  results = g_ptr_array_new_with_free_func ((GDestroyNotify)g_array_unref);
  pool = g_thread_pool_new (complete_gtype_func, version, MAX_QUERY_THREADS, TRUE, &error);
  for (guint i = 1; i <= MAX_QUERY_THREADS; i++)
    g_thread_pool_push (pool, results, &error);

  g_thread_pool_free (pool, FALSE, TRUE);
  compare_complete_gtype_results (results);
  g_ptr_array_unref (results);

  /* Test ide_gi_version_complete_root_objects */
  results = g_ptr_array_new_with_free_func ((GDestroyNotify)g_array_unref);
  pool = g_thread_pool_new (complete_root_objects_func, version, MAX_QUERY_THREADS, TRUE, &error);
  for (guint i = 1; i <= MAX_QUERY_THREADS; i++)
    g_thread_pool_push (pool, results, &error);

  g_thread_pool_free (pool, FALSE, TRUE);
  compare_complete_root_objects_results (results);
  g_ptr_array_unref (results);

  IDE_TRACE_MSG ("query time:%ld", g_get_monotonic_time () - start_time);

  g_task_return_boolean (task, TRUE);
  IDE_EXIT;
}

static void
test_threaded_query_async (GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GTask *task;

  IDE_ENTRY;

  task = g_task_new (NULL, cancellable, callback, user_data);
  test_gi_common_setup_async (cancellable, (GAsyncReadyCallback)test_threaded_query_cb, task);

  IDE_EXIT;
}

gint
main (gint   argc,
      gchar *argv[])
{
  static const gchar *required_plugins[] = { "buildconfig",
                                             "meson-plugin",
                                             "autotools-plugin",
                                             "directory-plugin",
                                             NULL };
  g_autoptr (IdeApplication) app = NULL;
  gboolean ret;

  g_test_init (&argc, &argv, NULL);

  ide_log_init (TRUE, NULL);
  ide_log_set_verbosity (4);

  app = ide_application_new (IDE_APPLICATION_MODE_TESTS | G_APPLICATION_NON_UNIQUE);
  ide_application_add_test (app,
                            "/Gi/repository/threaded_query",
                            test_threaded_query_async,
                            NULL,
                            required_plugins);

  gnome_builder_plugins_init ();
  ret = g_application_run (G_APPLICATION (app), argc, argv);

  g_object_unref (global_repository);

  return ret;
}
