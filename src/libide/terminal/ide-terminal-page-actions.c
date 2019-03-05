/* gb-editor-view-actions.c
 *
 * Copyright 2015 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-terminal-page"

#include <glib/gi18n.h>
#include <libide-gui.h>
#include <libide-terminal.h>
#include <string.h>

#include "ide-terminal-page-actions.h"
#include "ide-terminal-page-private.h"

typedef struct
{
  VteTerminal    *terminal;
  GFile          *file;
  GOutputStream  *stream;
  gchar          *buffer;
} SaveTask;

static void
savetask_free (gpointer data)
{
  SaveTask *savetask = (SaveTask *)data;

  if (savetask != NULL)
    {
      g_clear_object (&savetask->file);
      g_clear_object (&savetask->stream);
      g_clear_object (&savetask->terminal);
      g_clear_pointer (&savetask->buffer, g_free);
      g_slice_free (SaveTask, savetask);
    }
}

static gboolean
ide_terminal_page_actions_save_finish (IdeTerminalPage  *view,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  IdeTask *task = (IdeTask *)result;

  g_return_val_if_fail (ide_task_is_valid (result, view), FALSE);

  g_return_val_if_fail (IDE_IS_TERMINAL_PAGE (view), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (task), FALSE);

  return ide_task_propagate_boolean (task, error);
}

static void
save_worker (IdeTask      *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  SaveTask *savetask = (SaveTask *)task_data;
  g_autoptr(GError) error = NULL;
  gboolean ret;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_TERMINAL_PAGE (source_object));
  g_assert (savetask != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (savetask->buffer != NULL)
    {
      g_autoptr(GInputStream) input_stream = NULL;

      input_stream = g_memory_input_stream_new_from_data (savetask->buffer, -1, NULL);
      ret = g_output_stream_splice (G_OUTPUT_STREAM (savetask->stream),
                                    G_INPUT_STREAM (input_stream),
                                    G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                    cancellable,
                                    &error);
    }
  else
    {
      ret = vte_terminal_write_contents_sync (savetask->terminal,
                                              G_OUTPUT_STREAM (savetask->stream),
                                              VTE_WRITE_DEFAULT,
                                              cancellable,
                                              &error);
    }

  if (ret)
    ide_task_return_boolean (task, TRUE);
  else
    ide_task_return_error (task, g_steal_pointer (&error));
}

static void
ide_terminal_page_actions_save_async (IdeTerminalPage       *view,
                                     VteTerminal          *terminal,
                                     GFile                *file,
                                     GAsyncReadyCallback   callback,
                                     GCancellable         *cancellable,
                                     gpointer              user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFileOutputStream) output_stream = NULL;
  g_autoptr(GError) error = NULL;
  SaveTask *savetask;

  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (view, cancellable, callback, user_data);

  output_stream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, &error);

  if (output_stream != NULL)
    {
      savetask = g_slice_new0 (SaveTask);
      savetask->file = g_object_ref (file);
      savetask->stream = g_object_ref (G_OUTPUT_STREAM (output_stream));
      savetask->terminal = g_object_ref (terminal);
      savetask->buffer = g_steal_pointer (&view->selection_buffer);

      ide_task_set_task_data (task, savetask, savetask_free);
      save_worker (task, view, savetask, cancellable);
    }
  else
    ide_task_return_error (task, g_steal_pointer (&error));
}

static void
save_as_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  IdeTask *task = (IdeTask *)result;
  IdeTerminalPage *view = user_data;
  SaveTask *savetask;
  GFile *file;
  GError *error = NULL;

  savetask = ide_task_get_task_data (task);
  file = g_object_ref (savetask->file);

  if (!ide_terminal_page_actions_save_finish (view, result, &error))
    {
      g_object_unref (file);
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }
  else
    {
      g_clear_object (&view->save_as_file_top);
      view->save_as_file_top = file;
    }
}

static GFile *
get_last_focused_terminal_file (IdeTerminalPage *view)
{
  GFile *file = NULL;

  if (G_IS_FILE (view->save_as_file_top))
    file = view->save_as_file_top;

  return file;
}

static VteTerminal *
get_last_focused_terminal (IdeTerminalPage *view)
{
  return VTE_TERMINAL (view->terminal_top);
}

static gchar *
gb_terminal_get_selected_text (IdeTerminalPage  *view,
                               VteTerminal    **terminal_p)
{
  VteTerminal *terminal;
  gchar *buf = NULL;

  terminal = get_last_focused_terminal (view);
  if (terminal_p != NULL)
    *terminal_p = terminal;

  if (vte_terminal_get_has_selection (terminal))
    {
      vte_terminal_copy_primary (terminal);
      buf = gtk_clipboard_wait_for_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY));
    }

  return buf;
}

static void
save_as_response (GtkWidget *widget,
                  gint       response,
                  gpointer   user_data)
{
  g_autoptr(IdeTerminalPage) view = user_data;
  g_autoptr(GFile) file = NULL;
  GtkFileChooser *chooser = (GtkFileChooser *)widget;
  VteTerminal *terminal;

  g_assert (GTK_IS_FILE_CHOOSER (chooser));
  g_assert (IDE_IS_TERMINAL_PAGE (view));

  switch (response)
    {
    case GTK_RESPONSE_OK:
      file = gtk_file_chooser_get_file (chooser);
      terminal = get_last_focused_terminal (view);
      ide_terminal_page_actions_save_async (view, terminal, file, save_as_cb, NULL, view);
      break;

    case GTK_RESPONSE_CANCEL:
      g_free (view->selection_buffer);

    default:
      break;
    }

  gtk_widget_destroy (widget);
}

static void
ide_terminal_page_actions_save_as (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  IdeTerminalPage *view = user_data;
  GtkWidget *suggested;
  GtkWidget *toplevel;
  GtkWidget *dialog;
  GFile *file = NULL;

  g_assert (IDE_IS_TERMINAL_PAGE (view));

  /* We can't get this later because the dialog makes the terminal
   * unfocused and thus resets the selection
   */
  view->selection_buffer = gb_terminal_get_selected_text (view, NULL);

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
                         "action", GTK_FILE_CHOOSER_ACTION_SAVE,
                         "do-overwrite-confirmation", TRUE,
                         "local-only", FALSE,
                         "modal", TRUE,
                         "select-multiple", FALSE,
                         "show-hidden", FALSE,
                         "transient-for", toplevel,
                         "title", _("Save Terminal Content As"),
                         NULL);

  file = get_last_focused_terminal_file (view);
  if (file != NULL)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), file, NULL);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Save"), GTK_RESPONSE_OK,
                          NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  suggested = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (suggested),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  g_signal_connect (dialog, "response", G_CALLBACK (save_as_response), g_object_ref (view));

  ide_gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_terminal_page_actions_reset (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  IdeTerminalPage *self = user_data;
  VteTerminal *terminal;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  terminal = get_last_focused_terminal (self);
  vte_terminal_reset (terminal, TRUE, FALSE);
}

static void
ide_terminal_page_actions_reset_and_clear (GSimpleAction *action,
                                          GVariant      *param,
                                          gpointer       user_data)
{
  IdeTerminalPage *self = user_data;
  VteTerminal *terminal;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_TERMINAL_PAGE (self));

  terminal = get_last_focused_terminal (self);
  vte_terminal_reset (terminal, TRUE, TRUE);
}

static GActionEntry IdeTerminalPageActions[] = {
  { "save-as", ide_terminal_page_actions_save_as },
  { "reset", ide_terminal_page_actions_reset },
  { "reset-and-clear", ide_terminal_page_actions_reset_and_clear },
};

void
ide_terminal_page_actions_init (IdeTerminalPage *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeTerminalPageActions,
                                   G_N_ELEMENTS (IdeTerminalPageActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "terminal-view", G_ACTION_GROUP (group));
}
