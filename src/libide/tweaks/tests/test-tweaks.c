/* test-tweaks.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <unistd.h>

#include <gtksourceview/gtksource.h>

#include <libide-tweaks.h>

#include "ide-tweaks-init.h"
#include "ide-tweaks-item-private.h"

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(IdeTweaks) tweaks = NULL;
  g_autoptr(GString) string = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *expected = NULL;
  g_autofree char *expected_contents = NULL;
  g_autoptr(GtkCssProvider) css = NULL;
  gboolean display = FALSE;
  gsize len = 0;
  const GOptionEntry entries[] = {
    { "expected", 'e', 0, G_OPTION_ARG_FILENAME, &expected, "File containing expected output" },
    { "display", 'd', 0, G_OPTION_ARG_NONE, &display, "Display a window containin the tweaks" },
    { NULL }
  };

  gtk_init ();
  adw_init ();
  gtk_source_init ();
  _ide_tweaks_init ();

  context = g_option_context_new ("- test tweaks ui merging");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gtk_icon_theme_add_search_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                                  PACKAGE_ICONDIR);

  css = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css, "/org/gnome/libide-tweaks/style.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (css),
                                              GTK_STYLE_PROVIDER_PRIORITY_THEME+1);

  tweaks = ide_tweaks_new ();
  string = g_string_new (NULL);

  /* Test with languages exposed */
  {
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
    const char * const *ids = gtk_source_language_manager_get_language_ids (lm);
    const char * const *allowed = IDE_STRV_INIT ("c", "chdr", "css", "xml");
    g_autoptr(GListStore) languages = NULL;

    languages = g_list_store_new (GTK_SOURCE_TYPE_LANGUAGE);

    for (guint i = 0; ids[i]; i++)
      {
        if (g_strv_contains (allowed, ids[i]))
          g_list_store_append (languages, gtk_source_language_manager_get_language (lm, ids[i]));
      }

    ide_tweaks_expose_object (tweaks, "GtkSourceLanguages", G_OBJECT (languages));
  }

  for (guint i = 1; i < argc; i++)
    {
      const char *path = argv[i];
      g_autoptr(GFile) file = g_file_new_for_commandline_arg (path);

      if (!ide_tweaks_load_from_file (tweaks, file, NULL, &error))
        g_error ("Failed to parse %s: %s", path, error->message);
    }

  _ide_tweaks_item_printf (IDE_TWEAKS_ITEM (tweaks), string, 0);

  if (!expected)
    g_print ("%s", string->str);

  if (expected)
    {
      if (!g_file_get_contents (expected, &expected_contents, &len, &error))
        g_error ("Failed to load expected contents: %s: %s", expected, error->message);

      if (!ide_str_equal0 (expected_contents, string->str))
        {
          g_printerr ("Contents did not match.\n"
                      "\n"
                      "Expected:\n"
                      "=========\n"
                      "%s\n"
                      "\n"
                      "Got:\n"
                      "====\n"
                      "%s\n",
                      expected_contents,
                      string->str);
          return EXIT_FAILURE;
        }
    }

  if (display)
    {
      GtkWidget *window = ide_tweaks_window_new ();
      g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);

      ide_tweaks_window_set_tweaks (IDE_TWEAKS_WINDOW (window), tweaks);

      g_signal_connect_swapped (window,
                                "close-request",
                                G_CALLBACK (g_main_loop_quit),
                                main_loop);
      gtk_window_present (GTK_WINDOW (window));
      g_main_loop_run (main_loop);
    }

  return EXIT_SUCCESS;
}
