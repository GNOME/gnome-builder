/* gb-beautifier-process.c
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libide-editor.h>
#include <string.h>

#include "gb-beautifier-private.h"
#include "gb-beautifier-helper.h"

#include "gb-beautifier-process.h"

typedef struct
{
  GbBeautifierEditorAddin   *self;
  IdeSourceView             *source_view;
  GtkTextMark               *begin_mark;
  GtkTextMark               *end_mark;
  GbBeautifierConfigCommand  command;
  GPtrArray                 *command_args_strs;
  GFile                     *src_file;
  GFile                     *config_file;
  GFile                     *tmp_workdir_file;
  GFile                     *tmp_src_file;
  GFile                     *tmp_config_file;
  gchar                     *lang_id;
  gchar                     *text;
} ProcessState;

static void
process_state_free (gpointer data)
{
  ProcessState *state = (ProcessState *)data;
  GtkTextBuffer *buffer;

  g_assert (state != NULL);
  g_assert (!state->self || GB_IS_BEAUTIFIER_EDITOR_ADDIN (state->self));
  g_assert (!state->begin_mark || GTK_IS_TEXT_MARK (state->begin_mark));
  g_assert (!state->end_mark || GTK_IS_TEXT_MARK (state->end_mark));
  g_assert (!state->src_file || G_IS_FILE (state->src_file));
  g_assert (!state->config_file || G_IS_FILE (state->config_file));
  g_assert (!state->tmp_workdir_file || G_IS_FILE (state->tmp_workdir_file));
  g_assert (!state->tmp_src_file || G_IS_FILE (state->tmp_src_file));
  g_assert (!state->tmp_config_file || G_IS_FILE (state->tmp_config_file));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->source_view));
  gtk_text_buffer_delete_mark (buffer, g_steal_pointer (&state->begin_mark));
  gtk_text_buffer_delete_mark (buffer, g_steal_pointer (&state->end_mark));

  if (state->src_file != NULL)
    gb_beautifier_helper_remove_temp_for_file (state->self, state->src_file);

  if (state->tmp_config_file != NULL)
    gb_beautifier_helper_remove_temp_for_file (state->self, state->tmp_config_file);

  if (state->tmp_src_file != NULL)
    gb_beautifier_helper_remove_temp_for_file (state->self, state->tmp_src_file);

  if (state->tmp_workdir_file != NULL)
    gb_beautifier_helper_remove_temp_for_file (state->self, state->tmp_workdir_file);

  g_clear_object (&state->config_file);
  g_clear_object (&state->src_file);
  g_clear_object (&state->tmp_config_file);
  g_clear_object (&state->tmp_src_file);
  g_clear_object (&state->tmp_workdir_file);

  g_clear_pointer (&state->lang_id, g_free);
  g_clear_pointer (&state->text, g_free);

  g_clear_pointer (&state->command_args_strs, g_ptr_array_unref);

  g_slice_free (ProcessState, state);
}

/* Substitute each @c@ annd @s@ pattern by the corresponding file */
static void
command_args_expand (GbBeautifierEditorAddin *self,
                     GPtrArray               *args,
                     ProcessState            *state)
{
  g_autofree gchar *src_path = NULL;
  g_autofree gchar *config_path = NULL;
  gchar **arg_adr;
  gchar *new_arg;
  gboolean has_config = TRUE;

  src_path = g_file_get_path (state->src_file);
  if (G_IS_FILE (state->config_file))
    config_path = g_file_get_path (state->config_file);
  else
    has_config = FALSE;

  for (guint i = 0; g_ptr_array_index (args, i) != NULL; ++i)
    {
      arg_adr = (gchar **)&g_ptr_array_index (args, i);
      if (NULL != (new_arg = gb_beautifier_helper_match_and_replace (*arg_adr, "@s@", src_path)))
        {
          g_free (*arg_adr);
          *arg_adr = new_arg;
        }
      else if (has_config &&
               NULL != (new_arg = gb_beautifier_helper_match_and_replace (*arg_adr, "@c@", config_path)))
        {
          g_free (*arg_adr);
          *arg_adr = new_arg;
        }
    }
}

static GSubprocess *
gb_beautifier_process_create_generic (GbBeautifierEditorAddin  *self,
                                      ProcessState             *state,
                                      GError                  **error)
{
  GSubprocess *subprocess = NULL;
  g_autofree gchar *src_path = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (state != NULL);

  src_path = g_file_get_path (state->src_file);

  g_assert (!dzl_str_empty0 (src_path));
  g_assert (!dzl_str_empty0 (state->lang_id));

  command_args_expand (self, state->command_args_strs, state);

  subprocess = g_subprocess_newv ((const gchar * const *)state->command_args_strs->pdata,
                                  G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                  error);

  return subprocess;
}

