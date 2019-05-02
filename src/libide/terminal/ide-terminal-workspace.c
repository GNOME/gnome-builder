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
  IdeWorkspace        parent_instance;

  IdeHeaderBar       *header_bar;
  DzlShortcutTooltip *search_shortcut;
};

G_DEFINE_TYPE (IdeTerminalWorkspace, ide_terminal_workspace, IDE_TYPE_WORKSPACE)

static void
ide_terminal_workspace_context_set_cb (GtkWidget  *widget,
                                       IdeContext *context)
{
  IdeTerminalWorkspace *self = (IdeTerminalWorkspace *)widget;

  g_assert (IDE_IS_TERMINAL_WORKSPACE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context == NULL)
    return;

  if (ide_context_has_project (context))
    {
      GtkWidget *bar = ide_omni_bar_new ();
      GtkWidget *label = gtk_label_new (NULL);

      bar = g_object_new (IDE_TYPE_OMNI_BAR,
                          "visible", TRUE,
                          NULL);
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (self->header_bar), GTK_WIDGET (bar));

      label = g_object_new (GTK_TYPE_LABEL,
                            "visible", TRUE,
                            "xalign", 0.0f,
                            NULL);
      g_object_bind_property (context, "title", label, "label", G_BINDING_SYNC_CREATE);
      ide_omni_bar_set_placeholder (IDE_OMNI_BAR (bar), label);
    }
}

static void
ide_terminal_workspace_class_init (IdeTerminalWorkspaceClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);

  ide_workspace_class_set_kind (workspace_class, "terminal");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-terminal/ui/ide-terminal-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalWorkspace, header_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeTerminalWorkspace, search_shortcut);
}

static void
ide_terminal_workspace_init (IdeTerminalWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  ide_widget_set_context_handler (self, ide_terminal_workspace_context_set_cb);
}

IdeTerminalWorkspace *
ide_terminal_workspace_new (IdeApplication *application)
{
  g_return_val_if_fail (IDE_IS_APPLICATION (application), NULL);

  return g_object_new (IDE_TYPE_TERMINAL_WORKSPACE,
                       "application", application,
                       NULL);
}
