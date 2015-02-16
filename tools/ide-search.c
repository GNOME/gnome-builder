/* ide-search.c
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
static gchar *gSearchTerms;
static gsize gCount;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static void
on_result_added_cb (IdeSearchContext  *search_context,
                    IdeSearchProvider *provider,
                    IdeSearchResult   *result,
                    gpointer           user_data)
{
  gCount++;

  g_print ("Result: %s\n", g_type_name (G_TYPE_FROM_INSTANCE (result)));
}

static void
on_completed_cb (IdeSearchContext *search_context)
{
  gchar *line;
  guint len;
  guint i;

  line = g_strdup_printf (_("%"G_GSIZE_FORMAT" results"), gCount);
  len = strlen (line);
  for (i = 0; i < len; i++)
    g_printerr ("=");
  g_printerr ("\n");
  g_printerr ("%s\n", line);

  g_object_unref (search_context);
  quit (gExitCode);
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeSearchContext *search_context = NULL;
  IdeSearchEngine *search_engine;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  search_engine = ide_context_get_search_engine (context);
  search_context = ide_search_engine_search (search_engine, NULL, gSearchTerms);
  /* FIXME: ^ search terms duplicated */

  g_signal_connect (search_context, "result-added",
                    G_CALLBACK (on_result_added_cb), NULL);
  g_signal_connect (search_context, "completed",
                    G_CALLBACK (on_completed_cb), NULL);

  ide_search_context_execute (search_context, gSearchTerms);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";
  GString *search_terms;
  gint i;

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-search");

  context = g_option_context_new (_("PROJECT_FILE [SEARCH TERMS...]"));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc > 1)
    project_path = argv [1];
  project_file = g_file_new_for_path (project_path);

  search_terms = g_string_new (NULL);
  for (i = 2; i < argc; i++)
    g_string_append_printf (search_terms, " %s", argv [i]);
  gSearchTerms = g_string_free (search_terms, FALSE);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);
  g_clear_pointer (&gSearchTerms, g_free);

  return gExitCode;
}
