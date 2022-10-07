/* gbp-lua-service.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-lua-service"

#include "config.h"

#include <jsonrpc-glib.h>

#include "gbp-lua-service.h"

struct _GbpLuaService
{
  IdeLspService parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpLuaService, gbp_lua_service, IDE_TYPE_LSP_SERVICE)

static void
gbp_lua_service_configure_client (IdeLspService *service,
                                  IdeLspClient  *client)
{
  IDE_ENTRY;

  g_assert (GBP_IS_LUA_SERVICE (service));
  g_assert (IDE_IS_LSP_CLIENT (client));

  ide_lsp_client_add_language (client, "lua");

  IDE_EXIT;
}

static void
gbp_lua_service_prepare_run_context (IdeLspService *service,
                                     IdePipeline   *pipeline,
                                     IdeRunContext *run_context)
{
  IDE_ENTRY;

  g_assert (GBP_IS_LUA_SERVICE (service));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  /* Options:
   *
   *   --logpath=
   *   --loglevel=trace
   *   --metapath=
   *   --locale=
   *   --configpath=
   *   --version
   *   --check
   *   --checklevel=(Error|Warning|Information)
   */

  IDE_EXIT;
}

static void
gbp_lua_service_class_init (GbpLuaServiceClass *klass)
{
  IdeLspServiceClass *lsp_service_class = IDE_LSP_SERVICE_CLASS (klass);

  lsp_service_class->configure_client = gbp_lua_service_configure_client;
  lsp_service_class->prepare_run_context = gbp_lua_service_prepare_run_context;
}

static void
gbp_lua_service_init (GbpLuaService *self)
{
  ide_lsp_service_set_program (IDE_LSP_SERVICE (self), "lua-language-server");
}
