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

#include "gb-devhelp-document.h"
#include "gb-editor-document.h"
#include "gb-editor-workspace.h"
#include "gb-editor-workspace-actions.h"
#include "gb-editor-workspace-private.h"
#include "gb-project-tree-builder.h"
#include "gb-project-tree-actions.h"
#include "gb-string.h"
#include "gb-tree.h"
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
gb_editor_workspace__notify_focus_buffer_cb (GbEditorWorkspace *self,
                                             GParamSpec        *pspec,
                                             IdeBufferManager  *buffer_manager)
{
  IdeBuffer *buffer;

  IDE_ENTRY;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));

  buffer = ide_buffer_manager_get_focus_buffer (buffer_manager);

  if (buffer != NULL)
    {
      IDE_TRACE_MSG ("Focusing %s.", ide_buffer_get_title (buffer));
      gb_view_grid_focus_document (self->view_grid, GB_DOCUMENT (buffer));
    }

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
      IdeProject *project;
      GbWorkbench *workbench;
      GbTreeNode *root;
      g_autoptr(GPtrArray) buffers = NULL;
      gsize i;

      workbench = gb_widget_get_workbench (GTK_WIDGET (self));
      g_object_bind_property (workbench, "building", self->project_spinner, "active",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (workbench, "building", self->project_spinner, "visible",
                              G_BINDING_SYNC_CREATE);

      bufmgr = ide_context_get_buffer_manager (context);
      g_signal_connect_object (bufmgr,
                               "load-buffer",
                               G_CALLBACK (gb_editor_workspace__load_buffer_cb),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (bufmgr,
                               "notify::focus-buffer",
                               G_CALLBACK (gb_editor_workspace__notify_focus_buffer_cb),
                               self,
                               G_CONNECT_SWAPPED);

      buffers = ide_buffer_manager_get_buffers (bufmgr);

      for (i = 0; i < buffers->len; i++)
        {
          IdeBuffer *buffer = g_ptr_array_index (buffers, i);
          gb_editor_workspace__load_buffer_cb (self, buffer, bufmgr);
        }

      project = ide_context_get_project (context);
      g_object_bind_property (project, "name", self->project_button, "label",
                              G_BINDING_SYNC_CREATE);

      root = gb_tree_get_root (self->project_tree);
      gb_tree_node_set_item (root, G_OBJECT (context));

      gb_project_tree_builder_set_context (GB_PROJECT_TREE_BUILDER (self->project_tree_builder),
                                           context);
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
  gb_project_tree_actions_init (self);

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

  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_sidebar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_spinner);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_tree);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, view_grid);

  g_type_ensure (GB_TYPE_TREE);
  g_type_ensure (GB_TYPE_VIEW_GRID);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->project_tree_builder = gb_project_tree_builder_new (NULL);
  gb_tree_add_builder (self->project_tree, GB_TREE_BUILDER (self->project_tree_builder));
  gb_tree_set_root (self->project_tree, gb_tree_node_new ());

  gb_widget_set_context_handler (self, gb_editor_workspace_context_changed);
}

void
gb_editor_workspace_show_help (GbEditorWorkspace *self,
                               const gchar       *uri)
{
  GbDocument *document;
  GtkWidget *last_focus;
  GtkWidget *after;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (self));
  g_return_if_fail (uri);

  document = gb_view_grid_find_document_typed (self->view_grid, GB_TYPE_DEVHELP_DOCUMENT);

  if (document != NULL)
    {
      g_object_set (document, "uri", uri, NULL);
      gb_view_grid_focus_document (self->view_grid, document);
      return;
    }

  document = g_object_new (GB_TYPE_DEVHELP_DOCUMENT,
                           "uri", uri,
                           NULL);

  last_focus = gb_view_grid_get_last_focus (self->view_grid);

  if (last_focus == NULL)
    {
      gb_view_grid_focus_document (self->view_grid, document);
      return;
    }

  after = gb_view_grid_get_stack_after (self->view_grid, GB_VIEW_STACK (last_focus));

  if (after == NULL)
    after = gb_view_grid_add_stack_after (self->view_grid, GB_VIEW_STACK (last_focus));

  gb_view_stack_focus_document (GB_VIEW_STACK (after), document);

  g_clear_object (&document);
}

void
gb_editor_workspace_search_help (GbEditorWorkspace *self,
                                 const gchar       *keywords)
{
  GbDocument *document;
  GtkWidget *last_focus;
  GtkWidget *after;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (self));

  if (gb_str_empty0 (keywords))
    return;

  document = gb_view_grid_find_document_typed (self->view_grid, GB_TYPE_DEVHELP_DOCUMENT);

  if (document != NULL)
    {
      gb_devhelp_document_set_search (GB_DEVHELP_DOCUMENT (document), keywords);
      gb_view_grid_focus_document (self->view_grid, document);
      return;
    }

  document = g_object_new (GB_TYPE_DEVHELP_DOCUMENT,
                           NULL);
  gb_devhelp_document_set_search (GB_DEVHELP_DOCUMENT (document), keywords);

  last_focus = gb_view_grid_get_last_focus (self->view_grid);

  if (last_focus == NULL)
    {
      gb_view_grid_focus_document (self->view_grid, document);
      return;
    }

  after = gb_view_grid_get_stack_after (self->view_grid, GB_VIEW_STACK (last_focus));

  if (after == NULL)
    after = gb_view_grid_add_stack_after (self->view_grid, GB_VIEW_STACK (last_focus));

  gb_view_stack_focus_document (GB_VIEW_STACK (after), document);

  g_clear_object (&document);
}
