/* ide-lsp-code-action-provider.h
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#define IDE_TYPE_LSP_CODE_ACTION_PROVIDER (ide_lsp_code_action_provider_get_type())

IDE_AVAILABLE_IN_42
G_DECLARE_DERIVABLE_TYPE (IdeLspCodeActionProvider, ide_lsp_code_action_provider, IDE, LSP_CODE_ACTION_PROVIDER, IdeObject)

struct _IdeLspCodeActionProviderClass
{
  IdeObjectClass parent_class;
};

IDE_AVAILABLE_IN_42
void          ide_lsp_code_action_provider_set_client (IdeLspCodeActionProvider *self,
                                                       IdeLspClient             *client);
IDE_AVAILABLE_IN_42
IdeLspClient *ide_lsp_code_action_provider_get_client (IdeLspCodeActionProvider *self);

G_END_DECLS
