/* ide-editor-page-actions.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-page-actions"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-code.h>
#include <libide-gui.h>

#include "ide-editor-surface.h"
#include "ide-editor-private.h"
#include "ide-editor-print-operation.h"
#include "ide-editor-settings-dialog.h"

typedef struct
{
  IdeEditorPage *self;
  guint line;
  guint line_offset;
} ReloadState;

static void
reload_state_free (ReloadState *state)
{
  g_clear_object (&state->self);
  g_slice_free (ReloadState, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ReloadState, reload_state_free)

static void
ide_editor_page_actions_reload_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(ReloadState) state = user_data;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_EDITOR_PAGE (state->self));

  if (state->self->progress_bar != NULL)
    dzl_gtk_widget_hide_with_fade (GTK_WIDGET (state->self->progress_bar));

  if (!(buffer = ide_buffer_manager_load_file_finish (bufmgr, result, &error)))
    {
      g_warning ("%s", error->message);
      ide_page_report_error (IDE_PAGE (state->self),
                             /* translators: %s is the error message */
                             _("Failed to load file: %s"), error->message);
      ide_page_set_failed (IDE_PAGE (state->self), TRUE);
    }
  else
    {
      IdeSourceView *view;
      GtkTextIter iter;

      view = ide_editor_page_get_view (state->self);
      gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                               &iter,
                                               state->line,
                                               state->line_offset);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
      ide_source_view_scroll_to_iter (view,
                                      &iter,
                                      .25,
                                      IDE_SOURCE_SCROLL_BOTH,
                                      1.0,
                                      0.5,
                                      FALSE);

    }

  gtk_revealer_set_reveal_child (state->self->modified_revealer, FALSE);
}

static void
ide_editor_page_actions_reload (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeContext) context = NULL;
  IdeBufferManager *bufmgr;
  ReloadState *state;
  GtkTextIter iter;
  IdeBuffer *buffer;
  GFile *file;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  buffer = ide_editor_page_get_buffer (self);
  context = ide_buffer_ref_context (buffer);
  bufmgr = ide_buffer_manager_from_context (context);
  file = ide_buffer_get_file (buffer);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
  gtk_widget_show (GTK_WIDGET (self->progress_bar));

  notif = ide_notification_new ();

  ide_buffer_get_selection_bounds (buffer, &iter, NULL);

  state = g_slice_new0 (ReloadState);
  state->self = g_object_ref (self);
  state->line = gtk_text_iter_get_line (&iter);
  state->line_offset = gtk_text_iter_get_line_offset (&iter);

  ide_buffer_manager_load_file_async (bufmgr,
                                      file,
                                      IDE_BUFFER_OPEN_FLAGS_FORCE_RELOAD,
                                      notif,
                                      NULL,
                                      ide_editor_page_actions_reload_cb,
                                      g_steal_pointer (&state));

  g_object_bind_property (notif, "progress",
                          self->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);
}

static void
handle_print_result (IdeEditorPage           *self,
                     GtkPrintOperation       *operation,
                     GtkPrintOperationResult  result)
{
  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (GTK_IS_PRINT_OPERATION (operation));

  if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
    {
      g_autoptr(GError) error = NULL;

      gtk_print_operation_get_error (operation, &error);

      g_warning ("%s", error->message);
      ide_page_report_error (IDE_PAGE (self),
                             /* translators: %s is the error message */
                             _("Print failed: %s"), error->message);
    }
}

static void
print_done (GtkPrintOperation       *operation,
            GtkPrintOperationResult  result,
            gpointer                 user_data)
{
  IdeEditorPage *self = user_data;

  g_assert (GTK_IS_PRINT_OPERATION (operation));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  handle_print_result (self, operation, result);

  g_object_unref (operation);
  g_object_unref (self);
}

static void
ide_editor_page_actions_print (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  g_autoptr(IdeEditorPrintOperation) operation = NULL;
  IdeEditorPage *self = user_data;
  IdeSourceView *source_view;
  GtkWidget *toplevel;
  GtkPrintOperationResult result;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);

  source_view = ide_editor_page_get_view (self);
  operation = ide_editor_print_operation_new (source_view);

  /* keep a ref until "done" is emitted */
  g_object_ref (operation);
  g_signal_connect_after (g_object_ref (operation),
                          "done",
                          G_CALLBACK (print_done),
                          g_object_ref (self));

  result = gtk_print_operation_run (GTK_PRINT_OPERATION (operation),
                                    GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                    GTK_WINDOW (toplevel),
                                    NULL);

  handle_print_result (self, GTK_PRINT_OPERATION (operation), result);
}

