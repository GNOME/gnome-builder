/*
 * gbp-swiftformat-formatter.c
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

#include "config.h"

#include <libide-foundry.h>

#include "gbp-swiftformat-formatter.h"

struct _GbpSwiftformatFormatter
{
  IdeObject parent_instance;
};

static IdeRunContext *
create_run_context (GbpSwiftformatFormatter *self,
                    const char              *argv0)
{
  g_autoptr(IdeRunContext) run_context = NULL;
  IdeContext *context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SWIFTFORMAT_FORMATTER (self));
  g_assert (argv0 != NULL);

  run_context = ide_run_context_new ();

  if (!(context = ide_object_get_context (IDE_OBJECT (self))))
    goto run_on_host;

  if (ide_context_has_project (context))
    {
      IdeBuildManager *build_manager = ide_build_manager_from_context (context);
      IdePipeline *pipeline = ide_build_manager_get_pipeline (build_manager);

      if (pipeline != NULL &&
          ide_pipeline_contains_program_in_path (pipeline, argv0, NULL))
        {
          ide_pipeline_prepare_run_context (pipeline, run_context);
          ide_run_context_append_argv (run_context, argv0);

          return g_steal_pointer (&run_context);
        }
    }

run_on_host:
  ide_run_context_push_host (run_context);
  ide_run_context_append_argv (run_context, argv0);

  return g_steal_pointer (&run_context);
}

static void
gbp_swiftformat_formatter_format_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree char *stdout_buf = NULL;
  IdeBuffer *buffer = NULL;
  GtkTextIter start;
  GtkTextIter end;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  buffer = ide_task_get_task_data (task);

  g_assert (IDE_IS_BUFFER (buffer));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (!ide_subprocess_get_successful (subprocess) || ide_str_empty0 (stdout_buf))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 "Cannot format swift code which cannot be compiled");
      IDE_EXIT;
    }

  gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
  gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
  gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &start, stdout_buf, -1);
  gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
gbp_swiftformat_formatter_format_async (IdeFormatter        *formatter,
                                        IdeBuffer           *buffer,
                                        IdeFormatterOptions *options,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GbpSwiftformatFormatter *self = (GbpSwiftformatFormatter *)formatter;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeRunContext) run_context = NULL;
  g_autoptr(GBytes) contents = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) parent = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autofree char *path = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SWIFTFORMAT_FORMATTER (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (!options || IDE_IS_FORMATTER_OPTIONS (options));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (formatter, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_swiftformat_formatter_format_async);
  ide_task_set_task_data (task, g_object_ref (buffer), g_object_unref);

  run_context = create_run_context (self, "swift-format");
  ide_run_context_append_argv (run_context, "format");

  contents = ide_buffer_dup_content (buffer);
  file = ide_buffer_get_file (buffer);
  parent = g_file_get_parent (file);
  path = g_file_get_path (parent);

  /* Set the working directory so that `swift-format` can locate
   * the hierarchy of .swift-format files within the project.
   */
  ide_run_context_set_cwd (run_context, path);

  if (!(launcher = ide_run_context_end (run_context, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_launcher_set_flags (launcher,
                                     (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                      G_SUBPROCESS_FLAGS_STDOUT_PIPE));

  if (!(subprocess = ide_subprocess_launcher_spawn (launcher, NULL, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         g_bytes_get_data (contents, NULL),
                                         cancellable,
                                         gbp_swiftformat_formatter_format_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_swiftformat_formatter_format_finish (IdeFormatter  *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_FORMATTER (self));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
formatter_iface_init (IdeFormatterInterface *iface)
{
  iface->format_async = gbp_swiftformat_formatter_format_async;
  iface->format_finish = gbp_swiftformat_formatter_format_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSwiftformatFormatter, gbp_swiftformat_formatter, IDE_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_FORMATTER, formatter_iface_init))

static void
gbp_swiftformat_formatter_class_init (GbpSwiftformatFormatterClass *klass)
{
}

static void
gbp_swiftformat_formatter_init (GbpSwiftformatFormatter *self)
{

}
