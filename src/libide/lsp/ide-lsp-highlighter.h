/* ide-lsp-highlighter.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#if !defined (IDE_LSP_INSIDE) && !defined (IDE_LSP_COMPILATION)
# error "Only <libide-lsp.h> can be included directly."
#endif

#include <libide-code.h>

#include "ide-lsp-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LSP_HIGHLIGHTER (ide_lsp_highlighter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLspHighlighter, ide_lsp_highlighter, IDE, LSP_HIGHLIGHTER, IdeObject)

struct _IdeLspHighlighterClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeLspClient *ide_lsp_highlighter_get_client     (IdeLspHighlighter *self);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_highlighter_set_client     (IdeLspHighlighter *self,
                                                  IdeLspClient      *client);
IDE_AVAILABLE_IN_ALL
void          ide_lsp_highlighter_set_kind_style (IdeLspHighlighter *self,
                                                  IdeSymbolKind      kind,
                                                  const gchar       *style);

G_END_DECLS
