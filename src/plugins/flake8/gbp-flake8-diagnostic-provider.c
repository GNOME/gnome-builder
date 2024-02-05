/* gbp-flake8-diagnostic-provider.c
 *
 * Copyright 2024 Denis Ollier <dollierp@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flake8-diagnostic-provider"

#include "config.h"

#include "gbp-flake8-diagnostic-provider.h"

#define FLAKE8_DEFAULT_FORMAT "^(?<filename>[^:]+):(?<line>\\d+):(?<column>\\d+):\\s+(?<code>[^\\s]+)\\s+(?<text>.*)$"

struct _GbpFlake8DiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpFlake8DiagnosticProvider, gbp_flake8_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
gbp_flake8_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                    IdeRunContext      *run_context,
                                                    GFile              *file,
                                                    GBytes             *contents,
                                                    const char         *language_id,
                                                    GError            **error)
{
  GbpFlake8DiagnosticProvider *self = (GbpFlake8DiagnosticProvider *)tool;

  g_assert (GBP_IS_FLAKE8_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (gbp_flake8_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    return FALSE;

  ide_run_context_append_argv (run_context, "--format=default");

  if (contents != NULL)
    ide_run_context_append_argv (run_context, "-");
  else
    ide_run_context_append_argv (run_context, g_file_peek_path (file));

  return TRUE;
}

static inline IdeDiagnosticSeverity
parse_severity (const char *code)
{
  IdeDiagnosticSeverity severity;

  switch (code[0])
    {
    case 'F':
      severity = IDE_DIAGNOSTIC_FATAL;
      break;

    case 'E':
      severity = IDE_DIAGNOSTIC_ERROR;
      break;

    case 'W':
      severity = IDE_DIAGNOSTIC_WARNING;
      break;

    case 'I':
      severity = IDE_DIAGNOSTIC_NOTE;
      break;

    default:
      severity = IDE_DIAGNOSTIC_NOTE;
    }

  return severity;
}

static void
gbp_flake8_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                     IdeDiagnostics    *diagnostics,
                                                     GFile             *file,
                                                     const char        *stdout_buf,
                                                     const char        *stderr_buf)
{
  GbpFlake8DiagnosticProvider *self = (GbpFlake8DiagnosticProvider *)tool;
  g_autoptr(GError) error = NULL;
  g_autoptr(GRegex) regex = NULL;
  g_auto(GStrv) results = NULL;
  guint n_results;

  g_assert (GBP_IS_FLAKE8_DIAGNOSTIC_PROVIDER (self));
  g_assert (IDE_IS_DIAGNOSTICS (diagnostics));
  g_assert (G_IS_FILE (file));

  if (ide_str_empty0 (stdout_buf))
    return;

  results = g_strsplit (stdout_buf, "\n", 0);
  n_results = g_strv_length (results);

  if (n_results <= 0)
    return;

  if (!(regex = g_regex_new (FLAKE8_DEFAULT_FORMAT, G_REGEX_OPTIMIZE, 0, &error)))
    {
      g_warning ("%s", error->message);
      return;
    }

  for (guint r = 0; r < n_results; r++)
    {
      g_autoptr(IdeDiagnostic) diagnostic = NULL;
      g_autoptr(IdeLocation) start = NULL;
      g_autoptr(GMatchInfo) match_info = NULL;
      g_autofree char *filename = NULL;
      g_autofree char *line = NULL;
      g_autofree char *column = NULL;
      g_autofree char *code = NULL;
      g_autofree char *text = NULL;
      g_autofree char *message = NULL;
      IdeDiagnosticSeverity severity;
      guint64 lineno;
      guint64 columnno;

      g_regex_match (regex, results[r], 0, &match_info);
      if (!g_match_info_matches (match_info))
        continue;

      filename = g_match_info_fetch (match_info, 1);
      line = g_match_info_fetch (match_info, 2);
      column = g_match_info_fetch (match_info, 3);
      code = g_match_info_fetch (match_info, 4);
      text = g_match_info_fetch (match_info, 5);

      severity = parse_severity (code);
      lineno = g_ascii_strtoull (line, NULL, 10) - 1;
      columnno = g_ascii_strtoull (column, NULL, 10) - 1;
      message = g_strdup_printf ("Flake8(%s) %s", code, text);

      start = ide_location_new (file, lineno, columnno);
      diagnostic = ide_diagnostic_new (severity, message, start);

      ide_diagnostics_add (diagnostics, diagnostic);
    }
}

static void
gbp_flake8_diagnostic_provider_class_init (GbpFlake8DiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_flake8_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_flake8_diagnostic_provider_populate_diagnostics;
}

static void
gbp_flake8_diagnostic_provider_init (GbpFlake8DiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "flake8");
}
