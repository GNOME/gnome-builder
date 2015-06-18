/* ide-symbol-resolver.c
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

#include "ide-file.h"
#include "ide-symbol-resolver.h"

G_DEFINE_ABSTRACT_TYPE (IdeSymbolResolver, ide_symbol_resolver, IDE_TYPE_OBJECT)

static void
ide_symbol_resolver_class_init (IdeSymbolResolverClass *klass)
{
}

static void
ide_symbol_resolver_init (IdeSymbolResolver *self)
{
}

/**
 * ide_symbol_resolver_lookup_symbol_async:
 * @self: An #IdeSymbolResolver.
 * @location: An #IdeSourceLocation.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
 * @callback: (scope async): A callback to execute upon completion.
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

  IDE_SYMBOL_RESOLVER_GET_CLASS (self)->
    lookup_symbol_async (self, location, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_lookup_symbol_finish:
 * @self: An #IdeSymbolResolver.
 * @result: A #GAsyncResult provided to the callback.
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

  return IDE_SYMBOL_RESOLVER_GET_CLASS (self)->lookup_symbol_finish (self, result, error);
}

void
ide_symbol_resolver_get_symbols_async (IdeSymbolResolver   *self,
                                       IdeFile             *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (IDE_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_SYMBOL_RESOLVER_GET_CLASS (self)->
    get_symbols_async (self, file, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_get_symbols_finish:
 * @self: An #IdeSymbolResolver.
 * @result: A #GAsyncResult.
 * @error: (out): A location for a #GError or %NULL.
 *
 *
 * Returns: (transfer container) (element-type IdeSymbol*): A #GPtrArray if successful.
 */
GPtrArray *
ide_symbol_resolver_get_symbols_finish (IdeSymbolResolver  *self,
                                        GAsyncResult       *result,
                                        GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_CLASS (self)->get_symbols_finish (self, result, error);
}

void
ide_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *self,
                                           GFile               *file,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (G_IS_FILE (file));

  IDE_SYMBOL_RESOLVER_GET_CLASS (self)->get_symbol_tree_async (self, file, cancellable, callback, user_data);
}

/**
 * ide_symbol_resolver_get_symbol_tree_finish:
 *
 * Completes an asynchronous request to get the symbol tree for the requested file.
 *
 * Returns: (transfer full): An #IdeSymbolTree; otherwise %NULL and @error is set.
 */
IdeSymbolTree *
ide_symbol_resolver_get_symbol_tree_finish (IdeSymbolResolver  *self,
                                            GAsyncResult       *result,
                                            GError            **error)
{
  g_return_val_if_fail (IDE_IS_SYMBOL_RESOLVER (self), NULL);
  g_return_val_if_fail (!result || G_IS_ASYNC_RESULT (result), NULL);

  return IDE_SYMBOL_RESOLVER_GET_CLASS (self)->get_symbol_tree_finish (self, result, error);
}
