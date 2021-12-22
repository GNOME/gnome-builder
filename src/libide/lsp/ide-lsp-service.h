/* ide-lsp-service.h
 *
 * Copyright 2021 James Westman <james@jwestman.net>
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
#include <libide-foundry.h>

#include "ide-lsp-client.h"

G_BEGIN_DECLS

#define IDE_TYPE_LSP_SERVICE (ide_lsp_service_get_type())

IDE_AVAILABLE_IN_42
G_DECLARE_DERIVABLE_TYPE (IdeLspService, ide_lsp_service, IDE, LSP_SERVICE, IdeObject)

struct _IdeLspServiceClass
{
  IdeObjectClass parent_class;

  IdeSubprocessLauncher *(*create_launcher)      (IdeLspService           *self,
                                                  IdePipeline             *pipeline,
                                                  GSubprocessFlags         flags);
  void                   (*configure_launcher)   (IdeLspService           *self,
                                                  IdePipeline             *pipeline,
                                                  IdeSubprocessLauncher   *launcher);
  void                   (*configure_supervisor) (IdeLspService           *self,
                                                  IdeSubprocessSupervisor *supervisor);
  void                   (*configure_client)     (IdeLspService           *self,
                                                  IdeLspClient            *client);
};

IDE_AVAILABLE_IN_42
void                ide_lsp_service_class_bind_client      (IdeLspServiceClass *klass,
                                                            IdeObject          *provider);
IDE_AVAILABLE_IN_42
void                ide_lsp_service_class_bind_client_lazy (IdeLspServiceClass *klass,
                                                            IdeObject          *provider);
IDE_AVAILABLE_IN_42
void                ide_lsp_service_set_inherit_stderr     (IdeLspService      *self,
                                                            gboolean            inherit_stderr);
IDE_AVAILABLE_IN_42
gboolean            ide_lsp_service_get_inherit_stderr     (IdeLspService      *self);
IDE_AVAILABLE_IN_42
void                ide_lsp_service_restart                (IdeLspService      *self);
IDE_AVAILABLE_IN_42
const char         *ide_lsp_service_get_program            (IdeLspService      *self);
IDE_AVAILABLE_IN_42
void                ide_lsp_service_set_program            (IdeLspService      *self,
                                                            const char         *program);
IDE_AVAILABLE_IN_42
const char * const *ide_lsp_service_get_search_path        (IdeLspService      *self);
IDE_AVAILABLE_IN_42
void                ide_lsp_service_set_search_path        (IdeLspService      *self,
                                                            const char * const *search_path);

G_END_DECLS
