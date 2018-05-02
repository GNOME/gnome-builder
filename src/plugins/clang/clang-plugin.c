/* clang-plugin.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
 */

#include <libpeas/peas.h>
#include <ide.h>

#include "ide-clang-client.h"
#include "ide-clang-code-indexer.h"
#include "ide-clang-completion-item.h"
#include "ide-clang-completion-provider.h"
#include "ide-clang-diagnostic-provider.h"
#include "ide-clang-highlighter.h"
#include "ide-clang-preferences-addin.h"
#include "ide-clang-symbol-node.h"
#include "ide-clang-symbol-resolver.h"
#include "ide-clang-symbol-tree.h"

void
ide_clang_register_types (PeasObjectModule *module)
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
                                              IDE_TYPE_SERVICE,
                                              IDE_TYPE_CLANG_CLIENT);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              IDE_TYPE_CLANG_DIAGNOSTIC_PROVIDER);
#if 0
  /* Disabled until the new Completion Engine lands. GtkSourceView cannot keep
   * up with the performance due to some O(n²) issues in the hot path. We'll
   * be working on the new completion engine next.
   */
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_CLANG_COMPLETION_PROVIDER);
#endif
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PREFERENCES_ADDIN,
                                              IDE_TYPE_CLANG_PREFERENCES_ADDIN);
}
