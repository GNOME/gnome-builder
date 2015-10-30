/* ide-mine-projects.c
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

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-plugins.h"

static GMainLoop *main_loop;

static void
discover_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeRecentProjects *projects = (IdeRecentProjects *)object;
  GError *error = NULL;
  guint count;
  guint i;

  if (!ide_recent_projects_discover_finish (projects, result, &error))
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      g_main_loop_quit (main_loop);
      return;
    }

  count = g_list_model_get_n_items (G_LIST_MODEL (projects));

  for (i = 0; i < count; i++)
    {
      g_autoptr(IdeProjectInfo) info = NULL;
      GFile *file;
      gchar *path;

      info = g_list_model_get_item (G_LIST_MODEL (projects), i);

      file = ide_project_info_get_file (info);
      path = g_file_get_path (file);

      g_print ("%s (%s)\n", path, ide_project_info_get_name (info));

      g_free (path);
    }

  g_main_loop_quit (main_loop);
}

static gboolean
verbose_cb (void)
{
  ide_log_increase_verbosity ();
  return TRUE;
}

int
main (int    argc,
      gchar *argv[])
{
  static const GOptionEntry entries[] = {
    { "verbose", 'v', G_OPTION_FLAG_NO_ARG|G_OPTION_FLAG_IN_MAIN,
      G_OPTION_ARG_CALLBACK, verbose_cb },
    { NULL }
  };
  IdeRecentProjects *projects;
  GOptionContext *context;
  GError *error = NULL;

  ide_log_init (TRUE, NULL);

  context = g_option_context_new (_("- discover projects"));
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  gb_plugins_init (NULL);

  projects = ide_recent_projects_new ();
  ide_recent_projects_discover_async (projects, NULL, discover_cb, NULL);

  g_main_loop_run (main_loop);

  g_clear_object (&projects);
  g_main_loop_unref (main_loop);
  ide_log_shutdown ();

  return 0;
}
