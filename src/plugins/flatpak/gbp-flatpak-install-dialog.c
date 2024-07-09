/* gbp-flatpak-install-dialog.c
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

#define G_LOG_DOMAIN "gbp-flatpak-install-dialog"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-threading.h>

#include "gbp-flatpak-install-dialog.h"
#include "gbp-flatpak-util.h"

struct _GbpFlatpakInstallDialog
{
  AdwDialog     parent_instance;
  GtkListStore *liststore1;
  IdeTask      *close_task;
};

G_DEFINE_FINAL_TYPE (GbpFlatpakInstallDialog, gbp_flatpak_install_dialog, ADW_TYPE_DIALOG)

static void
cancel_clicked_cb (GbpFlatpakInstallDialog *self,
                   GtkButton               *button)
{
  ide_task_return_new_error (self->close_task,
                             G_IO_ERROR,
                             G_IO_ERROR_CANCELLED,
                             "User cancelled the request");
  adw_dialog_close (ADW_DIALOG (self));
}

static void
install_clicked_cb (GbpFlatpakInstallDialog *self,
                    GtkButton               *button)
{
  ide_task_return_boolean (self->close_task, TRUE);
  adw_dialog_close (ADW_DIALOG (self));
}

GbpFlatpakInstallDialog *
gbp_flatpak_install_dialog_new (void)
{
  return g_object_new (GBP_TYPE_FLATPAK_INSTALL_DIALOG, NULL);
}

static void
gbp_flatpak_install_dialog_finalize (GObject *object)
{
  GbpFlatpakInstallDialog *self = (GbpFlatpakInstallDialog *)object;

  g_clear_object (&self->close_task);

  G_OBJECT_CLASS (gbp_flatpak_install_dialog_parent_class)->finalize (object);
}

static void
gbp_flatpak_install_dialog_class_init (GbpFlatpakInstallDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_flatpak_install_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/flatpak/gbp-flatpak-install-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpFlatpakInstallDialog, liststore1);
  gtk_widget_class_bind_template_callback (widget_class, cancel_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, install_clicked_cb);
}

static void
gbp_flatpak_install_dialog_init (GbpFlatpakInstallDialog *self)
{
 gtk_widget_init_template (GTK_WIDGET (self));
}

void
gbp_flatpak_install_dialog_run_async (GbpFlatpakInstallDialog *self,
                                      GtkWidget               *parent,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (GBP_IS_FLATPAK_INSTALL_DIALOG (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_flatpak_install_dialog_run_async);

  if (self->close_task != NULL)
    {
      ide_task_chain (self->close_task, task);
      return;
    }

  ide_task_set_release_on_propagate (task, FALSE);
  self->close_task = g_object_ref (task);

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (adw_dialog_close),
                             self,
                             G_CONNECT_SWAPPED);

  adw_dialog_present (ADW_DIALOG (self), parent);
}

gboolean
gbp_flatpak_install_dialog_run_finish (GbpFlatpakInstallDialog  *self,
                                       GAsyncResult             *result,
                                       GError                  **error)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_INSTALL_DIALOG (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gboolean
gbp_flatpak_install_dialog_contains (GbpFlatpakInstallDialog *self,
                                     const gchar             *name,
                                     const gchar             *arch,
                                     const gchar             *branch)
{
  GtkTreeIter iter;

  g_assert (GBP_IS_FLATPAK_INSTALL_DIALOG (self));
  g_assert (name != NULL);

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->liststore1), &iter))
    {
      do
        {
          g_autofree gchar *item_name = NULL;
          g_autofree gchar *item_arch = NULL;
          g_autofree gchar *item_branch = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->liststore1), &iter,
                              0, &item_name,
                              1, &item_arch,
                              2, &item_branch,
                              -1);

          if (ide_str_equal0 (item_name, name) &&
              (!arch || ide_str_equal0 (item_arch, arch)) &&
              (!branch || ide_str_equal0 (item_branch, branch)))
            return TRUE;
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->liststore1), &iter));
    }

  return FALSE;
}

void
gbp_flatpak_install_dialog_add_runtime (GbpFlatpakInstallDialog *self,
                                        const gchar             *runtime_id)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *arch = NULL;
  g_autofree gchar *branch = NULL;

  g_assert (GBP_IS_FLATPAK_INSTALL_DIALOG (self));
  g_assert (runtime_id != NULL);

  if (g_str_has_prefix (runtime_id, "flatpak:"))
    runtime_id += strlen ("flatpak:");

  if (gbp_flatpak_split_id (runtime_id, &name, &arch, &branch) &&
      !gbp_flatpak_install_dialog_contains (self, name, arch, branch))
    {
      GtkTreeIter iter;

      gtk_list_store_append (self->liststore1, &iter);
      gtk_list_store_set (self->liststore1, &iter,
                          0, name,
                          1, arch,
                          2, branch,
                          -1);
    }
}

gboolean
gbp_flatpak_install_dialog_is_empty (GbpFlatpakInstallDialog *self)
{
  g_return_val_if_fail (GBP_IS_FLATPAK_INSTALL_DIALOG (self), FALSE);

  return gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->liststore1), NULL) == 0;
}
