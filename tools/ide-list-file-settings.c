/* ide-list-file-settings.c
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
static gchar **gPaths;
static int gActive;
static IdeContext *gContext;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static const gchar *
newline_string (GtkSourceNewlineType nt)
{
  switch (nt)
    {
    case GTK_SOURCE_NEWLINE_TYPE_LF:
      return "lf";
    case GTK_SOURCE_NEWLINE_TYPE_CR:
      return "cr";
    case GTK_SOURCE_NEWLINE_TYPE_CR_LF:
      return "crlf";
    default:
      return "unknown";
    }
}

static const gchar *
indent_style_string (IdeIndentStyle style)
{
  switch (style)
    {
    case IDE_INDENT_STYLE_SPACES:
      return "space";
    case IDE_INDENT_STYLE_TABS:
      return "tab";
    default:
      return "unknown";
    }
}

static void
load_settings_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeFileSettings *settings;
  IdeFile *file = (IdeFile *)object;
  GError *error = NULL;

  settings = ide_file_load_settings_finish (file, result, &error);

  if (!settings)
    {
      g_printerr ("%s\n", error->message);
      g_clear_error (&error);
      gExitCode = EXIT_FAILURE;
      goto cleanup;
    }

  g_print ("# %s (%s)\n",
           ide_file_get_path (file),
           g_type_name (G_TYPE_FROM_INSTANCE (settings)));
  g_print ("encoding = %s\n", ide_file_settings_get_encoding (settings) ?: "default");
  g_print ("indent_width = %d\n", ide_file_settings_get_indent_width (settings));
  g_print ("tab_width = %u\n", ide_file_settings_get_tab_width (settings));
  g_print ("insert_trailing_newline = %s\n", ide_file_settings_get_insert_trailing_newline (settings) ? "true" : "false");
  g_print ("trim_trailing_whitespace = %s\n", ide_file_settings_get_trim_trailing_whitespace (settings) ? "true" : "false");
  g_print ("newline_type = %s\n", newline_string (ide_file_settings_get_newline_type (settings)));
  g_print ("indent_sytle = %s\n", indent_style_string (ide_file_settings_get_indent_style (settings)));
  g_print ("right_margin_position = %u\n", ide_file_settings_get_right_margin_position (settings));

  g_clear_object (&settings);

cleanup:
  if (!--gActive)
    quit (gExitCode);
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeProject *project;
  IdeFile *file;
  int i;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  project = ide_context_get_project (context);

  if (gPaths)
    {
      for (i = 0; gPaths [i]; i++)
        {
          gActive++;
          file = ide_project_get_file_for_path (project, gPaths [i]);
          ide_file_load_settings_async (file,
                                        NULL,
                                        load_settings_cb,
                                        NULL);
        }
    }

  if (!gActive)
    {
      g_printerr (_("No files provided to load settings for.\n"));
      quit (EXIT_FAILURE);
    }

  gContext = g_object_ref (context);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";
  GPtrArray *strv;
  int i;

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-list-file-settings");

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

  strv = g_ptr_array_new ();
  for (i = 2; i < argc; i++)
    g_ptr_array_add (strv, g_strdup (argv [i]));
  g_ptr_array_add (strv, NULL);

  gPaths = (gchar **)g_ptr_array_free (strv, FALSE);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);
  g_strfreev (gPaths);
  g_clear_object (&gContext);

  return gExitCode;
}
