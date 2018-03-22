/* ide-langserv-formatter.h
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#include "formatting/ide-formatter.h"
#include "langserv/ide-langserv-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_FORMATTER (ide_langserv_formatter_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeLangservFormatter, ide_langserv_formatter, IDE, LANGSERV_FORMATTER, IdeObject)

struct _IdeLangservFormatter
{
  IdeObject parent_class;

  /*< private >*/
  gpointer _reserved[4];
};

IDE_AVAILABLE_IN_ALL
void                  ide_langserv_formatter_set_client (IdeLangservFormatter *self,
                                                         IdeLangservClient    *client);
IDE_AVAILABLE_IN_ALL
IdeLangservClient    *ide_langserv_formatter_get_client (IdeLangservFormatter *self);

G_END_DECLS
