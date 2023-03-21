/* code-index-plugin.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#include "config.h"

#include <libpeas.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-search.h>

#include "gbp-code-index-application-addin.h"
#include "gbp-code-index-workbench-addin.h"
#include "ide-code-index-search-provider.h"
#include "ide-code-index-symbol-resolver.h"

_IDE_EXTERN void
_ide_code_index_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_APPLICATION_ADDIN,
                                              GBP_TYPE_CODE_INDEX_APPLICATION_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SEARCH_PROVIDER,
                                              IDE_TYPE_CODE_INDEX_SEARCH_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              IDE_TYPE_CODE_INDEX_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_CODE_INDEX_WORKBENCH_ADDIN);
}
