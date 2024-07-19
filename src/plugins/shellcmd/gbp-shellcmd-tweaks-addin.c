/* gbp-shellcmd-tweaks-addin.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-tweaks-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>

#include "gbp-shellcmd-command-dialog.h"
#include "gbp-shellcmd-command-model.h"
#include "gbp-shellcmd-tweaks-addin.h"
#include "gbp-shellcmd-run-command.h"

struct _GbpShellcmdTweaksAddin
{
  IdeTweaksAddin parent_instance;
};

static void
row_activated_cb (GbpShellcmdTweaksAddin *self,
                  AdwActionRow           *row,
                  GtkListBox             *list)
{
  g_autoptr(GbpShellcmdRunCommand) new_command = NULL;
  GbpShellcmdCommandDialog *dialog;
  GbpShellcmdRunCommand *command;
  IdeContext *context;
  GtkRoot *root;

  g_assert (GBP_IS_SHELLCMD_TWEAKS_ADDIN (self));
  g_assert (ADW_IS_ACTION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list));

  command = g_object_get_data (G_OBJECT (row), "COMMAND");
  context = g_object_get_data (G_OBJECT (row), "CONTEXT");

  g_assert (!command || GBP_IS_SHELLCMD_RUN_COMMAND (command));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (command == NULL)
    command = new_command = gbp_shellcmd_run_command_create (context);

  dialog = gbp_shellcmd_command_dialog_new (command, !!new_command);
  root = gtk_widget_get_root (GTK_WIDGET (row));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (root));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_present (GTK_WINDOW (dialog));
}

static GtkWidget *
create_creation_row_cb (GbpShellcmdTweaksAddin *self,
                        IdeTweaksWidget        *widget,
                        IdeTweaksWidget        *instance)
{
  IdeTweaksItem *root;
  AdwActionRow *row;
  GtkListBox *list;
  IdeContext *context = NULL;
  GtkLabel *caption;
  GtkImage *image;
  GtkBox *box;

  g_assert (GBP_IS_SHELLCMD_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if ((root = ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (widget))) && IDE_IS_TWEAKS (root))
    context = ide_tweaks_get_context (IDE_TWEAKS (root));

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "spacing", 12,
                      NULL);
  list = g_object_new (GTK_TYPE_LIST_BOX,
                       "css-classes", IDE_STRV_INIT ("boxed-list"),
                       "selection-mode", GTK_SELECTION_NONE,
                       NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title", _("Create Command"),
                      "subtitle", _("Commands can be used to build, run, or modify your projects"),
                      NULL);
  if (context != NULL)
    g_object_set_data_full (G_OBJECT (row),
                            "CONTEXT",
                            g_object_ref (context),
                            g_object_unref);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "go-next-symbolic",
                        NULL);
  adw_action_row_add_suffix (row, GTK_WIDGET (image));
  gtk_list_box_append (list, GTK_WIDGET (row));
  caption = g_object_new (GTK_TYPE_LABEL,
                          "css-classes", IDE_STRV_INIT ("caption", "dim-label"),
                          "wrap", TRUE,
                          "wrap-mode", PANGO_WRAP_WORD_CHAR,
                          "label", context ? _("These commands are specific to this project.")
                                           : _("These commands are shared across all projects."),
                          "xalign", .0f,
                          NULL);
  gtk_box_append (box, GTK_WIDGET (list));
  gtk_box_append (box, GTK_WIDGET (caption));

  g_signal_connect_object (list,
                           "row-activated",
                           G_CALLBACK (row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (box);
}

static GtkWidget *
create_row_cb (gpointer item,
               gpointer item_data)
{
  GbpShellcmdRunCommand *command = item;
  AdwActionRow *row;
  GtkLabel *accel;

  g_assert (GBP_IS_SHELLCMD_RUN_COMMAND (command));

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "use-markup", FALSE,
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

static GtkWidget *
create_command_list_cb (GbpShellcmdTweaksAddin *self,
                        IdeTweaksWidget        *widget,
                        IdeTweaksWidget        *instance)
{
  g_autoptr(GbpShellcmdCommandModel) model = NULL;
  IdeTweaksItem *root;
  GtkListBox *list;
  IdeContext *context;

  g_assert (GBP_IS_SHELLCMD_TWEAKS_ADDIN (self));
  g_assert (IDE_IS_TWEAKS_WIDGET (widget));
  g_assert (IDE_IS_TWEAKS_WIDGET (instance));

  if ((root = ide_tweaks_item_get_root (IDE_TWEAKS_ITEM (widget))) &&
      IDE_IS_TWEAKS (root) &&
      (context = ide_tweaks_get_context (IDE_TWEAKS (root))))
    model = gbp_shellcmd_command_model_new_for_project (context);
  else
    model = gbp_shellcmd_command_model_new_for_app ();

  list = g_object_new (GTK_TYPE_LIST_BOX,
                       "css-classes", IDE_STRV_INIT ("boxed-list"),
                       "selection-mode", GTK_SELECTION_NONE,
                       NULL);
  gtk_list_box_bind_model (list,
                           G_LIST_MODEL (model),
                           create_row_cb,
                           NULL, NULL);
  ide_gtk_widget_hide_when_empty (GTK_WIDGET (list), G_LIST_MODEL (model));
  g_signal_connect_object (list,
                           "row-activated",
                           G_CALLBACK (row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  return GTK_WIDGET (list);
}

G_DEFINE_FINAL_TYPE (GbpShellcmdTweaksAddin, gbp_shellcmd_tweaks_addin, IDE_TYPE_TWEAKS_ADDIN)

static void
gbp_shellcmd_tweaks_addin_class_init (GbpShellcmdTweaksAddinClass *klass)
{
}

static void
gbp_shellcmd_tweaks_addin_init (GbpShellcmdTweaksAddin *self)
{
  ide_tweaks_addin_set_resource_paths (IDE_TWEAKS_ADDIN (self),
                                       IDE_STRV_INIT ("/plugins/shellcmd/tweaks.ui"));
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_creation_row_cb);
  ide_tweaks_addin_bind_callback (IDE_TWEAKS_ADDIN (self), create_command_list_cb);
}
