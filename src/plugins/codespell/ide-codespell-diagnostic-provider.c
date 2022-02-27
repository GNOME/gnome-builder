/* ide-codespell-diagnostic-provider.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
 * Copyright 2022 Veli Tasalı <me@velitasali.com>
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

#include <glib/gi18n.h>

#include "ide-codespell-diagnostic-provider.h"

struct _IdeCodespellDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeCodespellDiagnosticProvider, ide_codespell_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static void
ide_codespell_diagnostic_provider_configure_launcher (IdeDiagnosticTool     *tool,
                                                      IdeSubprocessLauncher *launcher,
                                                      GFile                 *file,
                                                      GBytes                *contents)
{
  ide_subprocess_launcher_push_argv (launcher, "-");
}

static void
ide_codespell_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                        IdeDiagnostics    *diagnostics,
                                                        GFile             *file,
                                                        const char        *stdout_buf,
                                                        const char        *stderr_buf)
{
  g_autoptr(GRegex) regex = NULL;
  g_autoptr(GError) regex_error = NULL;
  g_autoptr(GMatchInfo) issues = NULL;

  regex = g_regex_new ("(([0-9]+): .+?\n\t([a-zA-Z]+) ==> ([a-zA-Z0-9]+))",
                       G_REGEX_RAW,
                       G_REGEX_MATCH_NEWLINE_ANY,
                       &regex_error);
  g_regex_match (regex, stdout_buf, 0, &issues);
  while (g_match_info_matches (issues))
    {
      g_autofree gchar *line_word = g_match_info_fetch (issues, 2);
      g_autofree gchar *typo_word = g_match_info_fetch (issues, 3);
      g_autofree gchar *expected_word = g_match_info_fetch (issues, 4);
      g_autofree gchar *diagnostic_text = NULL;
      g_autoptr(IdeDiagnostic) diag = NULL;
      g_autoptr(IdeLocation) loc = NULL;
      g_autoptr(IdeLocation) loc_end = NULL;
      guint64 lineno = atoi (line_word);

      g_match_info_next (issues, NULL);

      if (!lineno || !line_word || !typo_word || !expected_word)
        continue;

      lineno--;

      diagnostic_text = g_strdup_printf (_("Possible typo in '%s'. Did you mean '%s'?"),
                                         typo_word,
                                         expected_word);
      loc = ide_location_new (file, lineno, -1);
      loc_end = ide_location_new (file, lineno, G_MAXINT);
      diag = ide_diagnostic_new (IDE_DIAGNOSTIC_NOTE, diagnostic_text, loc);
      ide_diagnostic_add_range (diag, ide_range_new (loc, loc_end));
      ide_diagnostics_add (diagnostics, diag);
    }
}

static void
ide_codespell_diagnostic_provider_class_init (IdeCodespellDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  tool_class->configure_launcher = ide_codespell_diagnostic_provider_configure_launcher;
  tool_class->populate_diagnostics = ide_codespell_diagnostic_provider_populate_diagnostics;
}

static void
ide_codespell_diagnostic_provider_init (IdeCodespellDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "codespell");
}
