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
  const gchar *title;
  const gchar *subtitle;

  gCount++;

  title = ide_search_result_get_title (result);
  subtitle = ide_search_result_get_subtitle (result);

  g_print ("%s\n", title);
  g_print ("%s\n", subtitle);
  g_print ("------------------------------------------------------------\n");

}

static void
on_completed_cb (IdeSearchContext *search_context,
                 IdeContext       *context)
{
  gchar *gCount_str;
  gCount_str = g_strdup_printf ("%"G_GSIZE_FORMAT, gCount);
  g_print (_("%s results\n"), gCount_str);
  g_free (gCount_str);
  g_object_unref (context);
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
  search_context = ide_search_engine_search (search_engine, gSearchTerms);
  /* FIXME: ^ search terms duplicated */

  g_signal_connect (search_context, "result-added",
                    G_CALLBACK (on_result_added_cb), NULL);
  g_signal_connect (search_context, "completed",
                    G_CALLBACK (on_completed_cb),
                    g_object_ref (context));

  ide_search_context_execute (search_context, gSearchTerms, 0);
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
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

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
