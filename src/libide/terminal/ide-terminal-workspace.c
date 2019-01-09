/* ide-terminal-workspace.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-terminal-workspace"

#include "config.h"

#include "ide-terminal-workspace.h"

struct _IdeTerminalWorkspace
{
  IdeWorkspace  parent_instance;

  IdeHeaderBar *header_bar;
};

G_DEFINE_TYPE (IdeTerminalWorkspace, ide_terminal_workspace, IDE_TYPE_WORKSPACE)

static void
ide_terminal_workspace_class_init (IdeTerminalWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "terminal");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalWorkspace, header_bar);
}

static void
ide_terminal_workspace_init (IdeTerminalWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
