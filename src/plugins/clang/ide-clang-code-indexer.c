/* ide-clang-code-indexer.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-clang-code-indexer"

#include <clang-c/Index.h>

#include "ide-clang-code-index-entries.h"
#include "ide-clang-code-indexer.h"
#include "ide-clang-private.h"
#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"

typedef struct
{
  GFile  *file;
  gchar **build_flags;
} BuildRequest;

static void
build_request_free (gpointer data)
{
  BuildRequest *br = data;

  g_clear_object (&br->file);
  g_clear_pointer (&br->build_flags, g_strfreev);
  g_slice_free (BuildRequest, br);
}

static void
ide_clang_code_indexer_index_file_worker (IdeTask      *task,
                                          gpointer      source_object,
                                          gpointer      task_data,
                                          GCancellable *cancellable)
{
  BuildRequest *br = task_data;
  g_auto(CXTranslationUnit) unit = NULL;
  g_auto(CXIndex) index = NULL;
  g_autofree gchar *path = NULL;
  enum CXErrorCode code;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG_CODE_INDEXER (source_object));
  g_assert (br != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  path = g_file_get_path (br->file);
  index = clang_createIndex (0, 0);
  code = clang_parseTranslationUnit2 (index,
                                      path,
                                      (const char * const *)br->build_flags,
                                      br->build_flags ? g_strv_length (br->build_flags) : 0,
                                      NULL,
                                      0,
                                      (CXTranslationUnit_DetailedPreprocessingRecord |
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 43)
                                       CXTranslationUnit_SingleFileParse |
#endif
#if CINDEX_VERSION >= CINDEX_VERSION_ENCODE(0, 35)
                                       CXTranslationUnit_KeepGoing |
#endif
                                       CXTranslationUnit_SkipFunctionBodies),
                                      &unit);

  if (code != CXError_Success)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to index \"%s\"",
                               path);
  else
    ide_task_return_pointer (task,
                             ide_clang_code_index_entries_new (g_steal_pointer (&index),
                                                               g_steal_pointer (&unit),
                                                               path),
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
  BuildRequest *br;

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

  br = g_slice_new0 (BuildRequest);
  br->build_flags = g_strdupv ((gchar **)args);
  br->file = g_object_ref (file);
  ide_task_set_task_data (task, br, build_request_free);

  ide_task_run_in_thread (task, ide_clang_code_indexer_index_file_worker);
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
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *key = NULL;
  IdeSourceLocation *location;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(unit = ide_clang_service_get_translation_unit_finish (service, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  location = ide_task_get_task_data (task);
  g_assert (location != NULL);

  if (!(key = ide_clang_translation_unit_generate_key (unit, location)))
    ide_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Key not found");
  else
    ide_task_return_pointer (task, g_steal_pointer (&key), g_free);
}

static void
ide_clang_code_indexer_generate_key_async (IdeCodeIndexer       *indexer,
                                           IdeSourceLocation    *location,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)indexer;
  g_autoptr(IdeTask) task = NULL;
  IdeClangService *service;
  IdeContext *context;

  g_return_if_fail (IDE_IS_CLANG_CODE_INDEXER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * The key to generate is what clang calls a "USR". That is a stable key that
   * can be referenced across compilation units.
   */

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_code_indexer_generate_key_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  ide_task_set_task_data (task,
                          ide_source_location_ref (location),
                          (GDestroyNotify)ide_source_location_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);
  g_assert (IDE_IS_CLANG_SERVICE (service));

  ide_clang_service_get_translation_unit_async (service,
                                                ide_source_location_get_file (location),
                                                0,
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

struct _IdeClangCodeIndexer { IdeObject parent; };

G_DEFINE_TYPE_WITH_CODE (IdeClangCodeIndexer, ide_clang_code_indexer, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEXER, code_indexer_iface_init))

static void
ide_clang_code_indexer_class_init (IdeClangCodeIndexerClass *klass)
{
}

static void
ide_clang_code_indexer_init (IdeClangCodeIndexer *self)
{
}
