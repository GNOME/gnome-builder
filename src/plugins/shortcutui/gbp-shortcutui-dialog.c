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

#include "ide-shortcut-manager-private.h"

#include "gbp-shortcutui-dialog.h"
#include "gbp-shortcutui-model.h"
#include "gbp-shortcutui-row.h"
#include "gbp-shortcutui-shortcut.h"

struct _GbpShortcutuiDialog
{
  AdwWindow            parent_instance;

  IdeContext          *context;

  GtkSearchEntry      *search;
  GtkListBox          *results_list_box;
  AdwPreferencesGroup *overview;
  AdwPreferencesGroup *results;
  AdwPreferencesGroup *empty;
  GtkStringFilter     *string_filter;
  GtkFilterListModel  *filter_model;
  IdeUniqueListModel  *unique_model;
  GtkCustomSorter     *sorter;

  guint                update_source;
};

G_DEFINE_FINAL_TYPE (GbpShortcutuiDialog, gbp_shortcutui_dialog, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

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
  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->unique_model));

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
shortcut_dialog_shortcut_set_cb (GbpShortcutuiDialog    *self,
                                 const char             *accel,
                                 IdeShortcutAccelDialog *dialog)
{
  GbpShortcutuiShortcut *shortcut;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_assert (IDE_IS_SHORTCUT_ACCEL_DIALOG (dialog));

  shortcut = g_object_get_data (G_OBJECT (dialog), "GBP_SHORTCUTUI_SHORTCUT");

  g_assert (GBP_IS_SHORTCUTUI_SHORTCUT (shortcut));

  if (!gbp_shortcutui_shortcut_override (shortcut, accel, &error))
    ide_object_warning (self->context,
                        "Failed to override keyboard shortcut: %s",
                        error->message);

  IDE_EXIT;
}

static void
gbp_shortcutui_dialog_row_activated_cb (GbpShortcutuiDialog *self,
                                        GbpShortcutuiRow    *row)
{
  IdeShortcutAccelDialog *dialog;
  GbpShortcutuiShortcut *shortcut;
  g_autofree char *accel = NULL;
  const char *name;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));
  g_assert (GBP_IS_SHORTCUTUI_ROW (row));

  name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  shortcut = gbp_shortcutui_row_get_shortcut (row);
  accel = gbp_shortcutui_shortcut_dup_accelerator (shortcut);
  name = gbp_shortcutui_shortcut_get_title (shortcut);

  dialog = g_object_new (IDE_TYPE_SHORTCUT_ACCEL_DIALOG,
                         "accelerator", accel,
                         "transient-for", self,
                         "modal", TRUE,
                         "shortcut-title", name,
                         "title", _("Set Shortcut"),
                         NULL);
  g_signal_connect_object (dialog,
                           "shortcut-set",
                           G_CALLBACK (shortcut_dialog_shortcut_set_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_object_set_data_full (G_OBJECT (dialog),
                          "GBP_SHORTCUTUI_SHORTCUT",
                          g_object_ref (shortcut),
                          g_object_unref);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
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

static GtkWidget *
gbp_shortcutui_dialog_create_row_cb (gpointer item,
                                     gpointer user_data)
{
  GbpShortcutuiShortcut *shortcut = item;
  GbpShortcutuiDialog *self = user_data;
  GtkWidget *row;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_SHORTCUT (shortcut));
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  row = g_object_new (GBP_TYPE_SHORTCUTUI_ROW,
                      "activatable", TRUE,
                      "shortcut", shortcut,
                      NULL);
  g_signal_connect_object (row,
                           "activated",
                           G_CALLBACK (gbp_shortcutui_dialog_row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  return row;
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
gbp_shortcutui_dialog_constructed (GObject *object)
{
  GbpShortcutuiDialog *self = (GbpShortcutuiDialog *)object;
  g_autoptr(GbpShortcutuiModel) model = NULL;
  AdwExpanderRow *last_group_row = NULL;
  g_autofree char *last_page = NULL;
  g_autofree char *last_group = NULL;
  GtkListBox *list_box = NULL;
  guint n_items;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SHORTCUTUI_DIALOG (self));

  G_OBJECT_CLASS (gbp_shortcutui_dialog_parent_class)->constructed (object);

  g_return_if_fail (IDE_IS_CONTEXT (self->context));

  model = gbp_shortcutui_model_new (self->context);

  ide_unique_list_model_set_incremental (self->unique_model, FALSE);
  ide_unique_list_model_set_model (self->unique_model, G_LIST_MODEL (model));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->unique_model));

  /* Collect all our page/groups for the overview selection */
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GbpShortcutuiShortcut) shortcut = g_list_model_get_item (G_LIST_MODEL (self->unique_model), i);
      const char *page = gbp_shortcutui_shortcut_get_page (shortcut);
      const char *group = gbp_shortcutui_shortcut_get_group (shortcut);

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
          GbpShortcutuiRow *row = gbp_shortcutui_row_new (shortcut);
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
}

static void
gbp_shortcutui_dialog_dispose (GObject *object)
{
  GbpShortcutuiDialog *self = (GbpShortcutuiDialog *)object;

  g_clear_handle_id (&self->update_source, g_source_remove);
  g_clear_object (&self->context);

  G_OBJECT_CLASS (gbp_shortcutui_dialog_parent_class)->dispose (object);
}

static void
gbp_shortcutui_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GbpShortcutuiDialog *self = GBP_SHORTCUTUI_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbpShortcutuiDialog *self = GBP_SHORTCUTUI_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->context = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_shortcutui_dialog_class_init (GbpShortcutuiDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gbp_shortcutui_dialog_constructed;
  object_class->dispose = gbp_shortcutui_dialog_dispose;
  object_class->get_property = gbp_shortcutui_dialog_get_property;
  object_class->set_property = gbp_shortcutui_dialog_set_property;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context", NULL, NULL,
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shortcutui/gbp-shortcutui-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, empty);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, filter_model);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, overview);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, results_list_box);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, search);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, unique_model);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, sorter);
  gtk_widget_class_bind_template_child (widget_class, GbpShortcutuiDialog, string_filter);
  gtk_widget_class_bind_template_callback (widget_class, gbp_shortcutui_dialog_queue_update);

  gtk_widget_class_install_action (widget_class, "shortcuts.reset-all", NULL, reset_all_shortcuts);
  gtk_widget_class_install_action (widget_class, "shortcuts.edit", NULL, edit_shortcuts);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  g_type_ensure (GBP_TYPE_SHORTCUTUI_MODEL);
  g_type_ensure (GBP_TYPE_SHORTCUTUI_ROW);
  g_type_ensure (GBP_TYPE_SHORTCUTUI_SHORTCUT);
}

static void
gbp_shortcutui_dialog_init (GbpShortcutuiDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef DEVELOPMENT_BUILD
  gtk_widget_add_css_class (GTK_WIDGET (self), "devel");
#endif

  gtk_custom_sorter_set_sort_func (self->sorter,
                                   (GCompareDataFunc)gbp_shortcutui_shortcut_compare,
                                   NULL, NULL);
  gtk_list_box_set_header_func (self->results_list_box,
                                gbp_shortcutui_dialog_update_header_cb,
                                NULL, NULL);
  gtk_list_box_bind_model (self->results_list_box,
                           G_LIST_MODEL (self->filter_model),
                           gbp_shortcutui_dialog_create_row_cb,
                           self, NULL);
}
