/* gb-beautifier-process.c
 *
 * Copyright © 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <gtksourceview/gtksource.h>
#include "ide.h"

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
  GPtrArray                 *command_args;
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

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->source_view));
  gtk_text_buffer_delete_mark (buffer, state->begin_mark);
  gtk_text_buffer_delete_mark (buffer, state->end_mark);

  gb_beautifier_helper_remove_tmp_file (state->self, state->src_file);
  g_clear_object (&state->src_file);
  g_clear_object (&state->config_file);

  if (state->tmp_config_file != NULL)
    g_file_delete (state->tmp_config_file, NULL, NULL);
  if (state->tmp_src_file != NULL)
    g_file_delete (state->tmp_src_file, NULL, NULL);
  if (state->tmp_workdir_file != NULL)
    g_file_delete (state->tmp_workdir_file, NULL, NULL);

  g_clear_object (&state->tmp_workdir_file);
  g_clear_object (&state->tmp_config_file);
  g_clear_object (&state->tmp_src_file);

  g_free (state->lang_id);
  g_free (state->text);

  if (state->command_args != NULL)
    g_ptr_array_unref (state->command_args);

  g_slice_free (ProcessState, state);
}

static gchar *
match_and_replace (const gchar *str,
                   const gchar *pattern,
                   const gchar *replacement)
{
  g_autofree gchar *head = NULL;
  g_autofree gchar *tail = NULL;
  gchar *needle;
  gsize head_len;

  g_assert (!ide_str_empty0 (str));
  g_assert (!ide_str_empty0 (pattern));

  if (NULL != (needle = g_strstr_len (str, -1, pattern)))
    {
      head_len = needle - str;
      if (head_len > 0)
        head = g_strndup (str, head_len);
      else
        head = g_strdup ("");

      tail = needle + strlen (pattern);
      if (*tail != '\0')
        tail = g_strdup (tail);
      else
        tail = g_strdup ("");

      return g_strconcat (head, replacement, tail, NULL);
    }
  else
    return NULL;
}

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

  for (gint i = 0; g_ptr_array_index (args, i) != NULL; ++i)
    {
      arg_adr = (gchar **)&g_ptr_array_index (args, i);
      if (NULL != (new_arg = match_and_replace (*arg_adr, "@s@", src_path)))
        {
          g_free (*arg_adr);
          *arg_adr = new_arg;
        }
      else if (has_config &&
               NULL != (new_arg = match_and_replace (*arg_adr, "@c@", config_path)))
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

  g_assert (!ide_str_empty0 (src_path));
  g_assert (!ide_str_empty0 (state->lang_id));

  command_args_expand (self, state->command_args, state);
  subprocess = g_subprocess_newv ((const gchar * const *)state->command_args->pdata,
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
  gchar *config_path;
  gchar *src_path;
  g_autofree gchar *tmp_workdir = NULL;
  g_autofree gchar *tmp_config_path = NULL;
  g_autofree gchar *tmp_src_path = NULL;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (state != NULL);

  config_path = g_file_get_path (state->config_file);
  src_path = g_file_get_path (state->src_file);

  g_assert (!ide_str_empty0 (config_path));
  g_assert (!ide_str_empty0 (src_path));
  g_assert (!ide_str_empty0 (state->lang_id));

  if (NULL == (tmp_workdir = g_dir_make_tmp ("gnome-builder-beautify-XXXXXX", error)))
    return NULL;

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
  g_ptr_array_add (args, "clang-format");
  g_ptr_array_add (args, "-style=file");
  g_ptr_array_add (args, tmp_src_path);
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
  g_autoptr (GSubprocess) process = (GSubprocess *)object;
  g_autoptr (GTask) task = (GTask *)user_data;
  g_autofree gchar *stdout_str = NULL;
  g_autofree gchar *stderr_str = NULL;
  g_autoptr(GError) error = NULL;
  GtkSourceCompletion *completion;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  ProcessState *state;
  gboolean status;

  g_assert (G_IS_SUBPROCESS (process));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_subprocess_communicate_utf8_finish (process, result, &stdout_str, &stderr_str, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (g_task_return_error_if_cancelled (task))
    return;

  state = (ProcessState *)g_task_get_task_data (task);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (state->source_view));
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (state->source_view));

  if (!ide_str_empty0 (stdout_str))
    {
      gtk_source_completion_block_interactive (completion);
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
      gtk_source_completion_unblock_interactive (completion);

      g_task_return_boolean (task, TRUE);
    }
  else
    g_warning ("beautify plugin: output empty");

  if (g_subprocess_get_if_exited (process))
    {
      status = g_subprocess_get_exit_status (process);
      if (status != 0 &&
          stderr_str != NULL &&
          !ide_str_empty0 (stderr_str))
        {
          g_warning ("beautify plugin stderr:\n%s", stderr_str);
        }
    }
}

static void
create_tmp_file_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GbBeautifierEditorAddin  *self = (GbBeautifierEditorAddin  *)object;
  g_autoptr (GTask) task = (GTask *)user_data;
  g_autoptr(GError) error = NULL;
  ProcessState *state;
  GSubprocess *process;
  GCancellable *cancellable;

  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = (ProcessState *)g_task_get_task_data (task);
  if (NULL == (state->src_file = gb_beautifier_helper_create_tmp_file_finish (self, result, &error)))
    goto fail;

  if (state->command == GB_BEAUTIFIER_CONFIG_COMMAND_CLANG_FORMAT)
    process = gb_beautifier_process_create_for_clang_format (self, state, &error);
  else
    process = gb_beautifier_process_create_generic (self, state, &error);

  if (process != NULL)
    {
      if (g_task_return_error_if_cancelled (task))
        g_object_unref (process);
      else
        {
          cancellable = g_task_get_cancellable (task);
          g_subprocess_communicate_utf8_async (process,
                                               NULL,
                                               cancellable,
                                               process_communicate_utf8_cb,
                                               g_steal_pointer (&task));
        }

      return;
    }

fail:
  g_task_return_error (task, g_steal_pointer (&error));
  return;
}

static GPtrArray *
command_args_copy (GPtrArray *args)
{
  GPtrArray *args_copy;

  g_assert (args != NULL);

  args_copy = g_ptr_array_new_with_free_func (g_free);
  for (gint i = 0; g_ptr_array_index (args, i) != NULL; ++i)
    g_ptr_array_add (args_copy, g_strdup (g_ptr_array_index (args, i)));

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
  g_autoptr(GTask) task = NULL;
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
    state->config_file = g_file_dup (entry->config_file);

  if (entry->command_args != NULL)
    state->command_args = command_args_copy (entry->command_args);
  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gb_beautifier_process_launch_async);
  g_task_set_task_data (task, state, process_state_free);

  gb_beautifier_helper_create_tmp_file_async (self,
                                              state->text,
                                              create_tmp_file_cb,
                                              cancellable,
                                              g_steal_pointer (&task));
}

gboolean
gb_beautifier_process_launch_finish (GbBeautifierEditorAddin  *self,
                                     GAsyncResult             *result,
                                     GError                  **error)
{
  g_assert (GB_IS_BEAUTIFIER_EDITOR_ADDIN (self));
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_boolean (G_TASK (result), error);
}
