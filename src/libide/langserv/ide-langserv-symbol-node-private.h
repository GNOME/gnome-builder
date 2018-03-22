/* ide-langserv-symbol-node-private.h
 *
 * Copyright 2016 Christian Hergert <chergert@redhat.com>
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

#include "langserv/ide-langserv-symbol-node.h"

G_BEGIN_DECLS

struct _IdeLangservSymbolNode
{
  IdeSymbolNode parent_instance;
  GNode         gnode;
};


IdeLangservSymbolNode *ide_langserv_symbol_node_new (GFile       *file,
                                                     const gchar *name,
                                                     const gchar *parent_name,
                                                     gint         kind,
                                                     guint        begin_line,
                                                     guint        begin_column,
                                                     guint        end_line,
                                                     guint        end_column);

G_END_DECLS
