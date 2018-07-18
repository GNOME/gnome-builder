/* ide-langserv-hover-provider.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "ide-object.h"
#include "ide-version-macros.h"

#include "langserv/ide-langserv-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_HOVER_PROVIDER (ide_langserv_hover_provider_get_type())

IDE_AVAILABLE_IN_3_30
G_DECLARE_DERIVABLE_TYPE (IdeLangservHoverProvider, ide_langserv_hover_provider, IDE, LANGSERV_HOVER_PROVIDER, IdeObject)

struct _IdeLangservHoverProviderClass
{
  IdeObjectClass parent_class;

  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_3_30
IdeLangservClient *ide_langserv_hover_provider_get_client (IdeLangservHoverProvider *self);
IDE_AVAILABLE_IN_3_30
void               ide_langserv_hover_provider_set_client (IdeLangservHoverProvider *self,
                                                           IdeLangservClient        *client);

G_END_DECLS
