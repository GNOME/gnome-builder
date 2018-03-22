/* ide-langserv-highlighter.h
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

#include "ide-object.h"
#include "ide-version-macros.h"

#include "highlighting/ide-highlighter.h"
#include "langserv/ide-langserv-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_HIGHLIGHTER (ide_langserv_highlighter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLangservHighlighter, ide_langserv_highlighter, IDE, LANGSERV_HIGHLIGHTER, IdeObject)

struct _IdeLangservHighlighterClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
};

IDE_AVAILABLE_IN_ALL
IdeLangservClient *ide_langserv_highlighter_get_client (IdeLangservHighlighter *self);
IDE_AVAILABLE_IN_ALL
void               ide_langserv_highlighter_set_client (IdeLangservHighlighter *self,
                                                        IdeLangservClient      *client);

G_END_DECLS
