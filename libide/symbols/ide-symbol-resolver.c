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

#define G_LOG_DOMAIN "ide-symbol-resolver"

#include "ide-context.h"

#include "files/ide-file.h"
#include "symbols/ide-symbol-resolver.h"

G_DEFINE_INTERFACE (IdeSymbolResolver, ide_symbol_resolver, IDE_TYPE_OBJECT)

static void
ide_symbol_resolver_real_get_symbol_tree_async (IdeSymbolResolver   *self,
                                                GFile               *file,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_SYMBOL_RESOLVER (self));
  g_assert (G_IS_FILE (file));
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
ide_symbol_resolver_default_init (IdeSymbolResolverInterface *iface)
{
  iface->get_symbol_tree_async = ide_symbol_resolver_real_get_symbol_tree_async;
  iface->get_symbol_tree_finish = ide_symbol_resolver_real_get_symbol_tree_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_object ("context",
                                                            "Context",
                                                            "Context",
                                                            IDE_TYPE_CONTEXT,
                                                            (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}

/**
 * ide_symbol_resolver_lookup_symbol_async:
 * @self: An #IdeSymbolResolver.
 * @location: An #IdeSourceLocation.
 * @cancellable: (allow-none): A #GCancellable or %NULL.
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

  return IDE_SYMBOL_RESOLVER_GET_IFACE (self)->lookup_symbol_finish (self, result, error);
}

/**
 * ide_symbol_resolver_get_symbol_tree_async:
 * @self: An #IdeSymbolResolver
 * @file: A #GFile
 * @cancellable: (allow-none): a #GCancellable or %NULL.
 * @callback: (allow-none): a callback to execute upon completion
 * @user_data: user data for @callback
 *
 * Asynchronously fetch an up to date symbol tree for @file.
 */
void
ide_symbol_resolver_get_symbol_tree_async (IdeSymbolResolver   *self,
                                           GFile               *file,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_return_if_fail (IDE_IS_SYMBOL_RESOLVER (self));
  g_return_if_fail (G_IS_FILE (file));

  IDE_SYMBOL_RESOLVER_GET_IFACE (self)->get_symbol_tree_async (self, file, cancellable, callback, user_data);
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
