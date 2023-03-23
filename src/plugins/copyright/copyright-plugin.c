/* copyright-plugin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
 * Copyright 2022 Tristan Partin <tristan@partin.io>
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

#include <libide-core.h>
#include <libide-gui.h>

#include "gbp-copyright-buffer-addin.h"
#include "gbp-copyright-tweaks-addin.h"

_IDE_EXTERN void
_gbp_copyright_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_BUFFER_ADDIN,
                                              GBP_TYPE_COPYRIGHT_BUFFER_ADDIN);
  peas_object_module_register_extension_type (module,
                                              IDE_TYPE_TWEAKS_ADDIN,
                                              GBP_TYPE_COPYRIGHT_TWEAKS_ADDIN);
}
