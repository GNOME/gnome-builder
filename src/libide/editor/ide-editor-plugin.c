/* ide-editor-plugin.c
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

#define G_LOG_DOMAIN "ide-editor-plugin"

#include "config.h"

#include <libpeas/peas.h>

#include "editor/ide-editor-layout-stack-addin.h"
#include "editor/ide-editor-workbench-addin.h"

void
ide_editor_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module, IDE_TYPE_LAYOUT_STACK_ADDIN, IDE_TYPE_EDITOR_LAYOUT_STACK_ADDIN);
  peas_object_module_register_extension_type (module, IDE_TYPE_WORKBENCH_ADDIN, IDE_TYPE_EDITOR_WORKBENCH_ADDIN);
}
