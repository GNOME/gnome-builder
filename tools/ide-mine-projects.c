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

#include "ide-project-miner.h"
#include "autotools/ide-autotools-project-miner.h"

static void
mine_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  IdeProjectMiner *miner = (IdeProjectMiner *)object;
  g_autoptr(GError) error = NULL;
  GMainLoop *main_loop = user_data;

  if (!ide_project_miner_mine_finish (miner, result, &error))
    g_warning ("%s", error->message);

  g_main_loop_quit (main_loop);
}

static void
discovered_cb (IdeProjectMiner *miner,
               IdeProjectInfo  *info)
{
  GFile *file;
  gchar *path;

  file = ide_project_info_get_file (info);
  path = g_file_get_path (file);

  g_print ("%s (%s)\n", path, ide_project_info_get_name (info));

  g_free (path);
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
  IdeProjectMiner *miner;
  GOptionContext *context;
  GMainLoop *main_loop;
  GError *error = NULL;

  ide_log_init (TRUE, NULL);

  context = g_option_context_new (_("- discover projects"));
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  miner = g_object_new (IDE_TYPE_AUTOTOOLS_PROJECT_MINER,
                        "root-directory", NULL,
                        NULL);
  g_signal_connect (miner, "discovered", G_CALLBACK (discovered_cb), NULL);
  main_loop = g_main_loop_new (NULL, FALSE);
  ide_project_miner_mine_async (miner, NULL, mine_cb, main_loop);
  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);

  ide_log_shutdown ();

  return 0;
}
