/* gbp-eslint-diagnostic-provider.c
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

#define G_LOG_DOMAIN "gbp-eslint-diagnostic-provider"

/* TODO: Comes from typescript-language-server but we'd like to remove
 *       that and push it of to an external plugin.
 */
#define BUNDLED_ESLINT "/app/lib/yarn/global/node_modules/typescript-language-server/node_modules/eslint/bin/eslint.js"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-eslint-diagnostic-provider.h"

struct _GbpEslintDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
  guint is_stdin : 1;
};

G_DEFINE_FINAL_TYPE (GbpEslintDiagnosticProvider, gbp_eslint_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
gbp_eslint_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                    IdeRunContext      *run_context,
                                                    GFile              *file,
                                                    GBytes             *contents,
                                                    const char         *language_id,
                                                    GError            **error)
{
  GbpEslintDiagnosticProvider *self = (GbpEslintDiagnosticProvider *)tool;

  g_assert (GBP_IS_ESLINT_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (IDE_DIAGNOSTIC_TOOL_CLASS (gbp_eslint_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    {
      ide_run_context_append_args (run_context,
                                   IDE_STRV_INIT ("-f", "json",
                                                  "--ignore-pattern", "!node_modules/*",
                                                  "--ignore-pattern", "!bower_components/*"));
      if (contents != NULL)
        ide_run_context_append_args (run_context, IDE_STRV_INIT ("--stdin", "--stdin-filename"));
      ide_run_context_append_argv (run_context, g_file_peek_path (file));
      return TRUE;
    }

  return FALSE;
}

static inline IdeDiagnosticSeverity
parse_severity (int n)
{
  switch (n)
    {
    case 1:
      return IDE_DIAGNOSTIC_WARNING;
    case 2:
      return IDE_DIAGNOSTIC_ERROR;
    default:
      return IDE_DIAGNOSTIC_NOTE;
    }
}

static void
gbp_eslint_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                     IdeDiagnostics    *diagnostics,
                                                     GFile             *file,
                                                     const char        *stdout_buf,
                                                     const char        *stderr_buf)
{
  GbpEslintDiagnosticProvider *self = (GbpEslintDiagnosticProvider *)tool;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  JsonNode *root;
  JsonArray *results;

  g_assert (GBP_IS_ESLINT_DIAGNOSTIC_PROVIDER (self));
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
          JsonArray *messages = json_object_get_array_member (result, "messages");
          guint n_messages = json_array_get_length (messages);

          for (guint m = 0; m < n_messages; m++)
            {
              JsonObject *message = json_array_get_object_element (messages, m);
              g_autoptr(IdeDiagnostic) diagnostic = NULL;
              g_autoptr(IdeLocation) start = NULL;
              g_autoptr(IdeLocation) end = NULL;
              IdeDiagnosticSeverity severity;
              guint start_line;
              guint start_col;

              if (!json_object_has_member (message, "line") ||
                  !json_object_has_member (message, "column"))
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

              severity = parse_severity (json_object_get_int_member (message, "severity"));

              diagnostic = ide_diagnostic_new (severity,
                                               json_object_get_string_member (message, "message"),
                                               start);
              if (end != NULL)
                ide_diagnostic_take_range (diagnostic, ide_range_new (start, end));

              /* TODO: (from python implementation)
               *
               * if 'fix' in message:
               * Fixes often come without end* information so we
               * will rarely get here, instead it has a file offset
               * which is not actually implemented in IdeSourceLocation
               * fixit = Ide.Fixit.new(range_, message['fix']['text'])
               * diagnostic.take_fixit(fixit)
               */

              ide_diagnostics_add (diagnostics, diagnostic);
            }
        }
    }
}

static void
gbp_eslint_diagnostic_provider_class_init (GbpEslintDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_eslint_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_eslint_diagnostic_provider_populate_diagnostics;
}

static void
gbp_eslint_diagnostic_provider_init (GbpEslintDiagnosticProvider *self)
{
  g_autofree char *local_program_path = g_build_filename ("node_modules", ".bin", "eslint", NULL);

  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "eslint");
  ide_diagnostic_tool_set_bundled_program_path (IDE_DIAGNOSTIC_TOOL (self), BUNDLED_ESLINT);
  ide_diagnostic_tool_set_local_program_path (IDE_DIAGNOSTIC_TOOL (self), local_program_path);
}
