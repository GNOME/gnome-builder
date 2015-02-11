/* ide-symbol-resolver.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_SYMBOL_RESOLVER_H
#define IDE_SYMBOL_RESOLVER_H

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL_RESOLVER               (ide_symbol_resolver_get_type ())
#define IDE_SYMBOL_RESOLVER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SYMBOL_RESOLVER, IdeSymbolResolver))
#define IDE_IS_SYMBOL_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SYMBOL_RESOLVER))
#define IDE_SYMBOL_RESOLVER_GET_INTERFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), IDE_TYPE_SYMBOL_RESOLVER, IdeSymbolResolverInterface))

struct _IdeSymbolResolverInterface
{
  GTypeInterface parent;
};

GType ide_symbol_resolver_get_type (void);

void       ide_symbol_resolver_lookup_symbol_async  (IdeSymbolResolver    *self,
                                                     IdeSourceLocation    *location,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              user_data);
IdeSymbol *ide_symbol_resolver_lookup_symbol_finish (IdeSymbolResolver    *self,
                                                     GAsyncResult         *result,
                                                     GError              **error);

G_END_DECLS

#endif /* IDE_SYMBOL_RESOLVER_H */
