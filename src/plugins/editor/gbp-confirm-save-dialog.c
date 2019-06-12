/* gbp-confirm-save-dialog.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-confirm-save-dialog"

#include "config.h"

#include <glib/gi18n.h>

#include "gbp-confirm-save-dialog.h"

struct _GbpConfirmSaveDialog
{
  GtkMessageDialog       parent_instance;

  IdeTask               *task;

  /* Template Widgets */
  GtkTreeView           *tree_view;
  GtkListStore          *model;
  GtkCellRendererToggle *toggle;
};

G_DEFINE_TYPE (GbpConfirmSaveDialog, gbp_confirm_save_dialog, GTK_TYPE_MESSAGE_DIALOG)

enum {
  COLUMN_SELECTED,
  COLUMN_BUFFER,
  COLUMN_TITLE,
};

static void
gbp_confirm_save_dialog_save_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  guint *count;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_buffer_save_file_finish (buffer, result, &error))
    g_warning ("Failed to save buffer: %s", error->message);

  count = ide_task_get_task_data (task);
  if (--(*count) == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_confirm_save_dialog_save (GbpConfirmSaveDialog *self,
                              IdeTask              *task)
{
  GtkTreeIter iter;
  guint *count;

  g_assert (GBP_IS_CONFIRM_SAVE_DIALOG (self));
  g_assert (IDE_IS_TASK (task));

  count = ide_task_get_task_data (task);

  g_assert (count != NULL);
  g_assert (*count == 0);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->model), &iter))
    {
      do
        {
          g_autoptr(IdeBuffer) buffer = NULL;
          gboolean selected = FALSE;

          gtk_tree_model_get (GTK_TREE_MODEL (self->model), &iter,
                              COLUMN_SELECTED, &selected,
                              COLUMN_BUFFER, &buffer,
                              -1);

          if (selected)
            {
              (*count)++;

              ide_buffer_save_file_async (buffer,
                                          NULL,
                                          ide_task_get_cancellable (task),
                                          NULL,
                                          gbp_confirm_save_dialog_save_cb,
                                          g_object_ref (task));
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->model), &iter));
    }

  if (*count == 0)
    ide_task_return_boolean (task, TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
}

static void
gbp_confirm_save_dialog_response (GtkDialog *dialog,
                                  gint       response_id)
{
  GbpConfirmSaveDialog *self = (GbpConfirmSaveDialog *)dialog;

  g_assert (GBP_IS_CONFIRM_SAVE_DIALOG (self));

  if (self->task == NULL)
    return;

  switch (response_id)
    {
    case GTK_RESPONSE_ACCEPT:
      gbp_confirm_save_dialog_save (self, self->task);
      break;

    case GTK_RESPONSE_CLOSE:
      ide_task_return_boolean (self->task, TRUE);
      break;

    case GTK_RESPONSE_CANCEL:
    default:
      ide_task_return_new_error (self->task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_CANCELLED,
                                 "The dialog was closed");
      break;
    }

  g_clear_object (&self->task);
}

static void
gbp_confirm_save_dialog_row_activated_cb (GbpConfirmSaveDialog *self,
                                          GtkTreePath          *tree_path,
                                          GtkTreeViewColumn    *column,
                                          GtkTreeView          *tree_view)
{
  GtkTreeIter iter;

  g_assert (GBP_IS_CONFIRM_SAVE_DIALOG (self));
  g_assert (tree_path != NULL);
  g_assert (GTK_IS_TREE_VIEW (tree_view));

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->model), &iter, tree_path))
    {
      gboolean selected;

      gtk_tree_model_get (GTK_TREE_MODEL (self->model), &iter,
                          COLUMN_SELECTED, &selected,
                          -1);
      gtk_list_store_set (self->model, &iter,
                          COLUMN_SELECTED, !selected,
                          -1);
    }
}

static void
gbp_confirm_save_dialog_class_init (GbpConfirmSaveDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  dialog_class->response = gbp_confirm_save_dialog_response;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/editor/gbp-confirm-save-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpConfirmSaveDialog, model);
  gtk_widget_class_bind_template_child (widget_class, GbpConfirmSaveDialog, toggle);
  gtk_widget_class_bind_template_child (widget_class, GbpConfirmSaveDialog, tree_view);
}

static void
gbp_confirm_save_dialog_init (GbpConfirmSaveDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_dialog_add_buttons (GTK_DIALOG (self),
                          _("Close without Saving"), GTK_RESPONSE_CLOSE,
                          _("Cancel"), GTK_RESPONSE_CANCEL,
                          _("Save"), GTK_RESPONSE_ACCEPT,
                          NULL);

  g_signal_connect_object (self->tree_view,
                           "row-activated",
                           G_CALLBACK (gbp_confirm_save_dialog_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

GbpConfirmSaveDialog *
gbp_confirm_save_dialog_new (GtkWindow *transient_for)
{
  return g_object_new (GBP_TYPE_CONFIRM_SAVE_DIALOG,
                       "modal", TRUE,
                       "transient-for", transient_for,
                       NULL);
}

void
gbp_confirm_save_dialog_add_buffer (GbpConfirmSaveDialog *self,
                                    IdeBuffer            *buffer)
{
  g_autofree gchar *title = NULL;
  GtkTreeIter iter;

  g_return_if_fail (GBP_IS_CONFIRM_SAVE_DIALOG (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  title = ide_buffer_dup_title (buffer);

  gtk_list_store_append (self->model, &iter);
  gtk_list_store_set (self->model, &iter,
                      COLUMN_SELECTED, TRUE,
                      COLUMN_BUFFER, buffer,
                      COLUMN_TITLE, title,
                      -1);
}

void
gbp_confirm_save_dialog_run_async (GbpConfirmSaveDialog *self,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_autofree gchar *title = NULL;
  const gchar *format;
  GtkWindow *transient_for;
  guint count;

  g_return_if_fail (GBP_IS_CONFIRM_SAVE_DIALOG (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->task == NULL);

  self->task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (self->task, gbp_confirm_save_dialog_run_async);
  ide_task_set_task_data (self->task, g_new0 (guint, 1), g_free);

  count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->model), NULL);

  format = g_dngettext (GETTEXT_PACKAGE,
                        "There is a file with unsaved changes. Save changes before closing?",
                        "There are files with unsaved changes. Save changes before closing?",
                        count);

  title = g_strdup_printf ("<span size='larger' weight='bold'>%s</span>", format);

  g_object_set (self,
                "use-markup", TRUE,
                "text", title,
                NULL);

  transient_for = gtk_window_get_transient_for (GTK_WINDOW (self));

  /* It's likely the last workspace was hidden when trying to delete-event
   * the window, and we need to make sure it is visible for our dialog.
   */
  if (!gtk_widget_get_visible (GTK_WIDGET (transient_for)))
    gtk_window_present (transient_for);

  gtk_window_present (GTK_WINDOW (self));
}

gboolean
gbp_confirm_save_dialog_run_finish (GbpConfirmSaveDialog  *self,
                                    GAsyncResult          *result,
                                    GError               **error)
{
  g_return_val_if_fail (GBP_IS_CONFIRM_SAVE_DIALOG (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
