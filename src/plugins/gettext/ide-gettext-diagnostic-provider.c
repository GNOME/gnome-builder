/* ide-gettext-diagnostic-provider.c
 *
 * Copyright 2016 Daiki Ueno <dueno@src.gnome.org>
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-gettext-diagnostic-provider"

#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-gettext-diagnostic-provider.h"

struct _IdeGettextDiagnosticProvider
{
  IdeDiagnosticTool parent_instance;
};

G_DEFINE_FINAL_TYPE (IdeGettextDiagnosticProvider, ide_gettext_diagnostic_provider, IDE_TYPE_DIAGNOSTIC_TOOL)

static const gchar *
id_to_xgettext_language (const gchar *id)
{
  static const struct {
    const gchar *id;
    const gchar *lang;
  } id_to_lang[] = {
    { "awk", "awk" },
    { "c", "C" },
    { "chdr", "C" },
    { "cpp", "C++" },
    { "js", "JavaScript" },
    { "lisp", "Lisp" },
    { "objc", "ObjectiveC" },
    { "perl", "Perl" },
    { "php", "PHP" },
    { "python", "Python" },
    { "sh", "Shell" },
    { "tcl", "Tcl" },
    { "vala", "Vala" }
  };

  if (id != NULL)
    {
      for (guint i = 0; i < G_N_ELEMENTS (id_to_lang); i++)
        {
          if (ide_str_equal0 (id, id_to_lang[i].id))
            return id_to_lang[i].lang;
        }
    }

  return NULL;
}


static void
ide_gettext_diagnostic_provider_populate_diagnostics (IdeDiagnosticTool *tool,
                                                      IdeDiagnostics    *diagnostics,
                                                      GFile             *file,
                                                      const char        *stdout_buf,
                                                      const char        *stderr_buf)
{
  IdeLineReader reader;
  gchar *line;
  gsize len;

  IDE_ENTRY;

  g_assert (IDE_IS_GETTEXT_DIAGNOSTIC_PROVIDER (tool));
  g_assert (IDE_IS_DIAGNOSTICS (diagnostics));
  g_assert (!file || G_IS_FILE (file));

  ide_line_reader_init (&reader, (char *)stderr_buf, -1);

  while (NULL != (line = ide_line_reader_next (&reader, &len)))
    {
      g_autoptr(IdeDiagnostic) diag = NULL;
      g_autoptr(IdeLocation) loc = NULL;
      guint64 lineno;

      line[len] = '\0';

      /* Lines that we want to parse should look something like this:
       * "standard input:195: ASCII double quote used instead of Unicode"
       */

      if (!g_str_has_prefix (line, "standard input:"))
        continue;

      line += strlen ("standard input:");
      if (!g_ascii_isdigit (*line))
        continue;

      lineno = g_ascii_strtoull (line, &line, 10);
      if ((lineno == G_MAXUINT64 && errno == ERANGE) || ((lineno == 0) && errno == EINVAL))
        continue;
      if (lineno > 0)
        lineno--;

      if (!g_str_has_prefix (line, ": "))
        continue;

      line += strlen (": ");

      loc = ide_location_new (file, lineno, -1);
      diag = ide_diagnostic_new (IDE_DIAGNOSTIC_WARNING, line, loc);
      ide_diagnostics_add (diagnostics, diag);
    }

  IDE_EXIT;
}

static gboolean
ide_gettext_diagnostic_provider_can_diagnose (IdeDiagnosticTool *tool,
                                              GFile             *file,
                                              GBytes            *contents,
                                              const char        *language_id)
{
  const char *xgettext_id;

  IDE_ENTRY;

  g_assert (IDE_IS_GETTEXT_DIAGNOSTIC_PROVIDER (tool));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file != NULL || contents != NULL);

  if (!(xgettext_id = id_to_xgettext_language (language_id)))
    IDE_RETURN (FALSE);

  IDE_RETURN (TRUE);
}

static gboolean
ide_gettext_diagnostic_provider_prepare_run_context (IdeDiagnosticTool  *tool,
                                                     IdeRunContext      *run_context,
                                                     GFile              *file,
                                                     GBytes             *contents,
                                                     const char         *language_id,
                                                     GError            **error)
{
  const char *xgettext_id;

  IDE_ENTRY;

  g_assert (IDE_IS_GETTEXT_DIAGNOSTIC_PROVIDER (tool));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (!file || G_IS_FILE (file));
  g_assert (file != NULL || contents != NULL);

  if (!(xgettext_id = id_to_xgettext_language (language_id)))
    g_assert_not_reached ();

  if (!IDE_DIAGNOSTIC_TOOL_CLASS (ide_gettext_diagnostic_provider_parent_class)->prepare_run_context (tool, run_context, file, contents, language_id, error))
    IDE_RETURN (FALSE);

  ide_run_context_append_argv (run_context, "--check=ellipsis-unicode");
  ide_run_context_append_argv (run_context, "--check=quote-unicode");
  ide_run_context_append_argv (run_context, "--check=space-ellipsis");
  ide_run_context_append_argv (run_context, "--from-code=UTF-8");
  ide_run_context_append_argv (run_context, "-k_");
  ide_run_context_append_argv (run_context, "-kN_");
  ide_run_context_append_argv (run_context, "-L");
  ide_run_context_append_argv (run_context, xgettext_id);
  ide_run_context_append_argv (run_context, "-o");
  ide_run_context_append_argv (run_context, "-");
  ide_run_context_append_argv (run_context, "-");

  IDE_RETURN (TRUE);
}

static void
ide_gettext_diagnostic_provider_class_init (IdeGettextDiagnosticProviderClass *klass)
{
  IdeDiagnosticToolClass *tool_class = IDE_DIAGNOSTIC_TOOL_CLASS (klass);

  tool_class->can_diagnose = ide_gettext_diagnostic_provider_can_diagnose;
  tool_class->prepare_run_context = ide_gettext_diagnostic_provider_prepare_run_context;
  tool_class->populate_diagnostics = ide_gettext_diagnostic_provider_populate_diagnostics;
}

static void
ide_gettext_diagnostic_provider_init (IdeGettextDiagnosticProvider *self)
{
  ide_diagnostic_tool_set_program_name (IDE_DIAGNOSTIC_TOOL (self), "xgettext");
  ide_diagnostic_tool_set_subprocess_flags (IDE_DIAGNOSTIC_TOOL (self),
                                            (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                             G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                             G_SUBPROCESS_FLAGS_STDERR_PIPE));
}
