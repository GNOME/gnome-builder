/* python-pack-plugin.c
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

#include "ide-python-indenter.h"

void _ide_python_indenter_register_type (GTypeModule *module);

void
ide_python_pack_register_types (PeasObjectModule *module)
{
  _ide_python_indenter_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module, IDE_TYPE_INDENTER, IDE_TYPE_PYTHON_INDENTER);
}
