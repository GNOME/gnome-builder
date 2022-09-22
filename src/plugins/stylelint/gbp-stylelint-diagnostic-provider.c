/* gbp-stylelint-diagnostic-provider.c
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

#define G_LOG_DOMAIN "gbp-stylelint-diagnostic-provider"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-stylelint-diagnostic-provider.h"

struct _GbpStylelintDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpStylelintDiagnosticProvider, gbp_stylelint_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
gbp_stylelint_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                       IdeRunContext      *run_context,
                                                       GFile              *file,
                                                       GBytes             *contents,
                                                       const char         *language_id,
                                                       GError            **error)
{
  GbpStylelintDiagnosticProvider *self = (GbpStylelintDiagnosticProvider *)tool;

  g_assert (GBP_IS_STYLELINT_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (gbp_stylelint_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    return FALSE;

  ide_run_context_append_args (run_context, IDE_STRV_INIT ("--formatter", "json"));
  if (contents != NULL)
    ide_run_context_append_args (run_context, IDE_STRV_INIT ("--stdin", "--stdin-filename"));
  ide_run_context_append_argv (run_context, g_file_peek_path (file));

  return TRUE;
}

static IdeDiagnosticSeverity
parse_severity (const char *str)
{
  if (ide_str_equal0 (str, "warning"))
    return IDE_DIAGNOSTIC_WARNING;

  if (ide_str_equal0 (str, "error"))
    return IDE_DIAGNOSTIC_ERROR;

  return IDE_DIAGNOSTIC_NOTE;
}

static void
gbp_stylelint_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                      IdeDiagnostics    *diagnostics,
                                                      GFile             *file,
                                                      const char        *stdout_buf,
                                                      const char        *stderr_buf)
{
  GbpStylelintDiagnosticProvider *self = (GbpStylelintDiagnosticProvider *)tool;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  JsonNode *root;
  JsonArray *results;

  g_assert (GBP_IS_STYLELINT_DIAGNOSTIC_PROVIDER (self));
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
      JSON_NODE_HOLDS_ARRAY (root) &&
      (results = json_node_get_array (root)))
    {
      guint n_results = json_array_get_length (results);

      for (guint r = 0; r < n_results; r++)
        {
          JsonObject *result = json_array_get_object_element (results, r);
          JsonArray *warnings = json_object_get_array_member (result, "warnings");
          guint n_warnings;

          if (warnings == NULL)
            continue;

          n_warnings = json_array_get_length (warnings);

          for (guint w = 0; w < n_warnings; w++)
            {
              JsonObject *warning = json_array_get_object_element (warnings, w);
              g_autoptr(IdeDiagnostic) diagnostic = NULL;
              g_autoptr(IdeLocation) start = NULL;
              IdeDiagnosticSeverity severity;
              const char *message;
              guint start_line;
              guint start_col;

              if (!json_object_has_member (warning, "line") ||
                  !json_object_has_member (warning, "column"))
                continue;

              start_line = MAX (json_object_get_int_member (warning, "line"), 1) - 1;
              start_col = MAX (json_object_get_int_member (warning, "column"), 1) - 1;
              start = ide_location_new (file, start_line, start_col);
              severity = parse_severity (json_object_get_string_member (warning, "severity"));
              message = json_object_get_string_member (warning, "text");

              diagnostic = ide_diagnostic_new (severity, message, start);
              ide_diagnostics_add (diagnostics, diagnostic);
            }
        }
    }
}

static void
gbp_stylelint_diagnostic_provider_class_init (GbpStylelintDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_stylelint_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_stylelint_diagnostic_provider_populate_diagnostics;
}

static void
gbp_stylelint_diagnostic_provider_init (GbpStylelintDiagnosticProvider *self)
{
  g_autofree char *local_program_path = g_build_filename ("node_modules", ".bin", "stylelint", NULL);

  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "stylelint");
  ide_diagnostic_tool_set_local_program_path (IDE_DIAGNOSTIC_TOOL (self), local_program_path);
}
