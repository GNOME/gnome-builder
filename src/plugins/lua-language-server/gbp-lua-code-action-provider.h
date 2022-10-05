/* gbp-lua-code-action-provider.h
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

#pragma once

#include <libide-lsp.h>

G_BEGIN_DECLS

#define GBP_TYPE_LUA_CODE_ACTION_PROVIDER (gbp_lua_code_action_provider_get_type())

G_DECLARE_FINAL_TYPE (GbpLuaCodeActionProvider, gbp_lua_code_action_provider, GBP, LUA_CODE_ACTION_PROVIDER, IdeLspCodeActionProvider)

G_END_DECLS
