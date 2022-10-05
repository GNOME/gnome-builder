/* lua-language-server-plugin.c
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

#define G_LOG_DOMAIN "lua-language-server-plugin"

#include "config.h"

#include <libpeas/peas.h>

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-lsp.h>

#include "gbp-lua-service.h"
#include "gbp-lua-completion-provider.h"
#include "gbp-lua-diagnostic-provider.h"
#include "gbp-lua-symbol-resolver.h"
#include "gbp-lua-highlighter.h"
#include "gbp-lua-formatter.h"
#include "gbp-lua-rename-provider.h"
#include "gbp-lua-hover-provider.h"
#include "gbp-lua-code-action-provider.h"

_IDE_EXTERN void
_gbp_lua_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              GBP_TYPE_LUA_DIAGNOSTIC_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              GBP_TYPE_LUA_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              GBP_TYPE_LUA_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              GBP_TYPE_LUA_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_FORMATTER,
                                              GBP_TYPE_LUA_FORMATTER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                              GBP_TYPE_LUA_HOVER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RENAME_PROVIDER,
                                              GBP_TYPE_LUA_RENAME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CODE_ACTION_PROVIDER,
                                              GBP_TYPE_LUA_CODE_ACTION_PROVIDER);
}
