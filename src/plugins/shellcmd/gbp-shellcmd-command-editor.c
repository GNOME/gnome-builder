/* gbp-shellcmd-command-editor.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-editor"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libide-gui.h>

#include "gbp-shellcmd-command-editor.h"

struct _GbpShellcmdCommandEditor
{
  GtkBin                parent_instance;

  DzlBindingGroup      *bindings;

  IdeEnvironmentEditor *environment;
  DzlShortcutLabel     *shortcut;
  GtkEntry             *title;
  GtkEntry             *command;
  GtkButton            *change;
};

G_DEFINE_TYPE (GbpShellcmdCommandEditor, gbp_shellcmd_command_editor, GTK_TYPE_BIN)

static void
on_dialog_response_cb (GbpShellcmdCommandEditor *self,
                       gint                      response,
                       DzlShortcutAccelDialog   *dialog)
{
  g_assert (GBP_IS_SHELLCMD_COMMAND_EDITOR (self));
  g_assert (DZL_IS_SHORTCUT_ACCEL_DIALOG (dialog));


  if (response == GTK_RESPONSE_ACCEPT)
    {
      GbpShellcmdCommand *command = GBP_SHELLCMD_COMMAND (dzl_binding_group_get_source (self->bindings));

      if (command != NULL)
        {
          g_autofree gchar *accel = dzl_shortcut_accel_dialog_get_accelerator (dialog);
          gbp_shellcmd_command_set_shortcut (command, accel);
        }
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_chagne_shortcut_cb (GbpShellcmdCommandEditor *self,
                       GtkButton                *button)
{
  GbpShellcmdCommand *command;
  g_autofree gchar *title = NULL;
  GtkWidget *dialog;

  g_assert (GBP_IS_SHELLCMD_COMMAND_EDITOR (self));
  g_assert (GTK_IS_BUTTON (button));

  command = GBP_SHELLCMD_COMMAND (dzl_binding_group_get_source (self->bindings));

  if (command == NULL)
    return;

  title = ide_command_get_title (IDE_COMMAND (command));

  dialog = g_object_new (DZL_TYPE_SHORTCUT_ACCEL_DIALOG,
                         "modal", TRUE,
                         "shortcut-title", title,
                         "title", _("Change Shortcut"),
                         "transient-for", gtk_widget_get_toplevel (GTK_WIDGET (self)),
                         "use-header-bar", TRUE,
                         NULL);

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_dialog_response_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
gbp_shellcmd_command_editor_destroy (GtkWidget *widget)
{
  GbpShellcmdCommandEditor *self = (GbpShellcmdCommandEditor *)widget;

  if (self->bindings != NULL)
    {
      dzl_binding_group_set_source (self->bindings, NULL);
      g_clear_object (&self->bindings);
    }

  GTK_WIDGET_CLASS (gbp_shellcmd_command_editor_parent_class)->destroy (widget);
}

static void
gbp_shellcmd_command_editor_class_init (GbpShellcmdCommandEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = gbp_shellcmd_command_editor_destroy;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-command-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, change);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, command);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, environment);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, shortcut);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, title);

  g_type_ensure (IDE_TYPE_ENVIRONMENT_EDITOR);
}

static void
gbp_shellcmd_command_editor_init (GbpShellcmdCommandEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->bindings, "title", self->title, "text", G_BINDING_BIDIRECTIONAL);
  dzl_binding_group_bind (self->bindings, "command", self->command, "text", G_BINDING_BIDIRECTIONAL);
  dzl_binding_group_bind (self->bindings, "shortcut", self->shortcut, "accelerator", G_BINDING_BIDIRECTIONAL);
  dzl_binding_group_bind (self->bindings, "environment", self->environment, "environment", G_BINDING_SYNC_CREATE);

  g_signal_connect_object (self->change,
                           "clicked",
                           G_CALLBACK (on_chagne_shortcut_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

void
gbp_shellcmd_command_editor_set_command (GbpShellcmdCommandEditor *self,
                                         GbpShellcmdCommand       *command)
{
  g_return_if_fail (GBP_IS_SHELLCMD_COMMAND_EDITOR (self));
  g_return_if_fail (!command || GBP_IS_SHELLCMD_COMMAND (command));

  dzl_binding_group_set_source (self->bindings, command);

  if (command != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (self->title));
}

GtkWidget *
gbp_shellcmd_command_editor_new (void)
{
  return g_object_new (GBP_TYPE_SHELLCMD_COMMAND_EDITOR, NULL);
}
