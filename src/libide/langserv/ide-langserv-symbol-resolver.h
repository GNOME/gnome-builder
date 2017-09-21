/* ide-langserv-symbol-resolver.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
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

#ifndef IDE_LANGSERV_SYMBOL_RESOLVER_H
#define IDE_LANGSERV_SYMBOL_RESOLVER_H

#include "ide-object.h"

#include "langserv/ide-langserv-client.h"
#include "symbols/ide-symbol-resolver.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_SYMBOL_RESOLVER (ide_langserv_symbol_resolver_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeLangservSymbolResolver, ide_langserv_symbol_resolver, IDE, LANGSERV_SYMBOL_RESOLVER, IdeObject)

struct _IdeLangservSymbolResolverClass
{
  IdeObjectClass parent_class;

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
};

IdeLangservClient *ide_langserv_symbol_resolver_get_client (IdeLangservSymbolResolver *self);
void               ide_langserv_symbol_resolver_set_client (IdeLangservSymbolResolver *self,
                                                            IdeLangservClient         *client);

G_END_DECLS

#endif /* IDE_LANGSERV_SYMBOL_RESOLVER_H */
