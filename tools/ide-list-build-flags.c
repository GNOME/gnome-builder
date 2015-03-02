/* ide-list-build-flags.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <stdlib.h>

static GMainLoop *gMainLoop;
static gint gExitCode = EXIT_SUCCESS;
static IdeContext *gContext;
static const gchar *gPath;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_clear_object (&gContext);
  g_main_loop_quit (gMainLoop);
}

static void
get_flags_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  gchar **flags = NULL;
  g_autoptr(GError) error = NULL;
  gsize i;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));

  flags = ide_build_system_get_build_flags_finish (build_system, result, &error);

  if (error)
    g_printerr ("%s\n", error->message);

  if (flags && flags [0])
    {
      g_print ("%s", flags [0]);
      for (i = 1; flags [i]; i++)
        g_print (" %s", flags [i]);
      g_print ("\n");
    }

  g_strfreev (flags);
  quit (EXIT_SUCCESS);
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeBuildSystem *build_system;
  IdeProject *project;
  g_autoptr(IdeFile) file = NULL;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  gContext = g_object_ref (context);

  build_system = ide_context_get_build_system (context);
  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, gPath);

  ide_build_system_get_build_flags_async (build_system, file, NULL, get_flags_cb, NULL);
}

int
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-build");

  context = g_option_context_new (_("- Get build flags for a project file"));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc > 2)
    {
      project_path = argv [1];
      gPath = argv [2];
    }
  else if (argc > 1)
    {
      project_path = ".";
      gPath = argv [1];
    }
  else
    {
      g_printerr ("usage: %s [configure.ac|PROJECT_FILE] FILE\n", argv [0]);
      return EXIT_FAILURE;
    }

  project_file = g_file_new_for_path (project_path);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
