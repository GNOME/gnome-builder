/* gbp-rubocop-diagnostic-provider.c
 *
 * Copyright 2021 Jeremy Wilkins <jeb@jdwilkins.co.uk>
 * Copyright 2022 Veli Tasalı <me@velitasali.com>
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

#define G_LOG_DOMAIN "gbp-rubocop-diagnostic-provider"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-rubocop-diagnostic-provider.h"

struct _GbpRubocopDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
  guint is_stdin : 1;
};

G_DEFINE_FINAL_TYPE (GbpRubocopDiagnosticProvider, gbp_rubocop_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
gbp_rubocop_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                     IdeRunContext      *run_context,
                                                     GFile              *file,
                                                     GBytes             *contents,
                                                     const char         *language_id,
                                                     GError            **error)
{
  GbpRubocopDiagnosticProvider *self = (GbpRubocopDiagnosticProvider *)tool;

  g_assert (GBP_IS_RUBOCOP_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (gbp_rubocop_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    return FALSE;

  ide_run_context_append_args (run_context, IDE_STRV_INIT ("--format", "json"));
  self->is_stdin = contents != NULL;
  if (self->is_stdin)
    ide_run_context_append_argv (run_context, "--stdin");
  ide_run_context_append_argv (run_context, g_file_peek_path (file));

  return TRUE;
}

static IdeDiagnosticSeverity
parse_severity (const char *str)
{
  if (ide_str_empty0 (str) ||
      ide_str_equal0 (str, "info") ||
      ide_str_equal0 (str, "refactor") ||
      ide_str_equal0 (str, "convention"))
    return IDE_DIAGNOSTIC_NOTE;

  if (ide_str_equal0 (str, "warning"))
    return IDE_DIAGNOSTIC_WARNING;

  if (ide_str_equal0 (str, "error"))
    return IDE_DIAGNOSTIC_ERROR;

  if (ide_str_equal0 (str, "fatal"))
    return IDE_DIAGNOSTIC_FATAL;

  return IDE_DIAGNOSTIC_NOTE;
}

static void
gbp_rubocop_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                      IdeDiagnostics    *diagnostics,
                                                      GFile             *file,
                                                      const char        *stdout_buf,
                                                      const char        *stderr_buf)
{
  GbpRubocopDiagnosticProvider *self = (GbpRubocopDiagnosticProvider *)tool;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  JsonObject *root_obj;
  JsonNode *root;
  JsonArray *files;

  g_assert (GBP_IS_RUBOCOP_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_DIAGNOSTICS (diagnostics));
  g_assert (G_IS_FILE (file));

  if (ide_str_empty0 (stdout_buf))
    return;

  parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, stdout_buf, -1, &error))
    {
      g_debug ("%s", error->message);
      return;
    }


  if ((root = json_parser_get_root (parser)) &&
      JSON_NODE_HOLDS_OBJECT (root) &&
      (root_obj = json_node_get_object (root)) &&
      (files = json_object_get_array_member (root_obj, "files")))
    {
      guint n_files = json_array_get_length (files);

      for (guint i = 0; i < n_files; i++)
        {
          JsonObject *item = json_array_get_object_element (files, i);
          JsonArray *offenses = json_object_get_array_member (item, "offenses");
          guint n_offenses = json_array_get_length (offenses);

          for (guint j = 0; j < n_offenses; j++)
            {
              g_autoptr(IdeDiagnostic) diagnostic = NULL;
              g_autoptr(IdeLocation) start = NULL;
              g_autoptr(IdeLocation) end = NULL;
              g_autofree char *full_message = NULL;
              JsonObject *offense = json_array_get_object_element (offenses, j);
              JsonObject *location;
              IdeDiagnosticSeverity severity;
              const char *message;
              guint start_line;
              guint start_col;
              guint end_line;
              guint end_col;

              if (!json_object_has_member (offense, "location"))
                continue;

              location = json_object_get_object_member (offense, "location");

              if (!json_object_has_member (location, "start_line") ||
                  !json_object_has_member (location, "start_column"))
                continue;

              start_line = MAX (json_object_get_int_member (location, "start_line") - 1, 0);
              start_col = MAX (json_object_get_int_member (location, "start_column") - 1, 0);
              start = ide_location_new (file, start_line, start_col);

              if (json_object_has_member (location, "last_line"))
                {
                  end_line = MAX (json_object_get_int_member (location, "last_line") - 1, 0);
                  end_col = MAX (json_object_get_int_member (location, "last_column") - 1, 0);
                  end = ide_location_new (file, end_line, end_col);
                }
              else
                {
                  end_line = start_line;
                  end_col = start_col + json_object_get_int_member (location, "length");
                  end = ide_location_new (file, end_line, end_col);
                }

              severity = parse_severity (json_object_get_string_member (offense, "severity"));
              message = json_object_get_string_member (offense, "message");

              if (self->is_stdin)
                {
                  const char *cop_name = json_object_get_string_member (offense, "cop_name");
                  message = full_message = g_strdup_printf ("%s: %s", cop_name, message);
                }

              diagnostic = ide_diagnostic_new (severity, message, start);
              ide_diagnostic_take_range (diagnostic, ide_range_new (start, end));
              ide_diagnostics_add (diagnostics, diagnostic);
            }
        }
    }
}

static void
gbp_rubocop_diagnostic_provider_class_init (GbpRubocopDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_rubocop_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_rubocop_diagnostic_provider_populate_diagnostics;
}

static void
gbp_rubocop_diagnostic_provider_init (GbpRubocopDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "rubocop");
}
