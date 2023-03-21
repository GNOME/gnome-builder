/* debuggerui-plugin.c
 *
 * Copyright 2015-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "debuggerui-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-debugger.h>
#include <libide-gui.h>

#include "gbp-debugger-tool.h"
#include "gbp-debuggerui-tweaks-addin.h"
#include "ide-debugger-hover-provider.h"
#include "ide-debugger-workspace-addin.h"

_IDE_EXTERN void
_gbp_debuggerui_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUN_TOOL,
                                              GBP_TYPE_DEBUGGER_TOOL);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                              IDE_TYPE_DEBUGGER_HOVER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_DEBUGGERUI_TWEAKS_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              IDE_TYPE_DEBUGGER_WORKSPACE_ADDIN);
}
