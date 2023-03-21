/* c-pack-plugin.c
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

#define G_LOG_DOMAIN "c-pack-plugin"

#include "config.h"

#include <libpeas.h>

#include <libide-editor.h>
#include <libide-sourceview.h>

#include "ide-c-indenter.h"
#include "cpack-editor-page-addin.h"

_IDE_EXTERN void
_ide_c_pack_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              GTK_SOURCE_TYPE_INDENTER,
                                              IDE_TYPE_C_INDENTER);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_EDITOR_PAGE_ADDIN,
                                              CPACK_TYPE_EDITOR_PAGE_ADDIN);
}