static void
ide_editor_page_actions_save_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeEditorPage) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    {
      g_warning ("%s", error->message);
      ide_page_report_error (IDE_PAGE (self),
                             /* translators: %s is the error message */
                             _("Failed to save file: %s"), error->message);
      ide_page_set_failed (IDE_PAGE (self), TRUE);
    }

  if (self->progress_bar != NULL)
    dzl_gtk_widget_hide_with_fade (GTK_WIDGET (self->progress_bar));
}

static void
ide_editor_page_actions_save (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GFile) local_file = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeBuffer *buffer;
  GFile *file;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  buffer = ide_editor_page_get_buffer (self);
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  context = ide_buffer_ref_context (buffer);
  g_return_if_fail (IDE_IS_CONTEXT (context));

  file = ide_buffer_get_file (buffer);
  g_return_if_fail (G_IS_FILE (file));

  workdir = ide_context_ref_workdir (context);
  g_assert (G_IS_FILE (workdir));

  if (ide_buffer_get_is_temporary (buffer))
    {
      GtkFileChooserNative *dialog;
      GtkWidget *toplevel;
      gint ret;

      toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);

      dialog = gtk_file_chooser_native_new (_("Save File"),
                                            GTK_WINDOW (toplevel),
                                            GTK_FILE_CHOOSER_ACTION_SAVE,
                                            _("Save"), _("Cancel"));

      g_object_set (dialog,
                    "do-overwrite-confirmation", TRUE,
                    "local-only", FALSE,
                    "modal", TRUE,
                    "select-multiple", FALSE,
                    "show-hidden", FALSE,
                    NULL);

      gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (dialog), workdir, NULL);

      ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));

      if (ret == GTK_RESPONSE_ACCEPT)
        file = local_file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));

      if (local_file == NULL)
        return;
    }

  ide_buffer_save_file_async (buffer,
                              file,
                              NULL,
                              &notif,
                              ide_editor_page_actions_save_cb,
                              g_object_ref (self));

  g_object_bind_property (notif, "progress", self->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);

  gtk_widget_show (GTK_WIDGET (self->progress_bar));
}


static void
ide_editor_page_actions_save_as_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeEditorPage) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    {
      /* In this case, the editor page hasn't failed since this is for an
       * alternate file (which maybe we just don't have access to on the
       * network or something).
       *
       * But we do still need to notify the user of the error.
       */
      g_warning ("%s", error->message);
      ide_page_report_error (IDE_PAGE (self),
                             /* translators: %s is the underlying error message */
                             _("Failed to save file: %s"),
                             error->message);
    }

  dzl_gtk_widget_hide_with_fade (GTK_WIDGET (self->progress_bar));
}

static void
ide_editor_page_actions_save_as (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  GtkFileChooserNative *dialog;
  IdeBuffer *buffer;
  GtkWidget *toplevel;
  GFile *file;
  gint ret;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  buffer = ide_editor_page_get_buffer (self);
  file = ide_buffer_get_file (buffer);

  /* Just redirect to the save flow if we have a temporary
   * file currently. That way we can avoid splitting the
   * flow to handle both cases here.
   */
  if (ide_buffer_get_is_temporary (buffer))
    {
      ide_editor_page_actions_save (action, NULL, user_data);
      return;
    }

  toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
  dialog = gtk_file_chooser_native_new (_("Save File As"),
                                        GTK_WINDOW (toplevel),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("Save As"),
                                        _("Cancel"));

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
  gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (dialog), FALSE);

  if (file != NULL)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), file, NULL);

  ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));

  if (ret == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) save_as = NULL;
      g_autoptr(IdeNotification) notif = NULL;

      save_as = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

      ide_buffer_save_file_async (buffer,
                                  save_as,
                                  NULL,
                                  &notif,
                                  ide_editor_page_actions_save_as_cb,
                                  g_object_ref (self));

      g_object_bind_property (notif, "progress", self->progress_bar, "fraction",
                              G_BINDING_SYNC_CREATE);

      gtk_widget_show (GTK_WIDGET (self->progress_bar));
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

static void
ide_editor_page_actions_find (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end))
    {
      g_autofree gchar *word = gtk_text_iter_get_slice (&begin, &end);
      ide_editor_search_set_search_text (self->search, word);
    }

  ide_editor_search_bar_set_replace_mode (self->search_bar, FALSE);
  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static void
ide_editor_page_actions_find_replace (GSimpleAction *action,
                                      GVariant      *variant,
                                      gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end))
    {
      g_autofree gchar *word = gtk_text_iter_get_slice (&begin, &end);
      ide_editor_search_set_search_text (self->search, word);
    }

  ide_editor_search_bar_set_replace_mode (self->search_bar, TRUE);
  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search_bar));
}

