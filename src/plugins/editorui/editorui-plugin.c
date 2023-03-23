/* editorui-plugin.c
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "editorui-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-gui.h>

#include "gbp-editorui-application-addin.h"
#include "gbp-editorui-resources.h"
#include "gbp-editorui-search-provider.h"
#include "gbp-editorui-tweaks-addin.h"
#include "gbp-editorui-workbench-addin.h"
#include "gbp-editorui-workspace-addin.h"

_IDE_EXTERN void
_gbp_editorui_register_types (PeasObjectModule *module)
{
  g_resources_register (gbp_editorui_get_resource ());

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_EDITORUI_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              GBP_TYPE_EDITORUI_SEARCH_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_EDITORUI_TWEAKS_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_EDITORUI_WORKBENCH_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              GBP_TYPE_EDITORUI_WORKSPACE_ADDIN);
}
