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

#include "ide-clang-service.h"
#include "ide-clang-translation-unit.h"
#include "ide-clang-code-indexer.h"
#include "ide-clang-code-index-entries.h"

/*
 * This class will index a file and returns IdeCodeIndexEntries using which index entries
 * can be retrieved one by one.It will be backed by TU of the file.
 */

struct _IdeClangCodeIndexer
{
  GObject                    parent;

  CXIndex                    index;

  gchar                    **build_flags;
};

static void code_indexer_iface_init (IdeCodeIndexerInterface *iface);

G_DEFINE_TYPE_EXTENDED (IdeClangCodeIndexer, ide_clang_code_indexer, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEXER, code_indexer_iface_init))

static gchar **
ide_clang_code_indexer_get_default_build_flags (IdeClangCodeIndexer *self)
{
  IdeConfigurationManager *manager;
  IdeConfiguration *config;
  IdeContext *context;
  const gchar *cflags;
  const gchar *cxxflags;
  gchar **argv = NULL;

  g_assert (IDE_IS_CLANG_CODE_INDEXER (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (manager);
  cflags = ide_configuration_getenv (config, "CFLAGS");
  cxxflags = ide_configuration_getenv (config, "CXXFLAGS");

  if (cflags && *cflags)
    g_shell_parse_argv (cflags, NULL, &argv, NULL);

  if ((!argv || !*argv) && cxxflags && *cxxflags)
    g_shell_parse_argv (cxxflags, NULL, &argv, NULL);

  if (argv == NULL)
    argv = g_new0 (gchar*, 1);

  return argv;
}

/* This will return a IdeCodeIndexEntries backed by translation unit of file. */
static IdeCodeIndexEntries *
ide_clang_code_indexer_index_file (IdeCodeIndexer      *indexer,
                                   GFile               *file,
                                   gchar              **args,
                                   GCancellable        *cancellable,
                                   GError             **error)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)indexer;
  g_autoptr(GTask) task = NULL;
  g_autofree gchar *filename = NULL;
  CXTranslationUnit tu;
  guint n_args = 0;

  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEXER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  task = g_task_new (self, cancellable, NULL, NULL);

  filename = g_file_get_path (file);

  g_debug ("Indexing %s", filename);

  if (args == NULL)
    {
      if (self->build_flags == NULL)
        self->build_flags = ide_clang_code_indexer_get_default_build_flags (self);

      args  = self->build_flags;
    }

  while (args [n_args] != NULL)
    n_args++;

  if (CXError_Success == clang_parseTranslationUnit2 (self->index,
                                                      filename,
                                                      (const char * const *)args,
                                                      n_args,
                                                      NULL,
                                                      0,
                                                      CXTranslationUnit_DetailedPreprocessingRecord,
                                                      &tu))
    {
      g_autoptr(IdeClangCodeIndexEntries) entries = NULL;

      /* entries has to dispose TU when done with it */
      entries = ide_clang_code_index_entries_new (tu, filename);

      g_task_return_pointer (task, g_steal_pointer (&entries), g_object_unref);
    }
  else
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Unable to create translation unit");
    }

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_code_indexer_generate_key_cb (GObject       *object,
                                        GAsyncResult  *result,
                                        gpointer       user_data)
{
  IdeClangService *service = (IdeClangService *)object;
  g_autoptr(IdeClangTranslationUnit) unit = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *key;
  IdeSourceLocation *location;

  g_assert (IDE_IS_CLANG_SERVICE (service));
  g_assert (G_IS_TASK (task));

  unit = ide_clang_service_get_translation_unit_finish (service, result, &error);

  if (unit == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  location = g_task_get_task_data (task);

  g_clear_error (&error);

  key = ide_clang_translation_unit_generate_key (unit, location);

  if (key == NULL)
    g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Key not found");
  else
    g_task_return_pointer (task, g_steal_pointer (&key), g_free);
}

/* This will get USR of declaration referneced at location by using IdeClangTranslationUnit.*/
static void
ide_clang_code_indexer_generate_key_async (IdeCodeIndexer       *indexer,
                                           IdeSourceLocation    *location,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)indexer;
  IdeContext *context;
  IdeClangService *service;
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_CLANG_CODE_INDEXER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_set_task_data (task,
                        ide_source_location_ref (location),
                        (GDestroyNotify)ide_source_location_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CLANG_SERVICE);

  if (g_task_return_error_if_cancelled (task))
    return;

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
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (task, error);
}

static void
ide_clang_code_indexer_finalize (GObject *object)
{
  IdeClangCodeIndexer *self = (IdeClangCodeIndexer *)object;

  g_clear_pointer (&self->index, clang_disposeIndex);
  g_clear_pointer (&self->build_flags, g_strfreev);

  G_OBJECT_CLASS (ide_clang_code_indexer_parent_class)->finalize (object);
}

static void
ide_clang_code_indexer_class_init (IdeClangCodeIndexerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_clang_code_indexer_finalize;
}

static void
code_indexer_iface_init (IdeCodeIndexerInterface *iface)
{
  iface->index_file = ide_clang_code_indexer_index_file;
  iface->generate_key_async = ide_clang_code_indexer_generate_key_async;
  iface->generate_key_finish = ide_clang_code_indexer_generate_key_finish;
}


static void
ide_clang_code_indexer_init (IdeClangCodeIndexer *self)
{
  self->index = clang_createIndex (0, 0);
}
