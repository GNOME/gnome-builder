/* ide-code-indexer.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-code-indexer"

#include "config.h"

#include "ide-code-indexer.h"
#include "ide-location.h"

/**
 * SECTION:ide-code-indexer
 * @title: IdeCodeIndexer
 * @short_description: Interface for background indexing source code
 *
 * The #IdeCodeIndexer interface is used to index source code in the project.
 * Plugins that want to provide global search features for source code should
 * implement this interface and specify which languages they support in their
 * .plugin definition, using "X-Code-Indexer-Languages". For example. to index
 * Python source code, you might use:
 *
 *   X-Code-Indexer-Languages=python,python3
 */

G_DEFINE_INTERFACE (IdeCodeIndexer, ide_code_indexer, IDE_TYPE_OBJECT)

static void
ide_code_indexer_real_index_file_async (IdeCodeIndexer      *self,
                                        GFile               *file,
                                        const gchar * const *build_flags,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_assert (IDE_IS_CODE_INDEXER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_code_indexer_real_index_file_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Get key is not supported");
}

static IdeCodeIndexEntries *
ide_code_indexer_real_index_file_finish (IdeCodeIndexer  *self,
                                         GAsyncResult    *result,
                                         GError         **error)
{
  g_assert (IDE_IS_CODE_INDEXER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_code_indexer_real_generate_key_async (IdeCodeIndexer      *self,
                                          IdeLocation   *location,
                                          const gchar * const *build_flags,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_assert (IDE_IS_CODE_INDEXER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self, callback, user_data,
                           ide_code_indexer_real_generate_key_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Get key is not supported");
}

static gchar *
ide_code_indexer_real_generate_key_finish (IdeCodeIndexer  *self,
                                           GAsyncResult    *result,
                                           GError         **error)
{
  g_assert (IDE_IS_CODE_INDEXER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_code_indexer_default_init (IdeCodeIndexerInterface *iface)
{
  iface->index_file_async = ide_code_indexer_real_index_file_async;
  iface->index_file_finish = ide_code_indexer_real_index_file_finish;
  iface->generate_key_async = ide_code_indexer_real_generate_key_async;
  iface->generate_key_finish = ide_code_indexer_real_generate_key_finish;
}

/**
 * ide_code_indexer_index_file_async:
 * @self: An #IdeCodeIndexer instance.
 * @file: Source file to index.
 * @build_flags: (nullable) (array zero-terminated=1): array of build flags to parse @file.
 * @cancellable: (nullable): a #GCancellable.
 * @callback: a #GAsyncReadyCallback
 * @user_data: closure data for @callback
 *
 * This function will take index source file and create an array of symbols in
 * @file. @callback is called upon completion and must call
 * ide_code_indexer_index_file_finish() to complete the operation.
 */
void
ide_code_indexer_index_file_async (IdeCodeIndexer      *self,
                                   GFile               *file,
                                   const gchar * const *build_flags,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
#ifdef IDE_ENABLE_TRACE
  g_autoptr(GFile) copy = NULL;
#endif

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CODE_INDEXER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

#ifdef IDE_ENABLE_TRACE
  /* Simplify leak detection */
  file = copy = g_file_dup (file);
#endif

  return IDE_CODE_INDEXER_GET_IFACE (self)->index_file_async (self, file, build_flags, cancellable, callback, user_data);
}

/**
 * ide_code_indexer_index_file_finish:
 * @self: a #IdeCodeIndexer
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_code_indexer_index_file_async().
 *
 * Returns: (transfer full): an #IdeCodeIndexEntries if successful; otherwise %NULL
 *   and @error is set.
 */
IdeCodeIndexEntries *
ide_code_indexer_index_file_finish (IdeCodeIndexer  *self,
                                    GAsyncResult    *result,
                                    GError         **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEXER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_CODE_INDEXER_GET_IFACE (self)->index_file_finish (self, result, error);
}

/**
 * ide_code_indexer_generate_key_async:
 * @self: An #IdeCodeIndexer instance.
 * @location: (not nullable): Source location of refernece.
 * @build_flags: (nullable) (array zero-terminated=1): array of build flags to parse @file.
 * @cancellable: (nullable): a #GCancellable.
 * @callback: A callback to execute upon indexing.
 * @user_data: User data to pass to @callback.
 *
 * This function will get key of reference located at #IdeSoureLocation.
 *
 * In 3.30 this function gained the @build_flags parameter.
 */
void
ide_code_indexer_generate_key_async (IdeCodeIndexer      *self,
                                     IdeLocation         *location,
                                     const gchar * const *build_flags,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CODE_INDEXER (self));
  g_return_if_fail (IDE_IS_LOCATION (location));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_CODE_INDEXER_GET_IFACE (self)->generate_key_async (self, location, build_flags, cancellable, callback, user_data);
}

/**
 * ide_code_indexer_generate_key_finish:
 * @self: an #IdeCodeIndexer
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Returns key for declaration of reference at a location.
 *
 * Returns: (transfer full) : A string which contains key.
 */
gchar *
ide_code_indexer_generate_key_finish (IdeCodeIndexer  *self,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEXER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_CODE_INDEXER_GET_IFACE (self)->generate_key_finish (self, result, error);
}
