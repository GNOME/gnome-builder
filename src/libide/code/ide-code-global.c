/* ide-code-global.c
 *
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

#include "ide-file-settings.h"
#include "ide-gsettings-file-settings.h"

#include "../../gconstructor.h"

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_code_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

static void
ide_code_init_ctor (void)
{
  g_io_extension_point_register (IDE_FILE_SETTINGS_EXTENSION_POINT);

  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_GSETTINGS_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".gsettings",
                                  -300);
}
