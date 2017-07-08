/* ide-editor-view-actions.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-view-actions"

#include <glib/gi18n.h>

#include "files/ide-file.h"
#include "files/ide-file-settings.h"
#include "buffers/ide-buffer.h"
#include "buffers/ide-buffer-manager.h"
#include "editor/ide-editor-private.h"
#include "editor/ide-editor-print-operation.h"
#include "util/ide-progress.h"
#include "vcs/ide-vcs.h"

static void
ide_editor_view_actions_reload_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeEditorView) self = user_data;
  g_autoptr(IdeBuffer) buffer = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  dzl_gtk_widget_hide_with_fade (GTK_WIDGET (self->progress_bar));

  if (!(buffer = ide_buffer_manager_load_file_finish (buffer_manager, result, &error)))
    {
      // ide_layout_view_set_failure_message (IDE_LAYOUT_VIEW (self), error->message);
      g_warning ("%s", error->message);
      ide_layout_view_set_failed (IDE_LAYOUT_VIEW (self), TRUE);
    }
  else
    {
      ide_editor_view_scroll_to_line (self, 0);
    }
}

static void
ide_editor_view_actions_reload (GSimpleAction *action,
                                GVariant      *param,
                                gpointer       user_data)
{
  IdeEditorView *self = user_data;
  g_autoptr(IdeProgress) progress = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;
  IdeBuffer *buffer;
  IdeFile *file;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  buffer = ide_editor_view_get_buffer (self);
  context = ide_buffer_get_context (buffer);
  buffer_manager = ide_context_get_buffer_manager (context);
  file = ide_buffer_get_file (buffer);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->progress_bar), 0.0);
  gtk_widget_show (GTK_WIDGET (self->progress_bar));

  ide_buffer_manager_load_file_async (buffer_manager,
                                      file,
                                      TRUE,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      &progress,
                                      NULL,
                                      ide_editor_view_actions_reload_cb,
                                      g_object_ref (self));

  g_object_bind_property (progress, "fraction",
                          self->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);
}

static void
handle_print_result (IdeEditorView           *self,
                     GtkPrintOperation       *operation,
                     GtkPrintOperationResult  result)
{
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR)
    {
      g_autoptr(GError) error = NULL;

      gtk_print_operation_get_error (operation, &error);

      /* info bar */
      g_warning ("%s", error->message);
      // ide_layout_view_add_error (...);
    }
}

static void
print_done (GtkPrintOperation       *operation,
            GtkPrintOperationResult  result,
            gpointer                 user_data)
{
  IdeEditorView *self = user_data;

  g_assert (GTK_IS_PRINT_OPERATION (operation));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  handle_print_result (self, operation, result);

  g_object_unref (operation);
  g_object_unref (self);
}

static void
ide_editor_view_actions_print (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  g_autoptr(IdeEditorPrintOperation) operation = NULL;
  IdeEditorView *self = user_data;
  IdeSourceView *source_view;
  GtkWidget *toplevel;
  GtkPrintOperationResult result;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);

  source_view = ide_editor_view_get_view (self);
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
ide_editor_view_actions_save_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBufferManager *bufmgr = (IdeBufferManager *)object;
  g_autoptr(IdeEditorView) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (bufmgr));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  if (!ide_buffer_manager_save_file_finish (bufmgr, result, &error))
    {
      g_warning ("%s", error->message);
      // ide_layout_view_set_failure_message (IDE_LAYOUT_VIEW (self), error->message);
      ide_layout_view_set_failed (IDE_LAYOUT_VIEW (self), TRUE);
    }

  dzl_gtk_widget_hide_with_fade (GTK_WIDGET (self->progress_bar));
}

static void
ide_editor_view_actions_save (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeEditorView *self = user_data;
  IdeBufferManager *buffer_manager;
  g_autoptr(IdeProgress) progress = NULL;
  g_autoptr(IdeFile) local_file = NULL;
  IdeContext *context;
  IdeBuffer *buffer;
  IdeFile *file;
  IdeVcs *vcs;
  GFile *workdir;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  buffer = ide_editor_view_get_buffer (self);
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  context = ide_buffer_get_context (buffer);
  g_return_if_fail (IDE_IS_CONTEXT (context));

  file = ide_buffer_get_file (buffer);
  g_return_if_fail (IDE_IS_FILE (file));

  buffer_manager = ide_context_get_buffer_manager (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_FILE (workdir));

  if (ide_file_get_is_temporary (file))
    {
      GtkFileChooserNative *dialog;
      g_autoptr(GFile) gfile = NULL;
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
        {
          gfile = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
          file = local_file = ide_file_new (context, gfile);
        }

      gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));

      if (local_file == NULL)
        return;
    }

  ide_buffer_manager_save_file_async (buffer_manager,
                                      buffer,
                                      file,
                                      &progress,
                                      NULL,
                                      ide_editor_view_actions_save_cb,
                                      g_object_ref (self));

  g_object_bind_property (progress, "fraction",
                          self->progress_bar, "fraction",
                          G_BINDING_SYNC_CREATE);

  gtk_widget_show (GTK_WIDGET (self->progress_bar));
}


