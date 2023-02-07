/* gbp-shortcutui-dialog.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shortcutui-dialog"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-gui.h>

#include "gbp-shortcutui-action.h"
#include "gbp-shortcutui-action-model.h"
#include "gbp-shortcutui-dialog.h"
#include "gbp-shortcutui-row.h"

struct _GbpShortcutuiDialog
{
  GtkWindow            parent_instance;
  GtkSearchEntry      *search;
  GtkListBox          *results_list_box;
  AdwPreferencesGroup *overview;
  AdwPreferencesGroup *results;
  AdwPreferencesGroup *empty;
  GtkStringFilter     *string_filter;
  GtkFilterListModel  *filter_model;
  guint                update_source;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiDialog, gbp_shortcutui_dialog, GTK_TYPE_WINDOW)

static void
gbp_shortcutui_dialog_update_header_cb (GtkListBoxRow *row,
                                        GtkListBoxRow *before,
                                        gpointer       user_data)
{
  g_assert (GBP_IS_SHORTCUTUI_ROW (row));
  g_assert (!before || GBP_IS_SHORTCUTUI_ROW (before));

  gbp_shortcutui_row_update_header (GBP_SHORTCUTUI_ROW (row),
                                    GBP_SHORTCUTUI_ROW (before));
}

static gboolean
gbp_shortcutui_dialog_update_visible (gpointer user_data)
{
  GbpShortcutuiDialog *self = user_data;
  const char *text;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  self->update_source = 0;

  text = gtk_editable_get_text (GTK_EDITABLE (self->search));
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->filter_model));

  if (ide_str_empty0 (text))
    {
      gtk_widget_show (GTK_WIDGET (self->overview));
      gtk_widget_hide (GTK_WIDGET (self->results));
      gtk_widget_hide (GTK_WIDGET (self->empty));
    }
  else
    {
      gboolean has_results = n_items > 0;

      gtk_widget_hide (GTK_WIDGET (self->overview));
      gtk_widget_set_visible (GTK_WIDGET (self->results), has_results);
      gtk_widget_set_visible (GTK_WIDGET (self->empty), !has_results);
    }

  return G_SOURCE_REMOVE;
}

static void
gbp_shortcutui_dialog_queue_update (GbpShortcutuiDialog *self)
{
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  if (self->update_source == 0)
    self->update_source = g_timeout_add (250, gbp_shortcutui_dialog_update_visible, self);
}

static void
gbp_shortcutui_dialog_dispose (GObject *object)
{
  GbpShortcutuiDialog *self = (GbpShortcutuiDialog *)object;

  g_clear_handle_id (&self->update_source, g_source_remove);

  G_OBJECT_CLASS (gbp_shortcutui_dialog_parent_class)->dispose (object);
}

static void
gbp_shortcutui_dialog_group_header_cb (GtkListBoxRow *row,
                                       GtkListBoxRow *before,
                                       gpointer       user_data)
{
  const char *page = g_object_get_data (G_OBJECT (row), "PAGE");
  const char *last_page = before ? g_object_get_data (G_OBJECT (before), "PAGE") : NULL;

  if (!ide_str_equal0 (page, last_page))
    {
      GtkLabel *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "css-classes", IDE_STRV_INIT ("heading"),
                            "label", page,
                            "use-markup", TRUE,
                            "xalign", .0f,
                            NULL);
      gtk_list_box_row_set_header (GTK_LIST_BOX_ROW (row), GTK_WIDGET (label));
      gtk_widget_add_css_class (GTK_WIDGET (row), "has-header");
    }
}

static void
set_accel (GbpShortcutuiDialog *self,
           const char          *accel)
{
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  /* TODO: Set accel for dialog */
  g_printerr ("Set accel to %s\n", accel);
}