static GSubprocess *
gb_beautifier_process_create_for_clang_format (GbBeautifierEditorAddin  *self,
                                               ProcessState             *state,
                                               GError                  **error)
{
  g_autoptr(GSubprocessLauncher) launcher = NULL;
  GSubprocess *subprocess = NULL;
  GPtrArray *args;
  g_autofree gchar *tmp_workdir = NULL;
  g_autofree gchar *tmp_config_path = NULL;
  g_autofree gchar *tmp_src_path = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (state != NULL);

  g_assert (!dzl_str_empty0 (state->lang_id));

  tmp_workdir = g_build_filename (self->tmp_dir, "clang-XXXXXX.txt", NULL);
  if (g_mkdtemp (tmp_workdir) == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to create temporary directory for the Beautifier plugin");

      return NULL;
    }

  state->tmp_workdir_file = g_file_new_for_path (tmp_workdir);
  tmp_config_path = g_build_filename (tmp_workdir,
                                      ".clang-format",
                                      NULL);
  state->tmp_config_file = g_file_new_for_path (tmp_config_path);
  if (!g_file_copy (state->config_file,
                    state->tmp_config_file,
                    G_FILE_COPY_OVERWRITE,
                    NULL, NULL, NULL,
                    error))
    return NULL;

  tmp_src_path = g_build_filename (tmp_workdir,
                                   "src_file",
                                   NULL);
  state->tmp_src_file = g_file_new_for_path (tmp_src_path);
  if (!g_file_copy (state->src_file,
                    state->tmp_src_file,
                    G_FILE_COPY_OVERWRITE,
                    NULL, NULL, NULL,
                    error))
    return NULL;

  args = g_ptr_array_new ();
  g_ptr_array_add (args, (gchar *)"clang-format");
  g_ptr_array_add (args, (gchar *)"-style=file");
  g_ptr_array_add (args, (gchar *)tmp_src_path);
  g_ptr_array_add (args, NULL);

  launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
  g_subprocess_launcher_set_cwd (launcher, tmp_workdir);
  subprocess = g_subprocess_launcher_spawnv (launcher,
                                             (const gchar * const *)args->pdata,
                                             error);

  g_ptr_array_free (args, TRUE);
  return subprocess;
}

static void
process_communicate_utf8_cb (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(GSubprocess) process = (GSubprocess *)object;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GBytes) stdout_gb = NULL;
  g_autoptr(GBytes) stderr_gb = NULL;
  const gchar *stdout_str = NULL;
  const gchar *stderr_str = NULL;
  g_autoptr(GError) error = NULL;
  IdeCompletion *completion;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  ProcessState *state;

  g_assert (G_IS_SUBPROCESS (process));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!g_subprocess_communicate_finish (process, result, &stdout_gb, &stderr_gb, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ide_task_return_error_if_cancelled (task))
    return;

  state = (ProcessState *)ide_task_get_task_data (task);
  if (stderr_gb != NULL &&
      NULL != (stderr_str = g_bytes_get_data (stderr_gb, NULL)) &&
      !dzl_str_empty0 (stderr_str) &&
      g_utf8_validate (stderr_str, -1, NULL))
    {
      if (g_subprocess_get_if_exited (process) && g_subprocess_get_exit_status (process) != 0)
        {
          ide_object_warning (state->self,
                              /* translators: %s is replaced with the command error message */
                              _("Beautifier plugin: command error output: %s"),
                              stderr_str);
        }
    }

  if (stdout_gb != NULL)
    stdout_str = g_bytes_get_data (stdout_gb, NULL);

  if (stdout_gb != NULL && dzl_str_empty0 (stdout_str))
    {
      ide_object_warning (state->self, _("Beautifier plugin: the command output is empty"));
    }
  else if (g_utf8_validate (stdout_str, -1, NULL))
    {
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->source_view));
      completion = ide_source_view_get_completion (IDE_SOURCE_VIEW (state->source_view));

      ide_completion_block_interactive (completion);
      gtk_text_buffer_begin_user_action (buffer);

      gtk_text_buffer_get_iter_at_mark (buffer, &begin, state->begin_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, &end, state->end_mark);
      gtk_text_buffer_delete (buffer, &begin, &end);
      gtk_text_buffer_insert (buffer, &begin, stdout_str, -1);

      /* Get valid iters from marks */
      gtk_text_buffer_get_iter_at_mark (buffer, &begin, state->begin_mark);
      gtk_text_buffer_get_iter_at_mark (buffer, &end, state->end_mark);
      gtk_text_buffer_select_range (buffer, &begin, &end);
      g_signal_emit_by_name (state->source_view, "selection-theatric", IDE_SOURCE_VIEW_THEATRIC_EXPAND);

      gtk_text_buffer_end_user_action (buffer);
      ide_completion_unblock_interactive (completion);

      ide_task_return_boolean (task, TRUE);
    }
  else
    ide_object_warning (state->self,_("Beautify plugin: the output is not a valid UTF-8 text"));
}

