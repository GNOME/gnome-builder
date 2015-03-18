/* gb-editor-workspace.c
 *
 * Copyright (C) 2014-2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "gb-editor-workspace"

#include <glib/gi18n.h>
#include <ide.h>

#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-editor-workspace-actions.h"
#include "gb-editor-workspace-private.h"
#include "gb-view-grid.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbEditorWorkspace, gb_editor_workspace, GB_TYPE_WORKSPACE)

static void
gb_editor_workspace__load_buffer_cb (GbEditorWorkspace *self,
                                     IdeBuffer         *buffer,
                                     IdeBufferManager  *buffer_manager)
{
  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (GB_IS_EDITOR_DOCUMENT (buffer));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  IDE_TRACE_MSG ("Loading %s.", ide_buffer_get_title (buffer));

  gb_view_grid_focus_document (self->view_grid, GB_DOCUMENT (buffer));

  IDE_EXIT;
}

static void
gb_editor_workspace_context_changed (GtkWidget  *workspace,
                                     IdeContext *context)
{
  GbEditorWorkspace *self = (GbEditorWorkspace *)workspace;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context)
    {
      IdeBufferManager *bufmgr;
      g_autoptr(GPtrArray) buffers = NULL;
      gsize i;

      bufmgr = ide_context_get_buffer_manager (context);
      g_signal_connect_object (bufmgr,
                               "load-buffer",
                               G_CALLBACK (gb_editor_workspace__load_buffer_cb),
                               self,
                               G_CONNECT_SWAPPED);

      buffers = ide_buffer_manager_get_buffers (bufmgr);

      for (i = 0; i < buffers->len; i++)
        {
          IdeBuffer *buffer = g_ptr_array_index (buffers, i);
          gb_editor_workspace__load_buffer_cb (self, buffer, bufmgr);
        }
    }
}

static void
gb_editor_workspace_grab_focus (GtkWidget *widget)
{
  GbEditorWorkspace *self = (GbEditorWorkspace *)widget;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->view_grid));
}

static void
gb_editor_workspace_constructed (GObject *object)
{
  GbEditorWorkspace *self = (GbEditorWorkspace *)object;

  IDE_ENTRY;

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->constructed (object);

  gb_editor_workspace_actions_init (self);

  IDE_EXIT;
}

static void
gb_editor_workspace_finalize (GObject *object)
{
  IDE_ENTRY;
  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);
  IDE_EXIT;
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_editor_workspace_constructed;
  object_class->finalize = gb_editor_workspace_finalize;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-workspace.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, view_grid);

  g_type_ensure (GB_TYPE_VIEW_GRID);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gb_widget_set_context_handler (self, gb_editor_workspace_context_changed);
}
