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

#include <dazzle.h>
#include <errno.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-gettext-diagnostic-provider.h"

struct _IdeGettextDiagnosticProvider
{
  IdeObject parent_instance;
};

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
          if (dzl_str_equal0 (id, id_to_lang[i].id))
            return id_to_lang[i].lang;
        }
    }

  return NULL;
}


static void
ide_gettext_diagnostic_provider_communicate_cb (GObject      *object,
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

  ide_line_reader_init (&reader, stderr_buf, -1);

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
      ide_diagnostics_add (ret, diag);
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_object_unref);
}

static void
ide_gettext_diagnostic_provider_diagnose_async (IdeDiagnosticProvider *provider,
                                                GFile                 *file,
                                                GBytes                *contents,
                                                const gchar           *lang_id,
                                                GCancellable          *cancellable,
                                                GAsyncReadyCallback    callback,
                                                gpointer               user_data)
{
  IdeGettextDiagnosticProvider *self = (IdeGettextDiagnosticProvider *)provider;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *xgettext_id;

  g_assert (IDE_IS_GETTEXT_DIAGNOSTIC_PROVIDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (contents != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gettext_diagnostic_provider_diagnose_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

  /* Figure out what language xgettext should use */
  if (!(xgettext_id = id_to_xgettext_language (lang_id)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Language %s is not supported",
                                 lang_id ?: "plain-text");
      return;
    }

  /* Return an empty set if we failed to locate any buffer contents */
  if (g_bytes_get_size (contents) == 0)
    {
      ide_task_return_pointer (task,
                               ide_diagnostics_new (),
                               g_object_unref);
      return;
    }

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);

  ide_subprocess_launcher_push_argv (launcher, "xgettext");
  ide_subprocess_launcher_push_argv (launcher, "--check=ellipsis-unicode");
  ide_subprocess_launcher_push_argv (launcher, "--check=quote-unicode");
  ide_subprocess_launcher_push_argv (launcher, "--check=space-ellipsis");
  ide_subprocess_launcher_push_argv (launcher, "--from-code=UTF-8");
  ide_subprocess_launcher_push_argv (launcher, "-k_");
  ide_subprocess_launcher_push_argv (launcher, "-kN_");
  ide_subprocess_launcher_push_argv (launcher, "-L");
  ide_subprocess_launcher_push_argv (launcher, xgettext_id);
  ide_subprocess_launcher_push_argv (launcher, "-o");
  ide_subprocess_launcher_push_argv (launcher, "-");
  ide_subprocess_launcher_push_argv (launcher, "-");

  /* Spawn the process of fail immediately */
  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* Write the buffer contents to the subprocess and wait for the result
   * from xgettext. We'll parse the result after the process exits.
   */
  ide_subprocess_communicate_utf8_async (subprocess,
                                         (const gchar *)g_bytes_get_data (contents, NULL),
                                         cancellable,
                                         ide_gettext_diagnostic_provider_communicate_cb,
                                         g_steal_pointer (&task));
}

static IdeDiagnostics *
ide_gettext_diagnostic_provider_diagnose_finish (IdeDiagnosticProvider  *provider,
                                                 GAsyncResult           *result,
                                                 GError                **error)
{
  g_assert (IDE_IS_GETTEXT_DIAGNOSTIC_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), provider));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
diagnostic_provider_iface_init (IdeDiagnosticProviderInterface *iface)
{
  iface->diagnose_async = ide_gettext_diagnostic_provider_diagnose_async;
  iface->diagnose_finish = ide_gettext_diagnostic_provider_diagnose_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeGettextDiagnosticProvider,
                         ide_gettext_diagnostic_provider,
                         IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DIAGNOSTIC_PROVIDER,
                                                diagnostic_provider_iface_init))

static void
ide_gettext_diagnostic_provider_class_init (IdeGettextDiagnosticProviderClass *klass)
{
}

static void
ide_gettext_diagnostic_provider_init (IdeGettextDiagnosticProvider *self)
{
}
