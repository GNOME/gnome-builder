/* ide.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "gconstructor.h"
#include "ide.h"

#include "ide-autotools-build-system.h"
#include "ide-c-language.h"
#include "ide-clang-service.h"
#include "ide-directory-build-system.h"
#include "ide-directory-vcs.h"
#include "ide-editorconfig-file-settings.h"
#include "ide-file-settings.h"
#include "ide-gca-service.h"
#include "ide-git-vcs.h"
#include "ide-gsettings-file-settings.h"
#include "ide-gjs-script.h"

static gboolean     gProgramNameRead;
static const gchar *gProgramName = "libide";

const gchar *
ide_get_program_name (void)
{
  gProgramNameRead = 1;
  return gProgramName;
}

void
ide_set_program_name (const gchar *program_name)
{
  if (gProgramNameRead)
    {
      g_warning (_("You must call %s() before using libide."),
                 G_STRFUNC);
      return;
    }

  gProgramName = g_intern_string (program_name);
}

static void
ide_init_ctor (void)
{
  g_type_ensure (IDE_TYPE_CONTEXT);
  g_type_ensure (IDE_TYPE_BUILD_SYSTEM);
  g_type_ensure (IDE_TYPE_VCS);

  g_io_extension_point_register (IDE_BUILD_SYSTEM_EXTENSION_POINT);
  g_io_extension_point_register (IDE_FILE_SETTINGS_EXTENSION_POINT);
  g_io_extension_point_register (IDE_LANGUAGE_EXTENSION_POINT);
  g_io_extension_point_register (IDE_SCRIPT_EXTENSION_POINT);
  g_io_extension_point_register (IDE_SERVICE_EXTENSION_POINT);
  g_io_extension_point_register (IDE_VCS_EXTENSION_POINT);

  g_io_extension_point_implement (IDE_BUILD_SYSTEM_EXTENSION_POINT,
                                  IDE_TYPE_AUTOTOOLS_BUILD_SYSTEM,
                                  IDE_BUILD_SYSTEM_EXTENSION_POINT".autotools",
                                  -100);
  g_io_extension_point_implement (IDE_BUILD_SYSTEM_EXTENSION_POINT,
                                  IDE_TYPE_DIRECTORY_BUILD_SYSTEM,
                                  IDE_BUILD_SYSTEM_EXTENSION_POINT".directory",
                                  -200);

  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_EDITORCONFIG_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".editorconfig",
                                  -100);
  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_GSETTINGS_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".gsettings",
                                  -200);

  g_io_extension_point_implement (IDE_LANGUAGE_EXTENSION_POINT,
                                  IDE_TYPE_C_LANGUAGE,
                                  IDE_LANGUAGE_EXTENSION_POINT".c",
                                  -100);

  g_io_extension_point_implement (IDE_SCRIPT_EXTENSION_POINT,
                                  IDE_TYPE_GJS_SCRIPT,
                                  IDE_SCRIPT_EXTENSION_POINT".gjs",
                                  -100);

  g_io_extension_point_implement (IDE_SERVICE_EXTENSION_POINT,
                                  IDE_TYPE_CLANG_SERVICE,
                                  IDE_SERVICE_EXTENSION_POINT".clang",
                                  -100);
  g_io_extension_point_implement (IDE_SERVICE_EXTENSION_POINT,
                                  IDE_TYPE_GCA_SERVICE,
                                  IDE_SERVICE_EXTENSION_POINT".gca",
                                  -200);

  g_io_extension_point_implement (IDE_VCS_EXTENSION_POINT,
                                  IDE_TYPE_GIT_VCS,
                                  IDE_VCS_EXTENSION_POINT".git",
                                  -100);
  g_io_extension_point_implement (IDE_VCS_EXTENSION_POINT,
                                  IDE_TYPE_DIRECTORY_VCS,
                                  IDE_VCS_EXTENSION_POINT".directory",
                                  -200);

  ggit_init ();
}

#if defined (G_HAS_CONSTRUCTORS)
# ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#  pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(ide_init_ctor)
# endif
G_DEFINE_CONSTRUCTOR(ide_init_ctor)
#else
# error Your platform/compiler is missing constructor support
#endif
