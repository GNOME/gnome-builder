/* gb-beautifier-process.c
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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
  GbBeautifierWorkbenchAddin *self;
  IdeSourceView              *source_view;
  GtkTextMark                *begin_mark;
  GtkTextMark                *end_mark;
  GbBeautifierConfigCommand   command;
  GFile                      *src_file;
  GFile                      *config_file;
  gchar                      *lang_id;
  gchar                      *text;
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
  g_free (state->lang_id);
  g_free (state->text);

  g_slice_free (ProcessState, state);
}

static GSubprocess *
gb_beautifier_process_create_for_uncrustify (GbBeautifierWorkbenchAddin *self,
                                             ProcessState               *state,
                                             GError                     *error)
{
  GSubprocess *subprocess = NULL;
  GPtrArray *args;
  gchar *config_path;
  gchar *src_path;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (state != NULL);

  config_path = g_file_get_path (state->config_file);
  src_path = g_file_get_path (state->src_file);

  g_assert (!ide_str_empty0 (config_path));
  g_assert (!ide_str_empty0 (src_path));
  g_assert (!ide_str_empty0 (state->lang_id));

  args = g_ptr_array_new ();
  g_ptr_array_add (args, "uncrustify");

  g_ptr_array_add (args, "-c");
  g_ptr_array_add (args, config_path);
  g_ptr_array_add (args, "-f");
  g_ptr_array_add (args, src_path);
  g_ptr_array_add (args, NULL);

  subprocess = g_subprocess_newv ((const gchar * const *)args->pdata,
                                  G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                  G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                  &error);

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
  g_autoptr(GError) error = NULL;
  GtkSourceCompletion *completion;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  ProcessState *state;

  g_assert (G_IS_SUBPROCESS (process));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!g_subprocess_communicate_utf8_finish (process, result, &stdout_str, NULL, &error))
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
}

static void
create_tmp_file_cb (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GbBeautifierWorkbenchAddin  *self = (GbBeautifierWorkbenchAddin  *)object;
  g_autoptr (GTask) task = (GTask *)user_data;
  g_autoptr(GError) error = NULL;
  ProcessState *state;
  GSubprocess *process;
  GCancellable *cancellable;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  state = (ProcessState *)g_task_get_task_data (task);
  if (NULL == (state->src_file = gb_beautifier_helper_create_tmp_file_finish (self, result, &error)))
    goto fail;

  if (state->command == GB_BEAUTIFIER_CONFIG_COMMAND_UNCRUSTIFY)
    process = gb_beautifier_process_create_for_uncrustify (self, state, error);
  else
    g_assert_not_reached ();

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

void
gb_beautifier_process_launch_async (GbBeautifierWorkbenchAddin  *self,
                                    IdeSourceView               *source_view,
                                    GtkTextIter                 *begin,
                                    GtkTextIter                 *end,
                                    GbBeautifierConfigEntry     *entry,
                                    GAsyncReadyCallback          callback,
                                    GCancellable                *cancellable,
                                    gpointer                     user_data)
{
  GtkTextBuffer *buffer;
  ProcessState *state;
  g_autoptr(GTask) task = NULL;
  const gchar *lang_id;

  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
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
  state->config_file = g_file_dup (entry->file);
  state->lang_id = g_strdup (lang_id);

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
gb_beautifier_process_launch_finish (GbBeautifierWorkbenchAddin  *self,
                                     GAsyncResult                *result,
                                     GError                     **error)
{
  g_assert (GB_IS_BEAUTIFIER_WORKBENCH_ADDIN (self));
  g_assert (g_task_is_valid (result, self));

  return g_task_propagate_boolean (G_TASK (result), error);
}