static void
create_text_tmp_file_cb (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GbBeautifierEditorAddin  *self = (GbBeautifierEditorAddin  *)object;
  g_autoptr (IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;
  ProcessState *state;
  GSubprocess *process;
  GCancellable *cancellable;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  state = (ProcessState *)ide_task_get_task_data (task);
  if (NULL == (state->src_file = gb_beautifier_helper_create_tmp_file_finish (self, result, &error)))
    goto fail;

  if (state->command == GB_BEAUTIFIER_CONFIG_COMMAND_CLANG_FORMAT)
    process = gb_beautifier_process_create_for_clang_format (self, state, &error);
  else
    process = gb_beautifier_process_create_generic (self, state, &error);

  if (process != NULL)
    {
      if (ide_task_return_error_if_cancelled (task))
        g_object_unref (process);
      else
        {
          cancellable = ide_task_get_cancellable (task);
          g_subprocess_communicate_async (process,
                                          NULL,
                                          cancellable,
                                          process_communicate_utf8_cb,
                                          g_steal_pointer (&task));
        }

      return;
    }

fail:
  ide_task_return_error (task, g_steal_pointer (&error));
  return;
}

/* We just need to keep the string part */
static GPtrArray *
command_args_copy (GArray *args)
{
  GPtrArray *args_copy;

  g_assert (args != NULL);

  args_copy = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; i < args->len; ++i)
    {
      GbBeautifierCommandArg *arg = &g_array_index (args, GbBeautifierCommandArg, i);

      g_ptr_array_add (args_copy, g_strdup (arg->str));
    }

  g_ptr_array_add (args_copy, NULL);

  return args_copy;
}

void
gb_beautifier_process_launch_async (GbBeautifierEditorAddin  *self,
                                    IdeSourceView            *source_view,
                                    GtkTextIter              *begin,
                                    GtkTextIter              *end,
                                    GbBeautifierConfigEntry  *entry,
                                    GAsyncReadyCallback       callback,
                                    GCancellable             *cancellable,
                                    gpointer                  user_data)
{
  GtkTextBuffer *buffer;
  ProcessState *state;
  g_autoptr(IdeTask) task = NULL;
  const gchar *lang_id;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (entry != NULL);
  g_assert (callback != NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  lang_id = gb_beautifier_helper_get_lang_id (self, source_view);

  if (begin == NULL)
    gtk_text_buffer_get_start_iter (buffer, begin);

  if (end == NULL)
    gtk_text_buffer_get_end_iter (buffer, begin);

  g_assert (gtk_text_iter_get_buffer (begin) == buffer);
  g_assert (gtk_text_iter_get_buffer (end) == buffer);

  state = g_slice_new0 (ProcessState);
  state->self = self;
  state->source_view = source_view;

  gtk_text_iter_order (begin, end);
  state->text = gtk_text_buffer_get_text (buffer, begin, end, FALSE);
  state->begin_mark = gtk_text_buffer_create_mark (buffer, NULL, begin, TRUE);
  state->end_mark = gtk_text_buffer_create_mark (buffer, NULL, end, FALSE);
  state->command = entry->command;
  state->lang_id = g_strdup (lang_id);

  if (G_IS_FILE (entry->config_file))
    state->config_file = g_object_ref (entry->config_file);

  if (entry->command_args != NULL)
    state->command_args_strs = command_args_copy (entry->command_args);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gb_beautifier_process_launch_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_task_data (task, state, process_state_free);

  gb_beautifier_helper_create_tmp_file_async (self,
                                              state->text,
                                              create_text_tmp_file_cb,
                                              cancellable,
                                              g_steal_pointer (&task));
}

gboolean
gb_beautifier_process_launch_finish (GbBeautifierEditorAddin  *self,
                                     GAsyncResult             *result,
                                     GError                  **error)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (ide_task_is_valid (result, self));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
