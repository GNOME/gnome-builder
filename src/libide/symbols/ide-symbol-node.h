/* ide-symbol-node.h
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

#include "ide-object.h"
#include "ide-version-macros.h"

#include "symbols/ide-symbol.h"
#include "diagnostics/ide-source-location.h"

G_BEGIN_DECLS

#define IDE_TYPE_SYMBOL_NODE (ide_symbol_node_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeSymbolNode, ide_symbol_node, IDE, SYMBOL_NODE, IdeObject)

struct _IdeSymbolNodeClass
{
  IdeObjectClass parent;

  void               (*get_location_async)  (IdeSymbolNode        *self,
                                             GCancellable         *cancellable,
                                             GAsyncReadyCallback   callback,
                                             gpointer              user_data);
  IdeSourceLocation *(*get_location_finish) (IdeSymbolNode        *self,
                                             GAsyncResult         *result,
                                             GError             **error);

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeSymbolKind      ide_symbol_node_get_kind            (IdeSymbolNode        *self);
IDE_AVAILABLE_IN_ALL
IdeSymbolFlags     ide_symbol_node_get_flags           (IdeSymbolNode        *self);
IDE_AVAILABLE_IN_ALL
const gchar       *ide_symbol_node_get_name            (IdeSymbolNode        *self);
IDE_AVAILABLE_IN_ALL
gboolean           ide_symbol_node_get_use_markup      (IdeSymbolNode        *self);
IDE_AVAILABLE_IN_ALL
void               ide_symbol_node_get_location_async  (IdeSymbolNode        *self,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
IDE_AVAILABLE_IN_ALL
IdeSourceLocation *ide_symbol_node_get_location_finish (IdeSymbolNode        *self,
                                                        GAsyncResult         *result,
                                                        GError              **error);

G_END_DECLS
