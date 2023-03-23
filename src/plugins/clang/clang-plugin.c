/* clang-plugin.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "clang-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-sourceview.h>

#include "ide-clang-client.h"
#include "ide-clang-code-indexer.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-highlighter.h"
#include "ide-clang-rename-provider.h"
#include "ide-clang-symbol-node.h"
#include "ide-clang-symbol-resolver.h"
#include "ide-clang-symbol-tree.h"
#include "gbp-clang-tweaks-addin.h"

_IDE_EXTERN void
_ide_clang_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_CODE_INDEXER,
                                              IDE_TYPE_CLANG_CODE_INDEXER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              IDE_TYPE_CLANG_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              IDE_TYPE_CLANG_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              IDE_TYPE_CLANG_DIAGNOSTIC_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_CLANG_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_RENAME_PROVIDER,
                                              IDE_TYPE_CLANG_RENAME_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_CLANG_TWEAKS_ADDIN);
}
