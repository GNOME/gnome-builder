/* vcsui-plugin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "vcsui-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-tree.h>

#include "gbp-vcsui-application-addin.h"
#include "gbp-vcsui-editor-page-addin.h"
#include "gbp-vcsui-tree-addin.h"
#include "gbp-vcsui-workbench-addin.h"
#include "gbp-vcsui-workspace-addin.h"

_IDE_EXTERN void
_gbp_vcsui_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_VCSUI_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_EDITOR_PAGE_ADDIN,
                                              GBP_TYPE_VCSUI_EDITOR_PAGE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TREE_ADDIN,
                                              GBP_TYPE_VCSUI_TREE_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_VCSUI_WORKBENCH_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              GBP_TYPE_VCSUI_WORKSPACE_ADDIN);
}
