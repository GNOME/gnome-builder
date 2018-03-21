/* ide-code-index-symbol-resolver.c
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

#define G_LOG_DOMAIN "code-index-symbol-resolver"

#include "ide-code-index-service.h"
#include "ide-code-index-symbol-resolver.h"

static void
ide_code_index_symbol_resolver_lookup_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeCodeIndexer *code_indexer = (IdeCodeIndexer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *key = NULL;
  IdeCodeIndexSymbolResolver *self;
  IdeCodeIndexService *service;
  IdeCodeIndexIndex *index;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEXER (code_indexer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CODE_INDEX_SYMBOL_RESOLVER (self));

  if (!(key = ide_code_indexer_generate_key_finish (code_indexer, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  service = ide_context_get_service_typed (context, IDE_TYPE_CODE_INDEX_SERVICE);
  g_assert (IDE_IS_CODE_INDEX_SERVICE (service));

  index = ide_code_index_service_get_index (service);
  g_assert (IDE_IS_CODE_INDEX_INDEX (index));

  symbol = ide_code_index_index_lookup_symbol (index, key);

  if (symbol != NULL)
    ide_task_return_pointer (task,
                             g_steal_pointer (&symbol),
                             (GDestroyNotify)ide_symbol_unref);
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to locate symbol \"%s\"", key);
}

static void
ide_code_index_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                                    IdeSourceLocation   *location,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  IdeCodeIndexSymbolResolver *self = (IdeCodeIndexSymbolResolver *)resolver;
  g_autoptr(IdeTask) task = NULL;
  IdeCodeIndexService *service;
  IdeCodeIndexer *code_indexer;
  const gchar *path;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_code_index_symbol_resolver_lookup_symbol_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  g_assert (IDE_IS_CONTEXT (context));

  service = ide_context_get_service_typed (context, IDE_TYPE_CODE_INDEX_SERVICE);
  g_assert (IDE_IS_CODE_INDEX_SERVICE (service));

  path = ide_file_get_path (ide_source_location_get_file (location));
  g_assert (path != NULL);

  code_indexer = ide_code_index_service_get_code_indexer (service, path);
  g_assert (!code_indexer || IDE_IS_CODE_INDEXER (code_indexer));

  if (code_indexer == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Failed to lcoate code indexer");
  else
    ide_code_indexer_generate_key_async (code_indexer,
                                         location,
                                         cancellable,
                                         ide_code_index_symbol_resolver_lookup_cb,
                                         g_steal_pointer (&task));
}
static IdeSymbol *
ide_code_index_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                     GAsyncResult       *result,
                                                     GError            **error)
{
  g_assert (IDE_IS_CODE_INDEX_SYMBOL_RESOLVER (resolver));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_code_index_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_code_index_symbol_resolver_lookup_symbol_finish;
}

struct _IdeCodeIndexSymbolResolver { IdeObject parent_instance; };

G_DEFINE_TYPE_WITH_CODE (IdeCodeIndexSymbolResolver, ide_code_index_symbol_resolver, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER, symbol_resolver_iface_init))

static void
ide_code_index_symbol_resolver_init (IdeCodeIndexSymbolResolver *self)
{
}

static void
ide_code_index_symbol_resolver_class_init (IdeCodeIndexSymbolResolverClass *self)
{
}
