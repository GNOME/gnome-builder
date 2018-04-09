/* ide-symbol-resolver.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-symbol-resolver"

#include "ide-context.h"

#include "buffers/ide-buffer.h"
#include "files/ide-file.h"
#include "symbols/ide-symbol-resolver.h"

G_DEFINE_INTERFACE (IdeSymbolResolver, ide_symbol_resolver, IDE_TYPE_OBJECT)

static void
ide_symbol_resolver_real_get_symbol_tree_async (IdeSymbolResolver   *self,
                                                GFile               *file,
                                                IdeBuffer           *buffer,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
  g_assert (buffer == NULL || IDE_IS_BUFFER (buffer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_symbol_resolver_get_symbol_tree_async);
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Symbol tree is not supported on this symbol resolver");
}

static IdeSymbolTree *
ide_symbol_resolver_real_get_symbol_tree_finish (IdeSymbolResolver  *self,
                                                 GAsyncResult       *result,
                                                 GError            **error)
{
  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_symbol_resolver_real_find_references_async (IdeSymbolResolver   *self,
                                                IdeSourceLocation   *location,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_symbol_resolver_real_find_references_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Finding references is not supported for this language");
}

static GPtrArray *
ide_symbol_resolver_real_find_references_finish (IdeSymbolResolver  *self,
                                                 GAsyncResult       *result,
                                                 GError            **error)
{
  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_symbol_resolver_real_find_nearest_scope_async (IdeSymbolResolver   *self,
                                                   IdeSourceLocation   *location,
                                                   GCancellable        *cancellable,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data)
{
  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (location != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (self,
                           callback,
                           user_data,
                           ide_symbol_resolver_real_find_nearest_scope_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Finding nearest scope is not supported for this language");
}

static IdeSymbol *
ide_symbol_resolver_real_find_nearest_scope_finish (IdeSymbolResolver  *self,
                                                    GAsyncResult       *result,
                                                    GError            **error)
{
  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
ide_symbol_resolver_default_init (IdeSymbolResolverInterface *iface)
{
  iface->get_symbol_tree_async = ide_symbol_resolver_real_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_symbol_resolver_real_get_symbol_tree_finish;
  iface->find_references_async = ide_symbol_resolver_real_find_references_async;
  iface->find_references_finish = ide_symbol_resolver_real_find_references_finish;
  iface->find_nearest_scope_async = ide_symbol_resolver_real_find_nearest_scope_async;
  iface->find_nearest_scope_finish = ide_symbol_resolver_real_find_nearest_scope_finish;
}

/**
 * ide_symbol_resolver_lookup_symbol_async:
 * @self: An #IdeSymbolResolver.
 * @location: An #IdeSourceLocation.
 * @cancellable: (allow-none): a #GCancellable or %NULL.
 * @callback: A callback to execute upon completion.
 * @user_data: user data for @callback.
 *
 * Asynchronously requests that @self determine the symbol existing at the source location
 * denoted by @self. @callback should call ide_symbol_resolver_lookup_symbol_finish() to
 * retrieve the result.
 */
void
ide_symbol_resolver_lookup_symbol_async  (IdeSymbolResolver   *self,
                                          IdeSourceLocation   *location,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SYMBOL_RESOLVER_GET_IFACE (self)->lookup_symbol_async (self, location, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_lookup_symbol_finish:
 * @self: An #IdeSymbolResolver.
 * @result: a #GAsyncResult provided to the callback.
 * @error: (out): A location for an @error or %NULL.
 *
 * Completes an asynchronous call to lookup a symbol using
 * ide_symbol_resolver_lookup_symbol_async().
 *
 * Returns: (transfer full) (nullable): An #IdeSymbol if successful; otherwise %NULL.
 */
IdeSymbol *
ide_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver  *self,
                                          GAsyncResult       *result,
                                          GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_IFACE (self)->lookup_symbol_finish (self, result, error);
}

/**
 * ide_symbol_resolver_get_symbol_tree_async:
 * @self: An #IdeSymbolResolver
 * @file: a #GFile
 * @buffer: an #IdeBuffer or %NULL
 * @cancellable: (allow-none): a #GCancellable or %NULL.
 * @callback: (allow-none): a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Asynchronously fetch an up to date symbol tree for @file.
 */
void
ide_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *self,
                                           GFile               *file,
                                           IdeBuffer           *buffer,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (G_IS_FILE (file));

  IDE_SYMBOL_RESOLVER_GET_IFACE (self)->get_symbol_tree_async (self, file, buffer, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_get_symbol_tree_finish:
 *
 * Completes an asynchronous request to get the symbol tree for the requested file.
 *
 * Returns: (nullable) (transfer full): An #IdeSymbolTree; otherwise %NULL and @error is set.
 */
IdeSymbolTree *
ide_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *self,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (!result || G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_IFACE (self)->get_symbol_tree_finish (self, result, error);
}

void
ide_symbol_resolver_load (IdeSymbolResolver *self)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));

  if (IDE_SYMBOL_RESOLVER_GET_IFACE (self)->load)
    IDE_SYMBOL_RESOLVER_GET_IFACE (self)->load (self);
}

void
ide_symbol_resolver_unload (IdeSymbolResolver *self)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));

  if (IDE_SYMBOL_RESOLVER_GET_IFACE (self)->unload)
    IDE_SYMBOL_RESOLVER_GET_IFACE (self)->unload (self);
}

void
ide_symbol_resolver_find_references_async (IdeSymbolResolver   *self,
                                           IdeSourceLocation   *location,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SYMBOL_RESOLVER_GET_IFACE (self)->find_references_async (self, location, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_find_references_finish:
 * @self: a #IdeSymbolResolver
 * @result: a #GAsyncResult
 * @error: a #GError or %NULL
 *
 * Completes an asynchronous request to ide_symbol_resolver_find_references_async().
 *
 * Returns: (transfer full) (element-type Ide.SourceRange): a #GPtrArray
 *   of #IdeSourceRange if successful; otherwise %NULL and @error is set.
 */
GPtrArray *
ide_symbol_resolver_find_references_finish (IdeSymbolResolver  *self,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_IFACE (self)->find_references_finish (self, result, error);
}

/**
 * ide_symbol_resolver_find_nearest_scope_async:
 * @self: a #IdeSymbolResolver
 * @location: an #IdeSourceLocation
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (scope async) (closure user_data): an async callback
 * @user_data: user data for @callback
 *
 * This function asynchronously requests to locate the containing
 * scope for a given source location.
 *
 * See ide_symbol_resolver_find_nearest_scope_finish() for how to
 * complete the operation.
 *
 * Since: 3.26
 */
void
ide_symbol_resolver_find_nearest_scope_async (IdeSymbolResolver    *self,
                                              IdeSourceLocation    *location,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (location != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SYMBOL_RESOLVER_GET_IFACE (self)->find_nearest_scope_async (self, location, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_find_nearest_scope_finish:
 * @self: a #IdeSymbolResolver
 * @result: a #GAsyncResult
 * @error: a location for a #GError or %NULL
 *
 * This function completes an asynchronous operation to locate the containing
 * scope for a given source location.
 *
 * See ide_symbol_resolver_find_nearest_scope_async() for more information.
 *
 * Returns: (transfer full) (nullable): An #IdeSymbol or %NULL
 *
 * Since: 3.26
 */
IdeSymbol *
ide_symbol_resolver_find_nearest_scope_finish (IdeSymbolResolver  *self,
                                               GAsyncResult       *result,
                                               GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_IFACE (self)->find_nearest_scope_finish (self, result, error);
}