static void
shortcut_dialog_response_cb (GbpShortcutuiDialog    *self,
                             int                     response_id,
                             IdeShortcutAccelDialog *dialog)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_assert (IDE_IS_SHORTCUT_ACCEL_DIALOG (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      const char *accel;

      accel = ide_shortcut_accel_dialog_get_accelerator (dialog);
      set_accel (self, accel);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
gbp_shortcutui_dialog_row_activated_cb (GbpShortcutuiDialog *self,
                                        GbpShortcutuiRow    *row)
{
  IdeShortcutAccelDialog *dialog;
  const char *accel;
  const char *name;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_assert (GBP_IS_SHORTCUTUI_ROW (row));

  name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  accel = gbp_shortcutui_row_get_accelerator (row);
  dialog = g_object_new (IDE_TYPE_SHORTCUT_ACCEL_DIALOG,
                         "accelerator", accel,
                         "transient-for", self,
                         "modal", TRUE,
                         "shortcut-title", name,
                         "title", _("Set Shortcut"),
                         "use-header-bar", 1,
                         NULL);
  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (shortcut_dialog_response_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static GtkWidget *
gbp_shortcutui_dialog_create_row_cb (gpointer item,
                                     gpointer user_data)
{
  GbpShortcutuiAction *action = item;
  GbpShortcutuiDialog *self = user_data;
  GtkWidget *row;

  g_assert (GBP_IS_SHORTCUTUI_ACTION (action));
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  row = g_object_new (GBP_TYPE_SHORTCUTUI_ROW,
                      "activatable", TRUE,
                      "action", action,
                      NULL);
  g_signal_connect_object (row,
                           "activated",
                           G_CALLBACK (gbp_shortcutui_dialog_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  return row;
}

void
gbp_shortcutui_dialog_set_model (GbpShortcutuiDialog *self,
                                 GListModel          *model)
{
  g_autoptr(GListModel) wrapped = NULL;
  AdwExpanderRow *last_group_row = NULL;
  g_autofree char *last_page = NULL;
  g_autofree char *last_group = NULL;
  GtkListBox *list_box = NULL;
  guint n_items;

  g_return_if_fail (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_return_if_fail (G_IS_LIST_MODEL (model));

  wrapped = gbp_shortcutui_action_model_new (model);
  n_items = g_list_model_get_n_items (wrapped);

  /* Collect all our page/groups for the overview selection */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpShortcutuiAction) action = g_list_model_get_item (wrapped, i);
      g_autofree char *page = NULL;
      g_autofree char *group = NULL;

      g_object_get (action,
                    "page", &page,
                    "group", &group,
                    NULL);

      if (ide_str_equal0 (page, "ignore") || ide_str_equal0 (group, "ignore"))
        continue;

      if (!ide_str_equal0 (page, last_page) || !ide_str_equal0 (group, last_group))
        {
          AdwExpanderRow *row;

          row = g_object_new (ADW_TYPE_EXPANDER_ROW,
                              "title", group,
                              NULL);

          g_object_set_data_full (G_OBJECT (row), "PAGE", g_strdup (page), g_free);

          adw_preferences_group_add (self->overview, GTK_WIDGET (row));

          if (list_box == NULL)
            list_box = GTK_LIST_BOX (gtk_widget_get_ancestor (GTK_WIDGET (row), GTK_TYPE_LIST_BOX));

          g_set_str (&last_group, group);
          g_set_str (&last_page, page);

          last_group_row = row;
        }

      if (last_group_row != NULL)
        {
          GbpShortcutuiRow *row;

          row = g_object_new (GBP_TYPE_SHORTCUTUI_ROW,
                              "activatable", TRUE,
                              "action", action,
                              NULL);
          g_signal_connect_object (row,
                                   "activated",
                                   G_CALLBACK (gbp_shortcutui_dialog_row_activated_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
          adw_expander_row_add_row (last_group_row, GTK_WIDGET (row));
        }
    }

  if (list_box != NULL)
    gtk_list_box_set_header_func (list_box,
                                  gbp_shortcutui_dialog_group_header_cb,
                                  NULL, NULL);

  gtk_filter_list_model_set_model (self->filter_model, wrapped);
}

static void
reset_all_shortcuts (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *param)
{
  ide_shortcut_manager_reset_user ();
}

static void
edit_shortcuts (GtkWidget  *widget,
                const char *action_name,
                GVariant   *param)
{
  GbpShortcutuiDialog *self = GBP_SHORTCUTUI_DIALOG (widget);
  g_autoptr(GFile) file = NULL;
  IdeWorkbench *workbench = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  file = g_file_new_build_filename (g_get_user_config_dir (),
                                    "gnome-builder",
                                    "keybindings.json",
                                    NULL);

  /* Ensure there is a file to open */
  if (!g_file_query_exists (file, NULL))
    g_file_set_contents (g_file_peek_path (file), "", 0, NULL);

  if (TRUE)
    {
      g_autoptr(GFile) workdir = g_file_get_parent (file);
      IdeEditorWorkspace *workspace;
      IdeContext *context;

      workbench = ide_workbench_new ();
      ide_application_add_workbench (IDE_APPLICATION_DEFAULT, workbench);

      context = ide_workbench_get_context (workbench);
      ide_context_set_workdir (context, workdir);

      workspace = ide_editor_workspace_new (IDE_APPLICATION_DEFAULT);
      ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));

      gtk_window_present (GTK_WINDOW (workspace));

      ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
    }

  g_assert (IDE_IS_WORKBENCH (workbench));

  ide_workbench_open_async (workbench, file, "editorui", IDE_BUFFER_OPEN_FLAGS_NONE,
                            NULL, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_shortcutui_dialog_class_init (GbpShortcutuiDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gbp_shortcutui_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shortcutui/gbp-shortcutui-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, empty);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, filter_model);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, overview);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, search);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, string_filter);
  gtk_widget_class_bind_template_callback (widget_class, gbp_shortcutui_dialog_queue_update);

  gtk_widget_class_install_action (widget_class, "shortcuts.reset-all", NULL, reset_all_shortcuts);
  gtk_widget_class_install_action (widget_class, "shortcuts.edit", NULL, edit_shortcuts);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
}

static void
gbp_shortcutui_dialog_init (GbpShortcutuiDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->results_list_box,
                                gbp_shortcutui_dialog_update_header_cb,
                                NULL, NULL);
  gtk_list_box_bind_model (self->results_list_box,
                           G_LIST_MODEL (self->filter_model),
                           gbp_shortcutui_dialog_create_row_cb,
                           self, NULL);
}
