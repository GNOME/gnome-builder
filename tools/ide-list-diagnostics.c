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
static GFile *gFile;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static const gchar *
severity_to_string (IdeDiagnosticSeverity severity)
{
  if (isatty (STDOUT_FILENO))
    {
      switch (severity)
        {
        case IDE_DIAGNOSTIC_IGNORED: return "ignored:";
        case IDE_DIAGNOSTIC_NOTE:    return "note:";
        case IDE_DIAGNOSTIC_WARNING: return "\033[1;35mwarning:\033[0m";
        case IDE_DIAGNOSTIC_ERROR:   return "\033[1;31merror:\033[0m";
        case IDE_DIAGNOSTIC_FATAL:   return "\033[1;31mfatal error:\033[0m";
        default: return "";
        }
    }
  else
    {
      switch (severity)
        {
        case IDE_DIAGNOSTIC_IGNORED: return "ignored:";
        case IDE_DIAGNOSTIC_NOTE:    return "note:";
        case IDE_DIAGNOSTIC_WARNING: return "warning:";
        case IDE_DIAGNOSTIC_ERROR:   return "error:";
        case IDE_DIAGNOSTIC_FATAL:   return "fatal error:";
        default: return "";
        }
    }
}

static gchar *
get_line (GFile *file,
          guint  line)
{
  g_autoptr(gchar) contents = NULL;
  gchar **lines;
  gchar *ret = NULL;
  gsize len;

  g_file_load_contents (file, NULL, &contents, &len, NULL, NULL);

  lines = g_strsplit (contents, "\n", line+2);

  if (g_strv_length (lines) > line)
    ret = g_strdup (lines [line]);

  return ret;
}

static void
print_diagnostic (IdeDiagnostic *diag)
{
  IdeSourceLocation *location;
  IdeFile *file;
  const gchar *text;
  g_autoptr(gchar) path = NULL;
  IdeDiagnosticSeverity severity;
  GFile *gfile;
  gsize num_ranges;
  guint line;
  guint column;
  g_autoptr(gchar) linestr = NULL;
  gsize i;

  text = ide_diagnostic_get_text (diag);
  num_ranges = ide_diagnostic_get_num_ranges (diag);
  severity = ide_diagnostic_get_severity (diag);

  location = ide_diagnostic_get_location (diag);
  file = ide_source_location_get_file (location);
  gfile = ide_file_get_file (file);
  path = g_file_get_path (gfile);
  line = ide_source_location_get_line (location);
  column = ide_source_location_get_line_offset (location);

  if (isatty (STDOUT_FILENO))
    g_print ("\033[1m%s:%u:%u:\033[0m %s \033[1m%s\033[0m\n",
             path, line+1, column+1,
             severity_to_string (severity),
             text);
  else
    g_print ("%s:%u:%u: %s %s\n",
             path, line+1, column+1,
             severity_to_string (severity),
             text);

  linestr = get_line (gfile, line);

  if (linestr)
    {
      gsize i;

      g_print ("%s\n", linestr);

      for (i = 0; i < column; i++)
        g_print (" ");

      if (isatty (STDOUT_FILENO))
        g_print ("\033[1;32m^\033[0m\n");
      else
        g_print ("^\n");
    }

  for (i = 0; i < num_ranges; i++)
    {
#if 0
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
#endif
    }
}

static void
diagnose_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  IdeDiagnostician *diagnostician = (IdeDiagnostician *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeDiagnostics) ret = NULL;
  guint error_count = 0;
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
      IdeDiagnosticSeverity severity;

      diag = ide_diagnostics_index (ret, i);
      severity = ide_diagnostic_get_severity (diag);

      if ((severity == IDE_DIAGNOSTIC_ERROR) ||
          (severity == IDE_DIAGNOSTIC_FATAL))
        error_count++;

      print_diagnostic (diag);
    }

  if (error_count)
    g_print ("%u error generated.\n", error_count);

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
  IdeLanguage *language;
  IdeProject *project;
  IdeFile *file;
  IdeVcs *vcs;
  GFile *workdir;
  g_autoptr(gchar) relpath = NULL;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  project = ide_context_get_project (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  relpath = g_file_get_relative_path (workdir, gFile);

  ide_project_reader_lock (project);
  file = ide_project_get_file_for_path (project, relpath);
  ide_project_reader_unlock (project);

  if (!file)
    {
      g_printerr (_("No such file in project: %s\n"), relpath);
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
  const gchar *path;

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
      path = argv [1];
    }
  else if (argc == 3)
    {
      project_path = argv [1];
      path = argv [2];
    }
  else
    {
      g_printerr ("usage: %s [PROJECT_FILE] TARGET_FILE\n", argv [0]);
      return EXIT_FAILURE;
    }

  gFile = g_file_new_for_path (path);

  project_file = g_file_new_for_path (project_path);
  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);

  g_clear_object (&project_file);
  g_clear_object (&gFile);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
