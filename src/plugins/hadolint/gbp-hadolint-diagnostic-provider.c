/*
 * gbp-hadolint-diagnostic-provider.c
 *
 * Copyright 2023 JCWasmx86 <JCWasmx86@t-online.de>
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

#define G_LOG_DOMAIN "gbp-hadolint-diagnostic-provider"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-hadolint-diagnostic-provider.h"

struct _GbpHadolintDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpHadolintDiagnosticProvider, gbp_hadolint_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
gbp_hadolint_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                      IdeRunContext      *run_context,
                                                      GFile              *file,
                                                      GBytes             *contents,
                                                      const char         *language_id,
                                                      GError            **error)
{
  GbpHadolintDiagnosticProvider *self = (GbpHadolintDiagnosticProvider *)tool;

  g_assert (GBP_IS_HADOLINT_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (gbp_hadolint_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    return FALSE;

  ide_run_context_append_argv (run_context, "--format=json");

  if (contents != NULL)
    ide_run_context_append_argv (run_context, "-");
  else
    ide_run_context_append_argv (run_context, g_file_peek_path (file));

  return TRUE;
}

static inline IdeDiagnosticSeverity
parse_severity (const char *level)
{
  if (ide_str_equal0 (level, "error"))
    return IDE_DIAGNOSTIC_ERROR;

  if (ide_str_equal0 (level, "warning"))
    return IDE_DIAGNOSTIC_WARNING;

  if (ide_str_equal0 (level, "info"))
    return IDE_DIAGNOSTIC_NOTE;

  if (ide_str_equal0 (level, "style"))
    return IDE_DIAGNOSTIC_NOTE;

  return IDE_DIAGNOSTIC_NOTE;
}

static void
gbp_hadolint_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                       IdeDiagnostics    *diagnostics,
                                                       GFile             *file,
                                                       const char        *stdout_buf,
                                                       const char        *stderr_buf)
{
  GbpHadolintDiagnosticProvider *self = (GbpHadolintDiagnosticProvider *)tool;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  JsonNode *root;
  JsonArray *results;

  g_assert (GBP_IS_HADOLINT_DIAGNOSTIC_PROVIDER (self));
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
          JsonObject *message = json_array_get_object_element (results, r);
          g_autoptr(IdeDiagnostic) diagnostic = NULL;
          g_autoptr(IdeLocation) start = NULL;
          g_autoptr(IdeLocation) end = NULL;
          IdeDiagnosticSeverity severity;
          const char *level;
          guint start_line;
          guint start_col;

          if (!json_object_has_member (message, "file") ||
              !json_object_has_member (message, "line"))
            continue;

          start_line = MAX (json_object_get_int_member (message, "line"), 1) - 1;
          start_col = MAX (json_object_get_int_member (message, "column"), 1) - 1;
          start = ide_location_new (file, start_line, start_col);

          if (json_object_has_member (message, "endLine") &&
              json_object_has_member (message, "endColumn"))
            {
              guint end_line = MAX (json_object_get_int_member (message, "endLine"), 1) - 1;
              guint end_col = MAX (json_object_get_int_member (message, "endColumn"), 1) - 1;

              end = ide_location_new (file, end_line, end_col);
            }

          if ((level = json_object_get_string_member (message, "level")))
            severity = parse_severity (level);
          else
            severity = IDE_DIAGNOSTIC_ERROR;

          diagnostic = ide_diagnostic_new (severity,
                                           json_object_get_string_member (message, "message"),
                                           start);

          if (end != NULL)
            ide_diagnostic_take_range (diagnostic, ide_range_new (start, end));

          ide_diagnostics_add (diagnostics, diagnostic);
        }
    }
}

static void
gbp_hadolint_diagnostic_provider_class_init (GbpHadolintDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_hadolint_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_hadolint_diagnostic_provider_populate_diagnostics;
}

static void
gbp_hadolint_diagnostic_provider_init (GbpHadolintDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "hadolint");
}
