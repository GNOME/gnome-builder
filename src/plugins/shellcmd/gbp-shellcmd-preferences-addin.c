/* gbp-shellcmd-preferences-addin.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-preferences-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-shellcmd-command-dialog.h"
#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-preferences-addin.h"
#include "gbp-shellcmd-run-command.h"

struct _GbpShellcmdPreferencesAddin
{
  GObject               parent_instance;
  IdePreferencesWindow *window;
  GSettings            *settings;
};

static GtkWidget *
gbp_shellcmd_preferences_addin_create_row_cb (gpointer item,
                                              gpointer item_data)
{
  GbpShellcmdRunCommand *command = item;
  AdwActionRow *row;
  GtkLabel *accel;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (command));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      NULL);
  g_object_bind_property (command, "display-name", row, "title",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (command, "subtitle", row, "subtitle",
                          G_BINDING_SYNC_CREATE);

  accel = g_object_new (GTK_TYPE_LABEL,
                        "margin-start", 6,
                        "margin-end", 6,
                        NULL);
  g_object_bind_property (command, "accelerator-label", accel, "label",
                          G_BINDING_SYNC_CREATE);
  adw_action_row_add_suffix (row, GTK_WIDGET (accel));
  adw_action_row_add_suffix (row,
                             g_object_new (GTK_TYPE_IMAGE,
                                           "icon-name", "go-next-symbolic",
                                           NULL));
  g_object_set_data_full (G_OBJECT (row),
                          "COMMAND",
                          g_object_ref (command),
                          g_object_unref);

  return GTK_WIDGET (row);
}

static void
on_row_activated_cb (GtkListBox           *list_box,
                     AdwActionRow         *row,
                     IdePreferencesWindow *window)
{
  g_autoptr(GbpShellcmdRunCommand) new_command = NULL;
  GbpShellcmdRunCommand *command;
  GbpShellcmdCommandDialog *dialog;

  IDE_ENTRY;

  g_assert (GTK_IS_LIST_BOX (list_box));
  g_assert (ADW_IS_ACTION_ROW (row));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  command = g_object_get_data (G_OBJECT (row), "COMMAND");

  g_assert (!command || GBP_IS_SHELLCMD_RUN_COMMAND (command));

  if (command == NULL)
    {
      IdePreferencesMode mode = ide_preferences_window_get_mode (window);
      IdeContext *context = ide_preferences_window_get_context (window);

      if (mode == IDE_PREFERENCES_MODE_PROJECT)
        new_command = gbp_shellcmd_run_command_create (context);
      else
        new_command = gbp_shellcmd_run_command_create (NULL);

      command = new_command;
    }

  dialog = gbp_shellcmd_command_dialog_new (command, new_command != NULL);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_present (GTK_WINDOW (dialog));

  IDE_EXIT;
}

static void
handle_shellcmd_list (const char                   *page_name,
                      const IdePreferenceItemEntry *entry,
                      AdwPreferencesGroup          *group,
                      gpointer                      user_data)
{
  IdePreferencesWindow *window = user_data;
  GbpShellcmdCommandModel *model;
  IdePreferencesMode mode;
  AdwActionRow *create_row;
  IdeContext *context;
  GtkListBox *list_box;
  GtkLabel *label;

  IDE_ENTRY;

  g_assert (ide_str_equal0 (page_name, "commands"));
  g_assert (ADW_IS_PREFERENCES_GROUP (group));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  context = ide_preferences_window_get_context (window);
  mode = ide_preferences_window_get_mode (window);

  if (mode == IDE_PREFERENCES_MODE_PROJECT)
    {
      model = gbp_shellcmd_command_model_new_for_project (context);
      adw_preferences_group_set_title (group, _("Project Commands"));
    }
  else
    {
      model = gbp_shellcmd_command_model_new_for_app ();
      adw_preferences_group_set_title (group, _("Shared Commands"));
    }

  list_box = g_object_new (GTK_TYPE_LIST_BOX,
                           "css-classes", IDE_STRV_INIT ("boxed-list"),
                           "selection-mode", GTK_SELECTION_NONE,
                           NULL);
  create_row = g_object_new (ADW_TYPE_ACTION_ROW,
                             "activatable", TRUE,
                             "title", _("Create Command"),
                             "subtitle", _("Commands can be used to build, run, or modify your projects"),
                             NULL);
  adw_action_row_add_suffix (create_row,
                             g_object_new (GTK_TYPE_IMAGE,
                                           "icon-name", "go-next-symbolic",
                                           NULL));
  g_signal_connect_object (list_box,
                           "row-activated",
                           G_CALLBACK (on_row_activated_cb),
                           window,
                           0);
  gtk_list_box_append (list_box, GTK_WIDGET (create_row));
  adw_preferences_group_add (group, GTK_WIDGET (list_box));

  label = g_object_new (GTK_TYPE_LABEL,
                        "css-classes", IDE_STRV_INIT ("dim-label", "caption"),
                        "margin-top", 6,
                        "xalign", .0f,
                        NULL);
  if (mode == IDE_PREFERENCES_MODE_PROJECT)
    gtk_label_set_label (label,
                         _("These commands may be run from this project only."));
  else
    gtk_label_set_label (label,
                         _("These commands may be shared across any project in Builder."));
  adw_preferences_group_add (group, GTK_WIDGET (label));

  list_box = g_object_new (GTK_TYPE_LIST_BOX,
                           "css-classes", IDE_STRV_INIT ("boxed-list"),
                           "selection-mode", GTK_SELECTION_NONE,
                           "margin-top", 18,
                           NULL);
  gtk_list_box_bind_model (list_box,
                           G_LIST_MODEL (model),
                           gbp_shellcmd_preferences_addin_create_row_cb,
                           NULL, NULL);
  adw_preferences_group_add (group, GTK_WIDGET (list_box));
  ide_gtk_widget_hide_when_empty (GTK_WIDGET (list_box), G_LIST_MODEL (model));
  g_signal_connect_object (list_box,
                           "row-activated",
                           G_CALLBACK (on_row_activated_cb),
                           window,
                           0);


  IDE_EXIT;
}

static const IdePreferenceGroupEntry groups[] = {
  { "commands", "shellcmd", 0 },
};

static const IdePreferenceItemEntry items[] = {
  { "commands", "shellcmd", "list", 0, handle_shellcmd_list },
};

static void
gbp_shellcmd_preferences_addin_load (IdePreferencesAddin  *addin,
                                     IdePreferencesWindow *window,
                                     IdeContext           *context)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  self->window = window;

  ide_preferences_window_add_groups (window, groups, G_N_ELEMENTS (groups), GETTEXT_PACKAGE);
  ide_preferences_window_add_items (window, items, G_N_ELEMENTS (items), window, NULL);

  IDE_EXIT;
}

static void
gbp_shellcmd_preferences_addin_unload (IdePreferencesAddin  *addin,
                                       IdePreferencesWindow *window,
                                       IdeContext           *context)
{
  GbpShellcmdPreferencesAddin *self = (GbpShellcmdPreferencesAddin *)addin;

  IDE_ENTRY;

  g_assert (GBP_IS_SHELLCMD_PREFERENCES_ADDIN (self));
  g_assert (IDE_IS_PREFERENCES_WINDOW (window));

  self->window = NULL;

  IDE_EXIT;
}

static void
preferences_addin_iface_init (IdePreferencesAddinInterface *iface)
{
  iface->load = gbp_shellcmd_preferences_addin_load;
  iface->unload = gbp_shellcmd_preferences_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpShellcmdPreferencesAddin, gbp_shellcmd_preferences_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_PREFERENCES_ADDIN, preferences_addin_iface_init))

static void
gbp_shellcmd_preferences_addin_class_init (GbpShellcmdPreferencesAddinClass *klass)
{
}

static void
gbp_shellcmd_preferences_addin_init (GbpShellcmdPreferencesAddin *self)
{
}
