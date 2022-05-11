/* ide-similar-file-locator.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-similar-file-locator"

#include "config.h"

#include "ide-similar-file-locator.h"

G_DEFINE_INTERFACE (IdeSimilarFileLocator, ide_similar_file_locator, G_TYPE_OBJECT)

static void
ide_similar_file_locator_default_init (IdeSimilarFileLocatorInterface *iface)
{
}

/**
 * ide_similar_file_locator_list_async:
 * @self: a #IdeSimilarFileLocator
 * @file: a #GFile to find similar files for
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: callback to use to complete operation
 * @user_data: closure data for @callback
 *
 * Asynchronously requests locating similar files.
 *
 * A similar file may be found such as those with similar file suffixes
 * or perhaps a designer file associated with a source file.
 */
void
ide_similar_file_locator_list_async (IdeSimilarFileLocator *self,
                                     GFile                 *file,
                                     GCancellable          *cancellable,
                                     GAsyncReadyCallback    callback,
                                     gpointer               user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SIMILAR_FILE_LOCATOR (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SIMILAR_FILE_LOCATOR_GET_IFACE (self)->list_async (self, file, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_similar_file_locator_list_finish:
 * @self: a #IdeSimilarFileLocator
 * @result: a #GAsyncResult
 * @error: location for a #GError, or %NULL
 *
 * Completes asynchronous request to list similar files.
 *
 * Returns: (transfer full): a #GListModel of #GFile or %NULL
 */
GListModel *
ide_similar_file_locator_list_finish (IdeSimilarFileLocator  *self,
                                      GAsyncResult           *result,
                                      GError                **error)
{
  GListModel *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SIMILAR_FILE_LOCATOR (self), NULL);

  ret = IDE_SIMILAR_FILE_LOCATOR_GET_IFACE (self)->list_finish (self, result, error);

  IDE_RETURN (ret);
}
