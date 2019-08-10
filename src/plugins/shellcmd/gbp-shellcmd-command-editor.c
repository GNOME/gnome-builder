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
};

G_DEFINE_TYPE (GbpShellcmdCommandEditor, gbp_shellcmd_command_editor, GTK_TYPE_BIN)

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
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, title);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, command);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, shortcut);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandEditor, environment);

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
