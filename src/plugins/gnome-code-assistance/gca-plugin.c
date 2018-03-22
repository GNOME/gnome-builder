/* gca-plugin.c
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

#include "ide-gca-diagnostic-provider.h"
#include "ide-gca-preferences-addin.h"
#include "ide-gca-service.h"

void
ide_gca_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_SERVICE,
                                              IDE_TYPE_GCA_SERVICE);

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                              IDE_TYPE_GCA_DIAGNOSTIC_PROVIDER);

  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_PREFERENCES_ADDIN,
                                              IDE_TYPE_GCA_PREFERENCES_ADDIN);
}
