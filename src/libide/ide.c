/* ide.c
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

#include <girepository.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "gconstructor.h"
#include "ide.h"

#include "files/ide-file-settings.h"
#include "gsettings/ide-gsettings-file-settings.h"
#include "modelines/ide-modelines-file-settings.h"

#ifdef ENABLE_EDITORCONFIG
# include "editorconfig/ide-editorconfig-file-settings.h"
#endif

static gboolean     programNameRead;
static const gchar *programName = "gnome-builder";

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif

const gchar *
ide_get_program_name (void)
{
  programNameRead = 1;
  return programName;
}

void
ide_set_program_name (const gchar *program_name)
{
  if (programNameRead)
    {
      g_warning (_("You must call %s() before using libide."), G_STRFUNC);
      return;
    }

  if (program_name != NULL)
    programName = g_intern_string (program_name);
}

static void
ide_init_ctor (void)
{
  g_io_extension_point_register (IDE_FILE_SETTINGS_EXTENSION_POINT);

  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_MODELINES_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".modelines",
                                  -100);
#ifdef ENABLE_EDITORCONFIG
  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_EDITORCONFIG_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".editorconfig",
                                  -200);
#endif
  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_GSETTINGS_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".gsettings",
                                  -300);
}