static void
ide_editor_page_actions_hide_search (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeEditorPage *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
ide_editor_page_actions_notify_file_settings (IdeEditorPage *self,
                                              GParamSpec    *pspec,
                                              IdeSourceView *source_view)
{
  IdeFileSettings *file_settings;
  GActionGroup *group;

  g_assert (IDE_IS_EDITOR_PAGE (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "file-settings");
  g_assert (DZL_IS_PROPERTIES_GROUP (group));

  file_settings = ide_source_view_get_file_settings (source_view);
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  g_object_set (group, "object", file_settings, NULL);
}

static void
ide_editor_page_actions_move_next_error (GSimpleAction *action,
                                         GVariant      *variant,
                                         gpointer       user_data)
{
  ide_editor_page_move_next_error (user_data);
}

static void
ide_editor_page_actions_move_previous_error (GSimpleAction *action,
                                             GVariant      *variant,
                                             gpointer       user_data)
{
  ide_editor_page_move_previous_error (user_data);
}

static void
ide_editor_page_actions_activate_next_search_result (GSimpleAction *action,
                                                     GVariant      *variant,
                                                     gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_move_next_search_result (self);

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (self->buffer), &begin, &end);
  ide_source_view_scroll_to_insert (self->source_view);
}

static void
ide_editor_page_actions_move_next_search_result (GSimpleAction *action,
                                                 GVariant      *variant,
                                                 gpointer       user_data)
{
  ide_editor_page_move_next_search_result (user_data);
}

static void
ide_editor_page_actions_move_previous_search_result (GSimpleAction *action,
                                                     GVariant      *variant,
                                                     gpointer       user_data)
{
  ide_editor_page_move_previous_search_result (user_data);
}

static void
ide_editor_page_actions_properties (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeEditorPage *self = user_data;
  IdeEditorSettingsDialog *dialog;

  g_assert (IDE_IS_EDITOR_PAGE (self));

  dialog = ide_editor_settings_dialog_new (self);
  g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
  ide_gtk_window_present (GTK_WINDOW (dialog));
}

static void
ide_editor_page_actions_toggle_map (GSimpleAction *action,
                                    GVariant      *variant,
                                    gpointer       user_data)
{
  IdeEditorPage *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_PAGE (self));

  ide_editor_page_set_show_map (self, !ide_editor_page_get_show_map (self));
}

static const GActionEntry editor_view_entries[] = {
  { "activate-next-search-result", ide_editor_page_actions_activate_next_search_result },
  { "find", ide_editor_page_actions_find },
  { "find-replace", ide_editor_page_actions_find_replace },
  { "hide-search", ide_editor_page_actions_hide_search },
  { "move-next-error", ide_editor_page_actions_move_next_error },
  { "move-next-search-result", ide_editor_page_actions_move_next_search_result },
  { "move-previous-error", ide_editor_page_actions_move_previous_error },
  { "move-previous-search-result", ide_editor_page_actions_move_previous_search_result },
  { "properties", ide_editor_page_actions_properties },
  { "print", ide_editor_page_actions_print },
  { "reload", ide_editor_page_actions_reload },
  { "save", ide_editor_page_actions_save },
  { "save-as", ide_editor_page_actions_save_as },
  { "toggle-map", ide_editor_page_actions_toggle_map },
};

void
_ide_editor_page_init_actions (IdeEditorPage *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(DzlPropertiesGroup) sv_props = NULL;
  g_autoptr(DzlPropertiesGroup) file_props = NULL;
  IdeSourceView *source_view;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  source_view = ide_editor_page_get_view (self);

  /* Setup our user-facing actions */
  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   editor_view_entries,
                                   G_N_ELEMENTS (editor_view_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-page", G_ACTION_GROUP (group));

  /* We want to access some settings properties as stateful GAction so they
   * manipulated using regular Gtk widgets from the properties panel.
   */
  sv_props = dzl_properties_group_new (G_OBJECT (source_view));
  dzl_properties_group_add_all_properties (sv_props);
  dzl_properties_group_add_property_full (sv_props,
                                          "use-spaces",
                                          "insert-spaces-instead-of-tabs",
                                          DZL_PROPERTIES_FLAGS_STATEFUL_BOOLEANS);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "source-view", G_ACTION_GROUP (sv_props));

  /*
   * We want to bind our file-settings, used to tweak values in the
   * source-view, to a GActionGroup that can be manipulated by the properties
   * editor. Make sure we get notified of changes and sink the current state.
   */
  file_props = dzl_properties_group_new_for_type (IDE_TYPE_FILE_SETTINGS);
  dzl_properties_group_add_all_properties (file_props);
  g_signal_connect_swapped (source_view,
                            "notify::file-settings",
                            G_CALLBACK (ide_editor_page_actions_notify_file_settings),
                            self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "file-settings", G_ACTION_GROUP (file_props));
  ide_editor_page_actions_notify_file_settings (self, NULL, source_view);
}

void
_ide_editor_page_update_actions (IdeEditorPage *self)
{
  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

}
