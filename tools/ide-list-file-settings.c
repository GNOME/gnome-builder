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

#include "gb-plugins.h"

static GMainLoop *main_loop;
static gint exit_code = EXIT_SUCCESS;
static gchar **paths;
static int active;
static IdeContext *ide_context;

static void
quit (gint code)
{
  exit_code = code;
  g_main_loop_quit (main_loop);
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
print_settings (IdeFileSettings *settings)
{
  IdeFile *file = ide_file_settings_get_file (settings);

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
  g_print ("show_right_margin = %s\n", ide_file_settings_get_show_right_margin (settings) ? "true" : "false");
}

static void
unref_job (void)
{
  if (!--active)
    quit (exit_code);
}

static void
settled_cb (IdeFileSettings *file_settings,
            GParamSpec      *pspec,
            gpointer         data)
{
  g_signal_handlers_disconnect_by_func (file_settings, settled_cb, NULL);
  print_settings (file_settings);
  g_clear_object (&file_settings);
  unref_job ();
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
      exit_code = EXIT_FAILURE;
      goto cleanup;
    }

  if (!ide_file_settings_get_settled (settings))
    {
      g_signal_connect (settings,
                        "notify::settled",
                        G_CALLBACK (settled_cb),
                        NULL);
      return;
    }

  print_settings (settings);
  g_clear_object (&settings);

cleanup:
  unref_job ();
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

  if (paths)
    {
      for (i = 0; paths [i]; i++)
        {
          active++;
          file = ide_project_get_file_for_path (project, paths [i]);
          ide_file_load_settings_async (file,
                                        NULL,
                                        load_settings_cb,
                                        NULL);
        }
    }

  if (!active)
    {
      g_printerr (_("No files provided to load settings for.\n"));
      quit (EXIT_FAILURE);
    }

  ide_context = g_object_ref (context);
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

  gtk_init (&argc, &argv);

  ide_log_init (TRUE, NULL);

  context = g_option_context_new (_("- List files found in project."));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  project_path = argv [1];

  project_file = g_file_new_for_path (project_path);

  strv = g_ptr_array_new ();
  for (i = 1; i < argc; i++)
    g_ptr_array_add (strv, g_strdup (argv [i]));
  g_ptr_array_add (strv, NULL);

  paths = (gchar **)g_ptr_array_free (strv, FALSE);

  gb_plugins_init (NULL);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (main_loop);
  g_clear_pointer (&main_loop, g_main_loop_unref);
  g_strfreev (paths);
  g_clear_object (&context);

  return exit_code;
}
