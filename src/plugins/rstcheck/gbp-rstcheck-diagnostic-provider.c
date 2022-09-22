/* gbp-rstcheck-diagnostic-provider.c
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

#define G_LOG_DOMAIN "gbp-rstcheck-diagnostic-provider"

#include "config.h"

#include "gbp-rstcheck-diagnostic-provider.h"

struct _GbpRstcheckDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (GbpRstcheckDiagnosticProvider, gbp_rstcheck_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static GRegex *rstcheck_regex;
static GHashTable *severities;

static gboolean
gbp_rstcheck_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                      IdeRunContext      *run_context,
                                                      GFile              *file,
                                                      GBytes             *contents,
                                                      const char         *language_id,
                                                      GError            **error)
{
  g_assert (GBP_IS_RSTCHECK_DIAGNOSTIC_PROVIDER (tool));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (G_IS_FILE (file));

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (gbp_rstcheck_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    return FALSE;

  ide_run_context_append_argv (run_context, "-");

  return TRUE;
}

static void
gbp_rstcheck_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                       IdeDiagnostics    *diagnostics,
                                                       GFile             *file,
                                                       const char        *stdout_data,
                                                       const char        *stderr_data)
{
  IdeLineReader reader;
  char *line;
  gsize len;

  g_assert (GBP_IS_RSTCHECK_DIAGNOSTIC_PROVIDER (tool));
  g_assert (IDE_IS_DIAGNOSTICS (diagnostics));
  g_assert (G_IS_FILE (file));
  g_assert (rstcheck_regex != NULL);

  if (ide_str_empty0 (stderr_data))
    return;

  /* Safe to stomp over "const" here since it's just for our consumption */
  ide_line_reader_init (&reader, (char *)stderr_data, -1);
  while ((line = ide_line_reader_next (&reader, &len)))
    {
      g_autoptr(IdeLocation) location = NULL;
      g_auto(GStrv) tokens = NULL;
      IdeDiagnosticSeverity severity;

      if (line[0] == 0)
        continue;

      tokens = g_regex_split (rstcheck_regex, line, 0);

      g_assert (tokens != NULL);
      g_assert (tokens[0] != NULL);
      g_assert (g_strv_length (tokens) >= 5);

      if (tokens[1] == NULL)
        continue;

      location = ide_location_new (file, atoi (tokens[1]) - 1, 0);
      severity = GPOINTER_TO_INT (g_hash_table_lookup (severities, tokens[2]));
      ide_diagnostics_take (diagnostics, ide_diagnostic_new (severity, g_strstrip (tokens[4]), location));
    }
}

static void
gbp_rstcheck_diagnostic_provider_class_init (GbpRstcheckDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *diagnostic_tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  diagnostic_tool_class->prepare_run_context = gbp_rstcheck_diagnostic_provider_prepare_run_context;
  diagnostic_tool_class->populate_diagnostics = gbp_rstcheck_diagnostic_provider_populate_diagnostics;

  rstcheck_regex = g_regex_new ("\\:([0-9]+)\\:\\s\\(([A-Z]+)\\/([0-9]{1})\\)\\s", G_REGEX_OPTIMIZE, 0, NULL);

  severities = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (severities, (char *)"INFO", GINT_TO_POINTER (IDE_DIAGNOSTIC_NOTE));
  g_hash_table_insert (severities, (char *)"WARNING", GINT_TO_POINTER (IDE_DIAGNOSTIC_WARNING));
  g_hash_table_insert (severities, (char *)"ERROR", GINT_TO_POINTER (IDE_DIAGNOSTIC_ERROR));
  g_hash_table_insert (severities, (char *)"SEVERE", GINT_TO_POINTER (IDE_DIAGNOSTIC_FATAL));
  g_hash_table_insert (severities, (char *)"NONE", GINT_TO_POINTER (IDE_DIAGNOSTIC_NOTE));
}

static void
gbp_rstcheck_diagnostic_provider_init (GbpRstcheckDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "rstcheck");
}
