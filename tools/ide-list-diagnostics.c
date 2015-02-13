/* ide-list-diagnostics.c
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
static gchar *gPath;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static const gchar *
severity_to_string (IdeDiagnosticSeverity severity)
{
  switch (severity)
    {
    case IDE_DIAGNOSTIC_IGNORED: return "IGNORED";
    case IDE_DIAGNOSTIC_NOTE: return "NOTE";
    case IDE_DIAGNOSTIC_WARNING: return "WARNING";
    case IDE_DIAGNOSTIC_ERROR: return "ERROR";
    case IDE_DIAGNOSTIC_FATAL: return "FATAL";
    default: return "";
    }
}

static void
print_diagnostic (IdeDiagnostic *diag)
{
  IdeSourceLocation *location;
  IdeFile *file;
  const gchar *text;
  const gchar *path;
  IdeDiagnosticSeverity severity;
  gsize i;
  gsize num_ranges;
  guint line;
  guint column;

  text = ide_diagnostic_get_text (diag);
  num_ranges = ide_diagnostic_get_num_ranges (diag);
  severity = ide_diagnostic_get_severity (diag);

  location = ide_diagnostic_get_location (diag);
  file = ide_source_location_get_file (location);
  path = ide_file_get_path (file);
  line = ide_source_location_get_line (location);
  column = ide_source_location_get_line_offset (location);

  g_print ("%s %s:%u:%u: %s\n",
           severity_to_string (severity),
           path, line+1, column+1, text);

#if 0
  for (i = 0; i < num_ranges; i++)
    {
      IdeSourceRange *range;
      IdeSourceLocation *begin;
      IdeSourceLocation *end;
      const gchar *path;
      IdeFile *file;
      guint line;
      guint column;

      range = ide_diagnostic_get_range (diag, i);
      begin = ide_source_range_get_begin (range);
      end = ide_source_range_get_end (range);

      file = ide_source_location_get_file (begin);
      line = ide_source_location_get_line (begin);
      column = ide_source_location_get_line_offset (begin);

      path = ide_file_get_path (file);

      g_print ("%s:%u:%u\n", path, line+1, column+1);
    }

  if (!num_ranges)
    g_print (">> %s\n", text);
#endif
}

static void
diagnose_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeDiagnostician *diagnostician = (IdeDiagnostician *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeDiagnostics) ret = NULL;
  gsize count;
  gsize i;

  ret = ide_diagnostician_diagnose_finish (diagnostician, result, &error);

  if (!ret)
    {
      g_printerr (_("Failed to diagnose: %s\n"), error->message);
      quit (EXIT_FAILURE);
      return;
    }

  count = ide_diagnostics_get_size (ret);

  for (i = 0; i < count; i++)
    {
      IdeDiagnostic *diag;
      diag = ide_diagnostics_index (ret, i);
      print_diagnostic (diag);
    }

  quit (EXIT_SUCCESS);
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeDiagnostician *diagnostician;
  IdeProjectItem *root;
  IdeLanguage *language;
  IdeProject *project;
  IdeFile *file;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  project = ide_context_get_project (context);

  ide_project_reader_lock (project);
  file = ide_project_get_file_for_path (project, gPath);
  ide_project_reader_unlock (project);

  if (!file)
    {
      g_printerr (_("No such file in project: %s\n"), gPath);
      quit (EXIT_FAILURE);
      return;
    }

  language = ide_file_get_language (file);
  diagnostician = ide_language_get_diagnostician (language);

  if (!diagnostician)
    {
      g_printerr (_("No diagnostician for language \"%s\"\n"),
                  ide_language_get_name (language));
      quit (EXIT_FAILURE);
      return;
    }

  ide_diagnostician_diagnose_async (diagnostician,
                                    file,
                                    NULL,
                                    diagnose_cb,
                                    g_object_ref (context));
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
  g_set_prgname ("ide-list-diagnostics");

  context = g_option_context_new (_("- List diagnostics for a file."));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc == 2)
    {
      gPath = argv [1];
    }
  else if (argc == 3)
    {
      project_path = argv [1];
      gPath = argv [2];
    }
  else
    {
      g_printerr ("usage: %s [PROJECT_FILE] TARGET_FILE\n", argv [0]);
      return EXIT_FAILURE;
    }

  project_file = g_file_new_for_path (project_path);
  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);

  g_clear_object (&project_file);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
