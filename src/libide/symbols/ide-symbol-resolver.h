/* ide-symbol-resolver.h
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#pragma once

#include "ide-version-macros.h"

#include "ide-object.h"
#include "symbols/ide-symbol-tree.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL_RESOLVER (ide_symbol_resolver_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (IdeSymbolResolver, ide_symbol_resolver, IDE, SYMBOL_RESOLVER, IdeObject)

struct _IdeSymbolResolverInterface
{
  GTypeInterface parent_interface;

  void           (*lookup_symbol_async)      (IdeSymbolResolver    *self,
                                              IdeSourceLocation    *location,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  IdeSymbol     *(*lookup_symbol_finish)     (IdeSymbolResolver    *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  void           (*get_symbol_tree_async)    (IdeSymbolResolver    *self,
                                              GFile                *file,
                                              IdeBuffer            *buffer,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  IdeSymbolTree *(*get_symbol_tree_finish)   (IdeSymbolResolver    *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  void           (*load)                     (IdeSymbolResolver    *self);
  void           (*find_references_async)    (IdeSymbolResolver    *self,
                                              IdeSourceLocation    *location,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  GPtrArray     *(*find_references_finish)   (IdeSymbolResolver    *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  void           (*find_nearest_scope_async) (IdeSymbolResolver    *self,
                                              IdeSourceLocation    *location,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data);
  IdeSymbol     *(*find_nearest_scope_finish) (IdeSymbolResolver    *self,
                                              GAsyncResult         *result,
                                              GError              **error);
  void           (*unload)                   (IdeSymbolResolver    *self);
};

IDE_AVAILABLE_IN_ALL
void           ide_symbol_resolver_load                      (IdeSymbolResolver    *self);
IDE_AVAILABLE_IN_3_28
void           ide_symbol_resolver_unload                    (IdeSymbolResolver    *self);
IDE_AVAILABLE_IN_ALL
void           ide_symbol_resolver_lookup_symbol_async       (IdeSymbolResolver    *self,
                                                              IdeSourceLocation    *location,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSymbol     *ide_symbol_resolver_lookup_symbol_finish      (IdeSymbolResolver    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_symbol_resolver_get_symbol_tree_async     (IdeSymbolResolver    *self,
                                                              GFile                *file,
                                                              IdeBuffer            *buffer,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSymbolTree *ide_symbol_resolver_get_symbol_tree_finish    (IdeSymbolResolver    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_symbol_resolver_find_references_async     (IdeSymbolResolver    *self,
                                                              IdeSourceLocation    *location,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
GPtrArray     *ide_symbol_resolver_find_references_finish    (IdeSymbolResolver    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);
IDE_AVAILABLE_IN_ALL
void           ide_symbol_resolver_find_nearest_scope_async  (IdeSymbolResolver    *self,
                                                              IdeSourceLocation    *location,
                                                              GCancellable         *cancellable,
                                                              GAsyncReadyCallback   callback,
                                                              gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSymbol     *ide_symbol_resolver_find_nearest_scope_finish (IdeSymbolResolver    *self,
                                                              GAsyncResult         *result,
                                                              GError              **error);

G_END_DECLS
