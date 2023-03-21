/* symbol-tree-plugin.c
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-gui.h>
#include <libide-search.h>
#include <libide-sourceview.h>

#include "gbp-symbol-hover-provider.h"
#include "gbp-symbol-search-provider.h"
#include "gbp-symbol-workspace-addin.h"

_IDE_EXTERN void
_gbp_symbol_tree_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_HOVER_PROVIDER,
                                              GBP_TYPE_SYMBOL_HOVER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              GBP_TYPE_SYMBOL_SEARCH_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKSPACE_ADDIN,
                                              GBP_TYPE_SYMBOL_WORKSPACE_ADDIN);
}
