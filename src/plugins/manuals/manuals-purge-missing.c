/* manuals-purge-missing.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-book.h"
#include "manuals-gom.h"
#include "manuals-heading.h"
#include "manuals-keyword.h"
#include "manuals-purge-missing.h"

struct _ManualsPurgeMissing
{
  ManualsImporter parent_instance;
};

G_DEFINE_FINAL_TYPE (ManualsPurgeMissing, manuals_purge_missing, MANUALS_TYPE_IMPORTER)

static DexFuture *
manuals_purge_missing_import_fiber (gpointer data)
{
  ManualsRepository *repository = data;
  g_autoptr(GListModel) books = NULL;
  g_autoptr(GListModel) sdks = NULL;

  g_assert (MANUALS_IS_REPOSITORY (repository));

  books = dex_await_object (manuals_repository_list (repository, MANUALS_TYPE_BOOK, NULL), NULL);

  if (books != NULL)
    {
      guint n_items = g_list_model_get_n_items (books);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(ManualsBook) book = g_list_model_get_item (books, i);
          const char *uri = manuals_book_get_uri (book);
          g_autoptr(GFile) file = g_file_new_for_uri (uri);
          g_auto(GValue) book_id = G_VALUE_INIT;
          g_autoptr(GomFilter) book_id_filter = NULL;

          if (dex_await_boolean (dex_file_query_exists (file), NULL))
            continue;

          g_value_init (&book_id, G_TYPE_INT64);
          g_value_set_int64 (&book_id, manuals_book_get_id (book));

          book_id_filter = gom_filter_new_eq (MANUALS_TYPE_KEYWORD, "book-id", &book_id);
          dex_await (manuals_repository_delete (repository,
                                                MANUALS_TYPE_KEYWORD,
                                                book_id_filter),
                     NULL);
          g_clear_object (&book_id_filter);

          book_id_filter = gom_filter_new_eq (MANUALS_TYPE_HEADING, "book-id", &book_id);
          dex_await (manuals_repository_delete (repository,
                                                MANUALS_TYPE_HEADING,
                                                book_id_filter),
                     NULL);
          g_clear_object (&book_id_filter);

          dex_await (gom_resource_delete (GOM_RESOURCE (book)), NULL);
        }
    }

  sdks = dex_await_object (manuals_repository_list (repository, MANUALS_TYPE_SDK, NULL), NULL);

  if (sdks != NULL)
    {
      guint n_items = g_list_model_get_n_items (sdks);

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(ManualsSdk) sdk = g_list_model_get_item (G_LIST_MODEL (sdks), i);
          g_auto(GValue) sdk_id = G_VALUE_INIT;
          g_autoptr(GomFilter) sdk_filter = NULL;
          g_autoptr(GError) error = NULL;
          guint count;

          g_value_init (&sdk_id, G_TYPE_INT64);
          g_value_set_int64 (&sdk_id, manuals_sdk_get_id (sdk));

          sdk_filter = gom_filter_new_eq (MANUALS_TYPE_BOOK, "sdk-id", &sdk_id);

          count = dex_await_uint (manuals_repository_count (repository, MANUALS_TYPE_BOOK, sdk_filter), &error);

          if (error == NULL && count == 0)
            {
              g_autoptr(GomFilter) id_filter = gom_filter_new_eq (MANUALS_TYPE_SDK, "id", &sdk_id);
              dex_await (manuals_repository_delete (repository, MANUALS_TYPE_SDK, id_filter), NULL);
            }
        }
    }

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
manuals_purge_missing_import (ManualsImporter   *importer,
                              ManualsRepository *repository,
                              ManualsProgress   *progress)
{
  g_assert (MANUALS_IS_PURGE_MISSING (importer));
  g_assert (MANUALS_IS_REPOSITORY (repository));
  g_assert (MANUALS_IS_PROGRESS (progress));

  return dex_scheduler_spawn (dex_thread_pool_scheduler_get_default (),
                              0,
                              manuals_purge_missing_import_fiber,
                              g_object_ref (repository),
                              g_object_unref);
}

static void
manuals_purge_missing_class_init (ManualsPurgeMissingClass *klass)
{
  ManualsImporterClass *importer_class = MANUALS_IMPORTER_CLASS (klass);

  importer_class->import = manuals_purge_missing_import;
}

static void
manuals_purge_missing_init (ManualsPurgeMissing *self)
{
}

ManualsImporter *
manuals_purge_missing_new (void)
{
  return g_object_new (MANUALS_TYPE_PURGE_MISSING, NULL);
}
