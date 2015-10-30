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

#include "gb-plugins.h"

static GMainLoop *main_loop;
static gint exit_code = EXIT_SUCCESS;
static IdeContext *ide_context;
static const gchar *path;

static void
quit (gint code)
{
  exit_code = code;
  g_clear_object (&ide_context);
  g_main_loop_quit (main_loop);
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

  ide_context = g_object_ref (context);

  build_system = ide_context_get_build_system (context);
  project = ide_context_get_project (context);
  file = ide_project_get_file_for_path (project, path);

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

  ide_log_init (TRUE, NULL);

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-build");

  context = g_option_context_new (_("- Get build flags for a project file"));
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  if (argc > 2)
    {
      project_path = argv [1];
      path = argv [2];
    }
  else if (argc > 1)
    {
      project_path = ".";
      path = argv [1];
    }
  else
    {
      g_printerr ("usage: %s [configure.ac|PROJECT_FILE] FILE\n", argv [0]);
      return EXIT_FAILURE;
    }

  project_file = g_file_new_for_path (project_path);

  gb_plugins_init (NULL);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (main_loop);
  g_clear_pointer (&main_loop, g_main_loop_unref);

  return exit_code;
}
