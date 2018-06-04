/* ide-langserv-completion-provider.h
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

#include "ide-version-macros.h"

#include "ide-object.h"

#include "completion/ide-completion-provider.h"
#include "langserv/ide-langserv-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LANGSERV_COMPLETION_PROVIDER (ide_langserv_completion_provider_get_type())

#define IDE_LANGSERV_COMPLETION_PROVIDER_PRIORITY 200

IDE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (IdeLangservCompletionProvider, ide_langserv_completion_provider, IDE, LANGSERV_COMPLETION_PROVIDER, IdeObject)

struct _IdeLangservCompletionProviderClass
{
  IdeObjectClass parent_class;

  /*< private >*/
  gpointer _reserved[8];
};

IDE_AVAILABLE_IN_ALL
IdeLangservClient *ide_langserv_completion_provider_get_client (IdeLangservCompletionProvider *self);
IDE_AVAILABLE_IN_ALL
void               ide_langserv_completion_provider_set_client (IdeLangservCompletionProvider *self,
                                                                IdeLangservClient             *client);

G_END_DECLS
