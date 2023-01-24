/* ide-webkit-workspace.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-webkit-workspace"

#include "ide-webkit-workspace.h"

struct _IdeWebkitWorkspace
{
  IdeWorkspace parent_workspace;
  IdeFrame *frame;
};

G_DEFINE_FINAL_TYPE (IdeWebkitWorkspace, ide_webkit_workspace, IDE_TYPE_WORKSPACE)

static gboolean
destroy_in_idle_cb (gpointer user_data)
{
  IdeWebkitWorkspace *self = IDE_WEBKIT_WORKSPACE (user_data);
  gtk_window_destroy (GTK_WINDOW (self));
  return G_SOURCE_REMOVE;
}

static void
on_frame_empty_cb (IdeWebkitWorkspace *self,
                   GParamSpec         *pspec,
                   PanelFrame         *frame)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WEBKIT_WORKSPACE (self));
  g_assert (PANEL_IS_FRAME (frame));

  if (panel_frame_get_empty (frame) &&
      gtk_widget_get_visible (GTK_WIDGET (self)))
    {
      gtk_widget_hide (GTK_WIDGET (self));
      g_idle_add_full (G_PRIORITY_DEFAULT,
                       destroy_in_idle_cb,
                       g_object_ref (self),
                       g_object_unref);
    }

  IDE_EXIT;
}

static void
ide_webkit_workspace_add_page (IdeWorkspace  *workspace,
                               IdePage       *page,
                               PanelPosition *position)
{
  IdeWebkitWorkspace *self = (IdeWebkitWorkspace *)workspace;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_WEBKIT_WORKSPACE (self));
  g_assert (IDE_IS_PAGE (page));
  g_assert (PANEL_IS_POSITION (position));

  panel_frame_add (PANEL_FRAME (self->frame), PANEL_WIDGET (page));

  IDE_EXIT;
}

static void
ide_webkit_workspace_class_init (IdeWebkitWorkspaceClass *klass)
{
  IdeWorkspaceClass *workspace_class = IDE_WORKSPACE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  workspace_class->add_page = ide_webkit_workspace_add_page;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/webkit/ide-webkit-workspace.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeWebkitWorkspace, frame);
}

static void
ide_webkit_workspace_init (IdeWebkitWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  panel_frame_set_header (PANEL_FRAME (self->frame),
                          PANEL_FRAME_HEADER (panel_frame_tab_bar_new ()));

  g_signal_connect_object (self->frame,
                           "notify::empty",
                           G_CALLBACK (on_frame_empty_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

IdeWorkspace *
ide_webkit_workspace_new (void)
{
  return g_object_new (IDE_TYPE_WEBKIT_WORKSPACE, NULL);
}
