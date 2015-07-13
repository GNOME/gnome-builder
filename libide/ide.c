/* ide.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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
#include <libgit2-glib/ggit.h>
#include <stdlib.h>

#include "gconstructor.h"
#include "ide.h"

#include "ide-autotools-build-system.h"
#include "ide-autotools-project-miner.h"
#include "ide-directory-build-system.h"
#include "ide-directory-vcs.h"
#include "ide-editorconfig-file-settings.h"
#include "ide-file-settings.h"
#include "ide-git-vcs.h"
#include "ide-gjs-script.h"
#include "ide-gsettings-file-settings.h"
#include "ide-modelines-file-settings.h"
#include "ide-internal.h"
#include "ide-project-miner.h"
#include "ide-pygobject-script.h"
#include "ide-search-provider.h"

#include "modeline-parser.h"

static gboolean     gProgramNameRead;
static const gchar *gProgramName = "libide";

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
  gProgramNameRead = 1;
  return gProgramName;
}

void
ide_set_program_name (const gchar *program_name)
{
  if (gProgramNameRead)
    {
      g_warning (_("You must call %s() before using libide."), G_STRFUNC);
      return;
    }

  gProgramName = g_intern_string (program_name);
}

static void
ide_init_ctor (void)
{
  GgitFeatureFlags ggit_flags;

  g_irepository_prepend_search_path (LIBDIR"/gnome-builder/girepository-1.0");

  g_type_ensure (IDE_TYPE_CONTEXT);
  g_type_ensure (IDE_TYPE_BUILD_SYSTEM);
  g_type_ensure (IDE_TYPE_VCS);

  g_io_extension_point_register (IDE_BUILD_SYSTEM_EXTENSION_POINT);
  g_io_extension_point_register (IDE_FILE_SETTINGS_EXTENSION_POINT);
  g_io_extension_point_register (IDE_PROJECT_MINER_EXTENSION_POINT);
  g_io_extension_point_register (IDE_SCRIPT_EXTENSION_POINT);
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
                                  IDE_TYPE_MODELINES_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".modelines",
                                  -100);
  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_EDITORCONFIG_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".editorconfig",
                                  -200);
  g_io_extension_point_implement (IDE_FILE_SETTINGS_EXTENSION_POINT,
                                  IDE_TYPE_GSETTINGS_FILE_SETTINGS,
                                  IDE_FILE_SETTINGS_EXTENSION_POINT".gsettings",
                                  -300);

  g_io_extension_point_implement (IDE_PROJECT_MINER_EXTENSION_POINT,
                                  IDE_TYPE_AUTOTOOLS_PROJECT_MINER,
                                  IDE_PROJECT_MINER_EXTENSION_POINT".autotools",
                                  0);

  g_io_extension_point_implement (IDE_SCRIPT_EXTENSION_POINT,
                                  IDE_TYPE_GJS_SCRIPT,
                                  IDE_SCRIPT_EXTENSION_POINT".gjs",
                                  -100);

  g_io_extension_point_implement (IDE_SCRIPT_EXTENSION_POINT,
                                  IDE_TYPE_PYGOBJECT_SCRIPT,
                                  IDE_SCRIPT_EXTENSION_POINT".py",
                                  -100);

  g_io_extension_point_implement (IDE_VCS_EXTENSION_POINT,
                                  IDE_TYPE_GIT_VCS,
                                  IDE_VCS_EXTENSION_POINT".git",
                                  -100);
  g_io_extension_point_implement (IDE_VCS_EXTENSION_POINT,
                                  IDE_TYPE_DIRECTORY_VCS,
                                  IDE_VCS_EXTENSION_POINT".directory",
                                  -200);

  modeline_parser_init ();

  ggit_init ();

  ggit_flags = ggit_get_features ();

  if ((ggit_flags & GGIT_FEATURE_THREADS) == 0)
    {
      g_error (_("Builder requires libgit2-glib with threading support."));
      exit (EXIT_FAILURE);
    }

  if ((ggit_flags & GGIT_FEATURE_SSH) == 0)
    {
      g_error (_("Builder requires libgit2-glib with SSH support."));
      exit (EXIT_FAILURE);
    }

  _ide_thread_pool_init ();

  _ide_battery_monitor_init ();
}
