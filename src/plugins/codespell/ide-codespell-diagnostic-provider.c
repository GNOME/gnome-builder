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
  GSettings *settings;
};

G_DEFINE_FINAL_TYPE (IdeCodespellDiagnosticProvider, ide_codespell_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static gboolean
ide_codespell_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                       IdeRunContext      *run_context,
                                                       GFile              *file,
                                                       GBytes             *contents,
                                                       const char         *language_id,
                                                       GError            **error)
{
  IdeCodespellDiagnosticProvider *self = (IdeCodespellDiagnosticProvider *)tool;

  IDE_ENTRY;

  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (self));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file != NULL || contents != NULL);

  if (!g_settings_get_boolean (self->settings, "check-spelling"))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Spellcheck disabled");
      IDE_RETURN (FALSE);
    }

  if (IDE_DIAGNOSTIC_TOOL_CLASS (ide_codespell_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    {
      ide_run_context_append_argv (run_context, "-");
      IDE_RETURN (TRUE);
    }

  IDE_RETURN (FALSE);
}

static void
ide_codespell_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                        IdeDiagnostics    *diagnostics,
                                                        GFile             *file,
                                                        const char        *stdout_buf,
                                                        const char        *stderr_buf)
{
  static GRegex *regex;
  GMatchInfo *issues = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (tool));
  g_assert (IDE_IS_DIAGNOSTICS (diagnostics));
  g_assert (!file || G_IS_FILE (file));

  if G_UNLIKELY (regex == NULL)
    {
      g_autoptr(GError) error = NULL;
      regex = g_regex_new ("(([0-9]+): .+?\n\t([a-zA-Z]+) ==> ([a-zA-Z0-9]+))",
                           G_REGEX_RAW,
                           G_REGEX_MATCH_NEWLINE_ANY,
                           &error);
      g_assert_no_error (error);
    }

  if (ide_str_empty0 (stdout_buf))
    IDE_EXIT;

  g_regex_match (regex, stdout_buf, 0, &issues);

  g_assert (issues != NULL);

  while (g_match_info_matches (issues))
    {
      g_autoptr(IdeDiagnostic) diag = NULL;
      g_autoptr(IdeLocation) loc = NULL;
      g_autoptr(IdeLocation) loc_end = NULL;
      g_autofree char *line_word = g_match_info_fetch (issues, 2);
      g_autofree char *typo_word = g_match_info_fetch (issues, 3);
      g_autofree char *expected_word = g_match_info_fetch (issues, 4);
      g_autofree char *diagnostic_text = NULL;
      guint64 lineno = g_ascii_strtoull (line_word, NULL, 10);

      if (lineno != 0 &&
          line_word != NULL &&
          typo_word != NULL &&
          expected_word != NULL)
        {
          lineno--;

          diagnostic_text = g_strdup_printf (_("Possible typo in “%s”. Did you mean “%s”?"),
                                             typo_word, expected_word);

          loc = ide_location_new (file, lineno, -1);
          loc_end = ide_location_new (file, lineno, G_MAXINT);

          diag = ide_diagnostic_new (IDE_DIAGNOSTIC_NOTE, diagnostic_text, loc);
          ide_diagnostic_take_range (diag, ide_range_new (loc, loc_end));
          ide_diagnostics_take (diagnostics, g_steal_pointer (&diag));
        }

      if (!g_match_info_next (issues, NULL))
        break;
    }

  g_match_info_free (issues);

  IDE_EXIT;
}

static void
ide_codespell_diagnostic_provider_finalize (GObject *object)
{
  IdeCodespellDiagnosticProvider *self = (IdeCodespellDiagnosticProvider *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_codespell_diagnostic_provider_parent_class)->finalize (object);
}

static void
ide_codespell_diagnostic_provider_class_init (IdeCodespellDiagnosticProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDiagnosticToolClass *tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  object_class->finalize = ide_codespell_diagnostic_provider_finalize;

  tool_class->prepare_run_context = ide_codespell_diagnostic_provider_prepare_run_context;
  tool_class->populate_diagnostics = ide_codespell_diagnostic_provider_populate_diagnostics;
}

static void
ide_codespell_diagnostic_provider_init (IdeCodespellDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "codespell");

  self->settings = g_settings_new ("org.gnome.builder.spelling");
}
