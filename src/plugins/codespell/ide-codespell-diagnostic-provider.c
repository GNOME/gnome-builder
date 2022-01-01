/* ide-codespell-diagnostic-provider.c
 *
 * Copyright 2020 Günther Wagner <info@gunibert.de>
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

#include "ide-codespell-diagnostic-provider.h"

struct _IdeCodespellDiagnosticProvider
{
  IdeObject parent_instance;
  char *codespell_path;
};

static void diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCodespellDiagnosticProvider,
                         ide_codespell_diagnostic_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                diagnostic_provider_iface_init))

IdeCodespellDiagnosticProvider *
ide_codespell_diagnostic_provider_new (void)
{
  return g_object_new (IDE_TYPE_CODESPELL_DIAGNOSTIC_PROVIDER, NULL);
}

static void
ide_codespell_diagnostic_provider_class_init (IdeCodespellDiagnosticProviderClass *klass)
{
}

static void
ide_codespell_diagnostic_provider_init (IdeCodespellDiagnosticProvider *self)
{
}

static void
ide_codespell_diagnostic_provider_communicate_cb (GObject      *object,
                                                  GAsyncResult *result,
                                                  gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeDiagnostics) ret = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *stderr_buf = NULL;
  g_autofree gchar *stdout_buf = NULL;
  IdeLineReader reader;
  GFile *file;
  gchar *line;
  gsize len;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  file = ide_task_get_task_data (task);
  g_assert (file != NULL);
  g_assert (G_IS_FILE (file));

  ret = ide_diagnostics_new ();

  ide_line_reader_init (&reader, stdout_buf, -1);

  while (NULL != (line = ide_line_reader_next (&reader, &len)))
    {
      g_autoptr(IdeDiagnostic) diag = NULL;
      g_autoptr(IdeLocation) loc = NULL;
      g_autoptr(IdeLocation) loc_end = NULL;
      guint64 lineno;

      line[len] = '\0';

      /* Lines that we want to parse should look something like this:
       * filename:42: misspelled word ==> correct word
       */
      if (!g_str_has_prefix (line, g_file_get_path (file)))
        continue;

      line += strlen (g_file_get_path (file)) + 1;
      if (!g_ascii_isdigit (*line))
        continue;

      lineno = g_ascii_strtoull (line, &line, 10);
      if (lineno == G_MAXUINT64 || lineno == 0)
        continue;
      if (lineno > 0)
        lineno--;

      if (!g_str_has_prefix (line, ": "))
        continue;

      line += strlen (": ");

      /* As we don't get a column information out of codespell mark the full line */
      loc = ide_location_new (file, lineno, -1);
      loc_end = ide_location_new (file, lineno, G_MAXINT);
      diag = ide_diagnostic_new (IDE_DIAGNOSTIC_NOTE, line, loc);
      ide_diagnostic_add_range (diag, ide_range_new (loc, loc_end));
      ide_diagnostics_add (ret, diag);
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_object_unref);

}

static void
ide_codespell_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                                  GFile                 *file,
                                                  GBytes                *contents,
                                                  const gchar           *lang_id,
                                                  GCancellable          *cancellable,
                                                  GAsyncReadyCallback    callback,
                                                  gpointer               user_data)
{
  IdeCodespellDiagnosticProvider *self = (IdeCodespellDiagnosticProvider *)provider;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (contents != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_codespell_diagnostic_provider_diagnose_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  if (self->codespell_path == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Not supported");
      return;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);

  ide_subprocess_launcher_push_argv (launcher, "codespell");
  /* ide_subprocess_launcher_push_argv (launcher, "-d"); */
  ide_subprocess_launcher_push_argv (launcher, g_file_get_path (file));

  /* Spawn the process of fail immediately */
  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         ide_codespell_diagnostic_provider_communicate_cb,
                                         g_steal_pointer (&task));
}

static IdeDiagnostics *
ide_codespell_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                                   GAsyncResult           *result,
                                                   GError                **error)
{
  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_codespell_diagnostic_provider_load (IdeDiagnosticProvider *provider)
{
  IdeCodespellDiagnosticProvider *self = (IdeCodespellDiagnosticProvider *)provider;

  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (self));

  self->codespell_path = g_find_program_in_path ("codespell");
}

static void
ide_codespell_diagnostic_provider_unload (IdeDiagnosticProvider *provider)
{
  IdeCodespellDiagnosticProvider *self = (IdeCodespellDiagnosticProvider *)provider;

  g_assert (IDE_IS_CODESPELL_DIAGNOSTIC_PROVIDER (self));

  g_clear_pointer (&self->codespell_path, g_free);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_codespell_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_codespell_diagnostic_provider_diagnose_finish;
  iface->load = ide_codespell_diagnostic_provider_load;
  iface->unload = ide_codespell_diagnostic_provider_unload;
}