static void
ide_editor_view_actions_save_as_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;
  g_autoptr(IdeEditorView) self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_EDITOR_VIEW (self));

  if (!ide_buffer_manager_save_file_finish (buffer_manager, result, &error))
    {
      /* In this case, the editor view hasn't failed since this is for an
       * alternate file (which maybe we just don't have access to on the
       * network or something).
       *
       * But we do still need to notify the user of the error.
       */
      g_warning ("%s", error->message);
      //ide_layout_view_add_error(...);
    }

  dzl_gtk_widget_hide_with_fade (GTK_WIDGET (self->progress_bar));
}

static void
ide_editor_view_actions_save_as (GSimpleAction *action,
                                 GVariant      *param,
                                 gpointer       user_data)
{
  IdeEditorView *self = user_data;
  GtkFileChooserNative *dialog;
  IdeContext *context;
  IdeBuffer *buffer;
  GtkWidget *toplevel;
  IdeFile *file;
  GFile *gfile;
  gint ret;

  g_assert (IDE_IS_EDITOR_VIEW (self));

  buffer = ide_editor_view_get_buffer (self);
  file = ide_buffer_get_file (buffer);

  /* Just redirect to the save flow if we have a temporary
   * file currently. That way we can avoid splitting the
   * flow to handle both cases here.
   */
  if (ide_file_get_is_temporary (file))
    {
      ide_editor_view_actions_save (action, NULL, user_data);
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

  context = ide_buffer_get_context (buffer);
  gfile = ide_file_get_file (file);

  if (gfile != NULL)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), gfile, NULL);

  ret = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));

  if (ret == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) target = NULL;
      g_autoptr(IdeFile) save_as = NULL;
      g_autoptr(IdeProgress) progress = NULL;
      IdeBufferManager *buffer_manager;

      target = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      save_as = ide_file_new (context, target);
      buffer_manager = ide_context_get_buffer_manager (context);

      ide_buffer_manager_save_file_async (buffer_manager,
                                          buffer,
                                          save_as,
                                          &progress,
                                          NULL,
                                          ide_editor_view_actions_save_as_cb,
                                          g_object_ref (self));

      g_object_bind_property (progress, "fraction",
                              self->progress_bar, "fraction",
                              G_BINDING_SYNC_CREATE);

      gtk_widget_show (GTK_WIDGET (self->progress_bar));
    }

  gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (dialog));
}

static void
ide_editor_view_actions_notify_file_settings (IdeEditorView *self,
                                              GParamSpec    *pspec,
                                              IdeSourceView *source_view)
{
  IdeFileSettings *file_settings;
  GActionGroup *group;

  g_assert (IDE_IS_EDITOR_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "file-settings");
  g_assert (DZL_IS_PROPERTIES_GROUP (group));

  file_settings = ide_source_view_get_file_settings (source_view);
  g_assert (!file_settings || IDE_IS_FILE_SETTINGS (file_settings));

  g_object_set (group, "object", file_settings, NULL);
}

static const GActionEntry editor_view_entries[] = {
  { "print", ide_editor_view_actions_print },
  { "reload", ide_editor_view_actions_reload },
  { "save", ide_editor_view_actions_save },
  { "save-as", ide_editor_view_actions_save_as },
};

static const gchar *source_view_property_actions[] = {
  "auto-indent",
  "smart-backspace",
  "highlight-current-line",
  "show-line-numbers",
  "show-right-margin",
  "tab-width",
};

void
_ide_editor_view_init_actions (IdeEditorView *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;
  g_autoptr(DzlPropertiesGroup) sv_props = NULL;
  g_autoptr(DzlPropertiesGroup) file_props = NULL;
  IdeSourceView *source_view;

  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  source_view = ide_editor_view_get_view (self);

  /* Setup our user-facing actions */
  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   editor_view_entries,
                                   G_N_ELEMENTS (editor_view_entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-view", G_ACTION_GROUP (group));

  /* We want to access some settings properties as stateful GAction so they
   * manipulated using regular Gtk widgets from the properties panel.
   */
  sv_props = dzl_properties_group_new (G_OBJECT (source_view));
  for (guint i = 0; i < G_N_ELEMENTS (source_view_property_actions); i++)
    {
      const gchar *name = source_view_property_actions[i];
      dzl_properties_group_add_property (sv_props, name, name);
    }
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
                            G_CALLBACK (ide_editor_view_actions_notify_file_settings),
                            self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "file-settings", G_ACTION_GROUP (file_props));
  ide_editor_view_actions_notify_file_settings (self, NULL, source_view);
}
