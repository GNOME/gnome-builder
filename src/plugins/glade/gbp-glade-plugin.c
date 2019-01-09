/* gbp-glade-plugin.c
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

#define G_LOG_DOMAIN "gbp-glade-plugin"

#include <gladeui/glade.h>
#include <ide.h>
#include <libpeas/peas.h>

#include "gbp-glade-editor-addin.h"
#include "gbp-glade-layout-stack-addin.h"
#include "gbp-glade-workbench-addin.h"

void
gbp_glade_register_types (PeasObjectModule *module)
{
  glade_init ();

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_EDITOR_ADDIN,
                                              GBP_TYPE_GLADE_EDITOR_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_LAYOUT_STACK_ADDIN,
                                              GBP_TYPE_GLADE_LAYOUT_STACK_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_GLADE_WORKBENCH_ADDIN);
}
