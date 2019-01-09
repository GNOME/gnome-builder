/* terminal-plugin.c
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

#define G_LOG_DOMAIN "terminal-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "gbp-terminal-application-addin.h"
#include "gbp-terminal-workspace-addin.h"

_IDE_EXTERN void
_gbp_terminal_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_TERMINAL_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              GBP_TYPE_TERMINAL_WORKSPACE_ADDIN);
}
