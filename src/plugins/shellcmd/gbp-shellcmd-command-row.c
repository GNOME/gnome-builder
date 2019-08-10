/* gbp-shellcmd-command-row.c
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

#define G_LOG_DOMAIN "gbp-shellcmd-command-row"

#include "config.h"

#include "gbp-shellcmd-command-row.h"

struct _GbpShellcmdCommandRow
{
  GtkListBoxRow       parent_instance;

  gchar              *id;
  GbpShellcmdCommand *command;

  GtkLabel           *title;
  DzlShortcutLabel   *chord;
};

G_DEFINE_TYPE (GbpShellcmdCommandRow, gbp_shellcmd_command_row, GTK_TYPE_LIST_BOX_ROW)

static void
gbp_shellcmd_command_row_finalize (GObject *object)
{
  GbpShellcmdCommandRow *self = (GbpShellcmdCommandRow *)object;

  g_clear_pointer (&self->id, g_free);
  g_clear_object (&self->command);

  G_OBJECT_CLASS (gbp_shellcmd_command_row_parent_class)->finalize (object);
}

static void
gbp_shellcmd_command_row_class_init (GbpShellcmdCommandRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_shellcmd_command_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/shellcmd/gbp-shellcmd-command-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandRow, chord);
  gtk_widget_class_bind_template_child (widget_class, GbpShellcmdCommandRow, title);
}

static void
gbp_shellcmd_command_row_init (GbpShellcmdCommandRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gbp_shellcmd_command_row_new (GbpShellcmdCommand *command)
{
  GbpShellcmdCommandRow *self;

  self = g_object_new (GBP_TYPE_SHELLCMD_COMMAND_ROW,
                       "visible", TRUE,
                       NULL);
  self->id = g_strdup (gbp_shellcmd_command_get_id (command));
  g_set_object (&self->command, command);

  g_object_bind_property (command, "title", self->title, "label", G_BINDING_SYNC_CREATE);
  g_object_bind_property (command, "shortcut", self->chord, "accelerator", G_BINDING_SYNC_CREATE);

  return GTK_WIDGET (self);
}

GbpShellcmdCommand *
gbp_shellcmd_command_row_get_command (GbpShellcmdCommandRow *self)
{
  g_return_val_if_fail (GBP_IS_SHELLCMD_COMMAND_ROW (self), NULL);

  return self->command;
}
