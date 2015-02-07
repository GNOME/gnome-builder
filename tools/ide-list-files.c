/* ide-list-files.c
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

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static void
walk_tree (IdeProjectItem *item,
           gint            depth)
{
  GSequenceIter *iter;
  GSequence *children;

  if (depth == 1 && !IDE_IS_PROJECT_FILES (item))
    return;

  if (IDE_IS_PROJECT_FILE (item))
    {
      GFileInfo *file_info;
      GFileType file_type;
      const gchar *name;
      guint i;

      for (i = 2; i < depth; i++)
        g_print ("  ");

      file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));
      file_type = g_file_info_get_file_type (file_info);
      name = g_file_info_get_display_name (file_info);

      if (file_type == G_FILE_TYPE_DIRECTORY)
        g_print ("%s/\n", name);
      else
        g_print ("%s\n", name);
    }

  children = ide_project_item_get_children (item);

  if (children)
    {
      iter = g_sequence_get_begin_iter (children);

      for (iter = g_sequence_get_begin_iter (children);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter))
        {
          IdeProjectItem *child;

          child = g_sequence_get (iter);
          walk_tree (child, depth + 1);
        }
    }
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeProjectItem *root;
  IdeProject *project;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  project = ide_context_get_project (context);
  root = ide_project_get_root (project);

  walk_tree (root, 0);

  quit (EXIT_SUCCESS);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";

  ide_set_program_name ("gnome-builder");

  context = g_option_context_new (_("- List files found in project."));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc > 1)
    project_path = argv [1];
  project_file = g_file_new_for_path (project_path);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
