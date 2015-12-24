/* gbp-gcc-build-result-addin.c
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

#include <string.h>

#include "egg-signal-group.h"

#include "gbp-gcc-build-result-addin.h"


#define ERROR_FORMAT_REGEX           \
  "(?<filename>[a-zA-Z0-9\\-\\.]+):" \
  "(?<line>\\d+):"                   \
  "(?<column>\\d+): "                \
  "(?<level>[\\w\\s]+): "            \
  "(?<message>.*)"

struct _GbpGccBuildResultAddin
{
  IdeObject       parent_instance;

  EggSignalGroup *signals;
  gchar          *current_dir;
};

static void build_result_addin_iface_init (IdeBuildResultAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbpGccBuildResultAddin, gbp_gcc_build_result_addin, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_RESULT_ADDIN,
                                               build_result_addin_iface_init))

static GRegex *errfmt;

static IdeDiagnosticSeverity
parse_severity (const gchar *str)
{
  g_autofree gchar *lower = NULL;

  if (str == NULL)
    return IDE_DIAGNOSTIC_WARNING;

  lower = g_utf8_strdown (str, -1);

  if (strstr (lower, "fatal") != NULL)
    return IDE_DIAGNOSTIC_FATAL;

  if (strstr (lower, "error") != NULL)
    return IDE_DIAGNOSTIC_ERROR;

  if (strstr (lower, "warning") != NULL)
    return IDE_DIAGNOSTIC_WARNING;

  if (strstr (lower, "ignored") != NULL)
    return IDE_DIAGNOSTIC_IGNORED;

  if (strstr (lower, "deprecated") != NULL)
    return IDE_DIAGNOSTIC_DEPRECATED;

  if (strstr (lower, "note") != NULL)
    return IDE_DIAGNOSTIC_NOTE;

  return IDE_DIAGNOSTIC_WARNING;
}

static IdeDiagnostic *
create_diagnostic (GbpGccBuildResultAddin *self,
                   GMatchInfo             *match_info)
{
  g_autofree gchar *filename = NULL;
  g_autofree gchar *line = NULL;
  g_autofree gchar *column = NULL;
  g_autofree gchar *message = NULL;
  g_autofree gchar *level = NULL;
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;
  IdeDiagnostic *diagnostic;
  IdeContext *context;
  struct {
    gint64 line;
    gint64 column;
    IdeDiagnosticSeverity severity;
  } parsed;

  g_assert (GBP_IS_GCC_BUILD_RESULT_ADDIN (self));
  g_assert (match_info != NULL);

  filename = g_match_info_fetch_named (match_info, "filename");
  line = g_match_info_fetch_named (match_info, "line");
  column = g_match_info_fetch_named (match_info, "column");
  message = g_match_info_fetch_named (match_info, "message");
  level = g_match_info_fetch_named (match_info, "level");

  parsed.line = g_ascii_strtoll (line, NULL, 10);
  if (parsed.line < 1 || parsed.line > G_MAXINT32)
    return NULL;
  parsed.line--;

  parsed.column = g_ascii_strtoll (column, NULL, 10);
  if (parsed.column < 1 || parsed.column > G_MAXINT32)
    return NULL;
  parsed.column--;

  parsed.severity = parse_severity (level);

  context = ide_object_get_context (IDE_OBJECT (self));

  if (self->current_dir)
    {
      gchar *path;

      path = g_build_filename (self->current_dir, filename, NULL);
      g_free (filename);
      filename = path;
    }

  file = ide_file_new_for_path (context, filename);
  location = ide_source_location_new (file, parsed.line, parsed.column, 0);
  diagnostic = ide_diagnostic_new (parsed.severity, message, location);

  return diagnostic;
}

static void
gbp_gcc_build_result_addin_log (GbpGccBuildResultAddin *self,
                                IdeBuildResultLog       log,
                                const gchar            *message,
                                IdeBuildResult         *result)
{
  GMatchInfo *match_info = NULL;
  const gchar *enterdir;

  g_assert (GBP_IS_GCC_BUILD_RESULT_ADDIN (self));
  g_assert (IDE_IS_BUILD_RESULT (result));

  /*
   * This expects LANG=C, which is defined in the autotools Builder.
   * Not the most ideal decoupling of logic, but we don't have a whole
   * lot to work with here.
   */
  if ((enterdir = strstr (message, "Entering directory '")))
    {
      gsize len;

      enterdir += IDE_LITERAL_LENGTH ("Entering directory '");
      len = strlen (enterdir);

      if (len > 0)
        {
          g_free (self->current_dir);
          self->current_dir = g_strndup (enterdir, len - 1);
        }
    }

  if (g_regex_match (errfmt, message, 0, &match_info))
    {
      IdeDiagnostic *diagnostic;

      if ((diagnostic = create_diagnostic (self, match_info)))
        {
          ide_build_result_emit_diagnostic (result, diagnostic);
          ide_diagnostic_unref (diagnostic);
        }
    }

  g_match_info_free (match_info);
}

static void
gbp_gcc_build_result_addin_class_init (GbpGccBuildResultAddinClass *klass)
{
  errfmt = g_regex_new (ERROR_FORMAT_REGEX, G_REGEX_OPTIMIZE | G_REGEX_CASELESS, 0, NULL);
  g_assert (errfmt != NULL);
}

static void
gbp_gcc_build_result_addin_init (GbpGccBuildResultAddin *self)
{
  self->signals = egg_signal_group_new (IDE_TYPE_BUILD_RESULT);

  egg_signal_group_connect_object (self->signals,
                                   "log",
                                   G_CALLBACK (gbp_gcc_build_result_addin_log),
                                   self,
                                   G_CONNECT_SWAPPED);
}

static void
gbp_gcc_build_result_addin_load (IdeBuildResultAddin *addin,
                                 IdeBuildResult      *result)
{
  GbpGccBuildResultAddin *self = (GbpGccBuildResultAddin *)addin;

  egg_signal_group_set_target (self->signals, result);
}

static void
gbp_gcc_build_result_addin_unload (IdeBuildResultAddin *addin,
                                   IdeBuildResult      *result)
{
  GbpGccBuildResultAddin *self = (GbpGccBuildResultAddin *)addin;

  egg_signal_group_set_target (self->signals, NULL);
}

static void
build_result_addin_iface_init (IdeBuildResultAddinInterface *iface)
{
  iface->load = gbp_gcc_build_result_addin_load;
  iface->unload = gbp_gcc_build_result_addin_unload;
}
