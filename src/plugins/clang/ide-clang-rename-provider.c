/* ide-clang-rename-provider.c
 *
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

#define G_LOG_DOMAIN "ide-clang-rename-provider"

#include "config.h"

#include <libide-code.h>
#include <libide-foundry.h>
#include <libide-vcs.h>

#include "ide-clang-rename-provider.h"

struct _IdeClangRenameProvider
{
  IdeObject  parent_instance;
  IdeBuffer *buffer;
};

enum {
  PROP_0,
  PROP_BUFFER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_clang_rename_provider_communicate_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(IdeTextEdit) edit = NULL;
  g_autoptr(IdeLocation) begin = NULL;
  g_autoptr(IdeLocation) end = NULL;
  g_autoptr(IdeRange) range = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) edits = NULL;
  g_autofree gchar *stdout_buf = NULL;
  IdeBuffer *buffer;
  GtkTextIter begin_iter, end_iter;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  if (ide_task_return_error_if_cancelled (task))
    IDE_EXIT;

  if (ide_str_empty0 (stdout_buf) || (stdout_buf[0] == '\n' && stdout_buf[1] == 0))
    {
      /* Don't allow deleting the buffer contents */
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_FAILED,
                                 "Failed to get replacement buffer for file");
      IDE_EXIT;
    }

  buffer = ide_task_get_task_data (task);

  g_assert (buffer != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * If the buffer has trailing newline set, then just remove the added \n we
   * will get at the end of the buffer.
   */
  if (gtk_source_buffer_get_implicit_trailing_newline (GTK_SOURCE_BUFFER (buffer)))
    {
      gsize len = strlen (stdout_buf);

      if (len > 0 && stdout_buf[len-1] == '\n')
        stdout_buf[len-1] = 0;
    }

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin_iter, &end_iter);
  begin = ide_buffer_get_iter_location (buffer, &begin_iter);
  end = ide_buffer_get_iter_location (buffer, &end_iter);
  range = ide_range_new (begin, end);

  /*
   * We just get the single replacement buffer from clang-rename instead
   * of individual file-edits, so create IdeTextEdit to reflect that.
   */

  edit = ide_text_edit_new (range, stdout_buf);
  edits = g_ptr_array_new_full (1, g_object_unref);
  g_ptr_array_add (edits, g_steal_pointer (&edit));

  ide_task_return_pointer (task, g_steal_pointer (&edits), g_ptr_array_unref);

  IDE_EXIT;
}

static void
ide_clang_rename_provider_rename_async (IdeRenameProvider   *provider,
                                        IdeLocation         *location,
                                        const gchar         *new_name,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeClangRenameProvider *self = (IdeClangRenameProvider *)provider;
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *new_name_arg = NULL;
  g_autofree gchar *position_arg = NULL;
  g_autofree gchar *path = NULL;
  IdePipeline *pipeline;
  IdeBuildManager *build_manager;
  const gchar *builddir = NULL;
  IdeContext *context;
  GFile *file;
  guint offset;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_RENAME_PROVIDER (self));
  g_assert (IDE_IS_BUFFER (self->buffer));
  g_assert (location != NULL);
  g_assert (!ide_str_empty0 (new_name));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: For build systems that don't support compile_commands.json,
   *       we could synthesize one for the file. But that probably belongs
   *       at a layer a bit higher up.
   */

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_rename_provider_rename_async);
  ide_task_set_task_data (task, g_object_ref (self->buffer), g_object_unref);

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_build_manager_from_context (context);
  if ((pipeline = ide_build_manager_get_pipeline (build_manager)))
    builddir = ide_pipeline_get_builddir (pipeline);

  file = ide_location_get_file (location);
  path = g_file_get_path (file);

  if (path == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Only local files are supported");
      IDE_EXIT;
    }

  offset = ide_location_get_offset (location);

  position_arg = g_strdup_printf ("-offset=%u", offset);
  new_name_arg = g_strdup_printf ("-new-name=%s", new_name);

  launcher = ide_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  if (builddir != NULL)
    ide_subprocess_launcher_set_cwd (launcher, builddir);
  else
    {
      IdeVcs *vcs = ide_vcs_from_context (context);
      GFile *workdir = ide_vcs_get_workdir (vcs);
      g_autofree gchar *srcdir = g_file_get_path (workdir);

      /* fallback to srcdir */
      ide_subprocess_launcher_set_cwd (launcher, srcdir);
    }

  ide_subprocess_launcher_push_argv (launcher, "clang-rename");
  ide_subprocess_launcher_push_argv (launcher, path);
  ide_subprocess_launcher_push_argv (launcher, position_arg);
  ide_subprocess_launcher_push_argv (launcher, new_name_arg);

  if (builddir != NULL)
    {
      g_autofree gchar *compile_commands = NULL;
      g_autofree gchar *p_arg = NULL;

      compile_commands = g_build_filename (builddir,
                                           "compile_commands.json",
                                           NULL);
      p_arg = g_strdup_printf ("-p=%s", compile_commands);

      if (g_file_test (compile_commands, G_FILE_TEST_EXISTS))
        ide_subprocess_launcher_push_argv (launcher, p_arg);
    }

  subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_subprocess_communicate_utf8_async (subprocess,
                                         NULL,
                                         cancellable,
                                         ide_clang_rename_provider_communicate_cb,
                                         g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_clang_rename_provider_rename_finish (IdeRenameProvider  *provider,
                                         GAsyncResult       *result,
                                         GPtrArray         **edits,
                                         GError            **error)
{
  g_autoptr(GPtrArray) ar = NULL;
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_CLANG_RENAME_PROVIDER (provider));
  g_assert (IDE_IS_TASK (result));

  ar = ide_task_propagate_pointer (IDE_TASK (result), error);
  ret = (ar != NULL);

  if (edits != NULL)
    *edits = IDE_PTR_ARRAY_STEAL_FULL (&ar);

  IDE_RETURN (ret);
}

static void
rename_provider_iface_init (IdeRenameProviderInterface *iface)
{
  iface->rename_async = ide_clang_rename_provider_rename_async;
  iface->rename_finish = ide_clang_rename_provider_rename_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangRenameProvider, ide_clang_rename_provider, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_RENAME_PROVIDER, rename_provider_iface_init))

static void
ide_clang_rename_provider_destroy (IdeObject *object)
{
  IdeClangRenameProvider *self = (IdeClangRenameProvider *)object;

  g_clear_object (&self->buffer);

  IDE_OBJECT_CLASS (ide_clang_rename_provider_parent_class)->destroy (object);
}

static void
ide_clang_rename_provider_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  IdeClangRenameProvider *self = IDE_CLANG_RENAME_PROVIDER (object);

  switch (prop_id)
    {
    case PROP_BUFFER:
      if (self->buffer != g_value_get_object (value))
        {
          g_clear_object (&self->buffer);
          self->buffer = g_value_dup_object (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_clang_rename_provider_class_init (IdeClangRenameProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->set_property = ide_clang_rename_provider_set_property;

  i_object_class->destroy = ide_clang_rename_provider_destroy;

  properties [PROP_BUFFER] =
    g_param_spec_object ("buffer",
                         "Buffer",
                         "The buffer for renames",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_clang_rename_provider_init (IdeClangRenameProvider *self)
{
}
