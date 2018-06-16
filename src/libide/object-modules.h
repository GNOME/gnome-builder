/* object-modules.h
 *
 * Copyright © 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <libpeas/peas.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

_IDE_EXTERN void ide_build_tool_register_types  (PeasObjectModule *module);
_IDE_EXTERN void ide_buildconfig_register_types (PeasObjectModule *module);
_IDE_EXTERN void ide_debugger_register_types    (PeasObjectModule *module);
_IDE_EXTERN void ide_directory_register_types   (PeasObjectModule *module);
_IDE_EXTERN void ide_editor_register_types      (PeasObjectModule *module);
_IDE_EXTERN void ide_test_register_types        (PeasObjectModule *module);
_IDE_EXTERN void ide_webkit_register_types      (PeasObjectModule *module);

G_END_DECLS
