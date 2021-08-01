/* ide-clang-code-indexer.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-clang-code-indexer"

#include <clang-c/Index.h>

#include "ide-clang-code-index-entries.h"
#include "ide-clang-code-indexer.h"
#include "ide-clang-client.h"

struct _IdeClangCodeIndexer
{
  IdeObject parent;
};

static void code_indexer_iface_init (IdeCodeIndexerInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangCodeIndexer, ide_clang_code_indexer, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEXER, code_indexer_iface_init))

static void
ide_clang_code_indexer_index_file_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GVariant) entries = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *path;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  path = ide_task_get_task_data (task);

  if (!(entries = ide_clang_client_index_file_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task,
                             ide_clang_code_index_entries_new (path, entries),
                             g_object_unref);
}

static void
ide_clang_code_indexer_index_file_async (IdeCodeIndexer      *indexer,
                                         GFile               *file,
                                         const gchar * const *args,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)indexer;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeClangClient) client = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CLANG_CODE_INDEXER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_code_indexer_index_file_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (!g_file_is_native (file))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Only native files are supported");
      return;
    }

  ide_task_set_task_data (task, g_file_get_path (file), g_free);

  context = ide_object_get_context (IDE_OBJECT (self));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);

  ide_clang_client_index_file_async (client,
                                     file,
                                     args,
                                     cancellable,
                                     ide_clang_code_indexer_index_file_cb,
                                     g_steal_pointer (&task));
}

static IdeCodeIndexEntries *
ide_clang_code_indexer_index_file_finish (IdeCodeIndexer  *indexer,
                                          GAsyncResult    *result,
                                          GError         **error)
{
  g_assert (IDE_IS_CLANG_CODE_INDEXER (indexer));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_clang_code_indexer_generate_key_cb (GObject       *object,
                                        GAsyncResult  *result,
                                        gpointer       user_data)
{
  IdeClangClient *client = (IdeClangClient *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *key = NULL;

  g_assert (IDE_IS_CLANG_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(key = ide_clang_client_get_index_key_finish (client, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_pointer (task, g_steal_pointer (&key), g_free);
}

static void
ide_clang_code_indexer_generate_key_async (IdeCodeIndexer       *indexer,
                                           IdeLocation    *location,
                                           const gchar * const  *args,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)indexer;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeClangClient) client = NULL;
  IdeContext *context;
  GFile *file;
  guint line;
  guint column;

  g_assert (IDE_IS_CLANG_CODE_INDEXER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_code_indexer_generate_key_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  context = ide_object_get_context (IDE_OBJECT (self));
  client = ide_object_ensure_child_typed (IDE_OBJECT (context), IDE_TYPE_CLANG_CLIENT);

  file = ide_location_get_file (location);
  line = ide_location_get_line (location);
  column = ide_location_get_line_offset (location);

  ide_clang_client_get_index_key_async (client,
                                        file,
                                        args,
                                        line + 1,
                                        column + 1,
                                        cancellable,
                                        ide_clang_code_indexer_generate_key_cb,
                                        g_steal_pointer (&task));
}

static gchar *
ide_clang_code_indexer_generate_key_finish (IdeCodeIndexer  *self,
                                            GAsyncResult    *result,
                                            GError         **error)
{
  g_assert (IDE_IS_CODE_INDEXER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
code_indexer_iface_init (IdeCodeIndexerInterface *iface)
{
  iface->index_file_async = ide_clang_code_indexer_index_file_async;
  iface->index_file_finish = ide_clang_code_indexer_index_file_finish;
  iface->generate_key_async = ide_clang_code_indexer_generate_key_async;
  iface->generate_key_finish = ide_clang_code_indexer_generate_key_finish;
}

static void
ide_clang_code_indexer_class_init (IdeClangCodeIndexerClass *klass)
{
}

static void
ide_clang_code_indexer_init (IdeClangCodeIndexer *self)
{
}
