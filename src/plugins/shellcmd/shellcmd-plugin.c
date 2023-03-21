/* shellcmd-plugin.c
 *
 * Copyright 2019-2022 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <libpeas.h>

#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-search.h>

#include "gbp-shellcmd-run-command-provider.h"
#include "gbp-shellcmd-search-provider.h"
#include "gbp-shellcmd-shortcut-provider.h"
#include "gbp-shellcmd-tweaks-addin.h"

_IDE_EXTERN void
_gbp_shellcmd_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RUN_COMMAND_PROVIDER,
                                              GBP_TYPE_SHELLCMD_RUN_COMMAND_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              GBP_TYPE_SHELLCMD_SEARCH_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SHORTCUT_PROVIDER,
                                              GBP_TYPE_SHELLCMD_SHORTCUT_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_SHELLCMD_TWEAKS_ADDIN);
}
