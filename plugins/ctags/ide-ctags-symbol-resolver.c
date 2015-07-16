/* ide-ctags-symbol-resolver.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "ide-ctags-service.h"
#include "ide-ctags-symbol-resolver.h"
#include "ide-internal.h"

struct _IdeCtagsSymbolResolver
{
  IdeObject parent_instance;
};

enum {
  PROP_0,
  LAST_PROP
};

static void symbol_resolver_iface_init (IdeSymbolResolverInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (IdeCtagsSymbolResolver,
                                ide_ctags_symbol_resolver,
                                IDE_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE (IDE_TYPE_SYMBOL_RESOLVER,
                                                       symbol_resolver_iface_init))

static void
ide_ctags_symbol_resolver_lookup_symbol_async (IdeSymbolResolver   *resolver,
                                               IdeSourceLocation   *location,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  IdeContext *context;
  IdeBufferManager *bufmgr;
  IdeCtagsService *service;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GPtrArray) indexes = NULL;
  const gchar *keyword = NULL;
  gsize i;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  context = ide_object_get_context (IDE_OBJECT (self));
  service = ide_context_get_service_typed (context, IDE_TYPE_CTAGS_SERVICE);
  indexes = ide_ctags_service_get_indexes (service);
  bufmgr = ide_context_get_buffer_manager (context);

  for (i = 0; i < indexes->len; i++)
    {
      IdeCtagsIndex *index = g_ptr_array_index (indexes, i);
      const IdeCtagsIndexEntry *entries;
      gsize count;

      entries = ide_ctags_index_lookup (index, keyword, &count);

      if (entries && (count > 0))
        {
        }
    }
}

static IdeSymbol *
ide_ctags_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *resolver,
                                                GAsyncResult       *result,
                                                GError            **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_ctags_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *resolver,
                                                 GFile               *file,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeCtagsSymbolResolver *self = (IdeCtagsSymbolResolver *)resolver;
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /*
   * FIXME: I think the symbol tree should be a separate interface.
   */

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "CTags symbol resolver does not support symbol tree.");
}

static IdeSymbolTree *
ide_ctags_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *resolver,
                                                  GAsyncResult       *result,
                                                  GError            **error)
{
  GTask *task = (GTask *)result;

  g_assert (IDE_IS_CTAGS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

static void
ide_ctags_symbol_resolver_class_init (IdeCtagsSymbolResolverClass *klass)
{
}

static void
ide_ctags_symbol_resolver_class_finalize (IdeCtagsSymbolResolverClass *klass)
{
}

static void
ide_ctags_symbol_resolver_init (IdeCtagsSymbolResolver *resolver)
{
}

static void
symbol_resolver_iface_init (IdeSymbolResolverInterface *iface)
{
  iface->lookup_symbol_async = ide_ctags_symbol_resolver_lookup_symbol_async;
  iface->lookup_symbol_finish = ide_ctags_symbol_resolver_lookup_symbol_finish;
  iface->get_symbol_tree_async = ide_ctags_symbol_resolver_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_ctags_symbol_resolver_get_symbol_tree_finish;
}

void
_ide_ctags_symbol_resolver_register_type (GTypeModule *module)
{
  ide_ctags_symbol_resolver_register_type (module);
}
