/* ctags-plugin.c
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

#include "config.h"

#include <libpeas.h>
#include <gtksourceview/gtksource.h>

#include <libide-code.h>
#include <libide-gui.h>
#include <libide-sourceview.h>
#include <libide-io.h>

#include "gbp-ctags-tweaks-addin.h"
#include "gbp-ctags-workbench-addin.h"
#include "ide-ctags-builder.h"
#include "ide-ctags-completion-item.h"
#include "ide-ctags-completion-provider.h"
#include "ide-ctags-highlighter.h"
#include "ide-ctags-index.h"
#include "ide-ctags-symbol-resolver.h"

_IDE_EXTERN void
_ide_ctags_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                              IDE_TYPE_CTAGS_COMPLETION_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_HIGHLIGHTER,
                                              IDE_TYPE_CTAGS_HIGHLIGHTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SYMBOL_RESOLVER,
                                              IDE_TYPE_CTAGS_SYMBOL_RESOLVER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_WORKBENCH_ADDIN,
                                              GBP_TYPE_CTAGS_WORKBENCH_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_CTAGS_TWEAKS_ADDIN);

  ide_g_file_add_ignored_pattern ("tags.??????");
}
