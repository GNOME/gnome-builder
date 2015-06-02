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
#include "gb-string.h"
#include "gb-tree.h"
#include "gb-view-grid.h"
#include "gb-widget.h"
#include "gb-workbench.h"

#define SIDEBAR_POSITION 250

G_DEFINE_TYPE (GbEditorWorkspace, gb_editor_workspace, GB_TYPE_WORKSPACE)

enum {
  PROP_0,
  PROP_SHOW_PROJECT_TREE,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

gboolean
gb_editor_workspace_get_show_project_tree (GbEditorWorkspace *self)
{
  return FALSE;
}

static void
gb_editor_workspace_set_show_project_tree (GbEditorWorkspace *self,
                                           gboolean           show_project_tree)
{
}

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

      gb_project_tree_set_context (self->project_tree, context);
    }
}

static void
gb_editor_workspace__toplevel_set_focus (GbEditorWorkspace *self,
                                         GtkWidget         *focus,
                                         GbWorkbench       *workbench)
{
  GtkStyleContext *style_context;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));
  g_assert (GB_IS_WORKBENCH (workbench));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self->project_sidebar_header));
  gtk_style_context_remove_class (style_context, "focused");

  while (focus != NULL)
    {
      if (focus == GTK_WIDGET (self->project_sidebar))
        {
          gtk_style_context_add_class (style_context, "focused");
          break;
        }

      if (GTK_IS_POPOVER (focus))
        focus = gtk_popover_get_relative_to (GTK_POPOVER (focus));
      else
        focus = gtk_widget_get_parent (focus);
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
gb_editor_workspace_hierarchy_changed (GtkWidget *widget,
                                       GtkWidget *previous_toplevel)
{
  GbEditorWorkspace *self = (GbEditorWorkspace *)widget;
  GtkWidget *toplevel;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  if (GTK_IS_WINDOW (previous_toplevel))
    {
      g_signal_handlers_disconnect_by_func (previous_toplevel,
                                            G_CALLBACK (gb_editor_workspace__toplevel_set_focus),
                                            self);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      g_signal_connect_object (toplevel,
                               "set-focus",
                               G_CALLBACK (gb_editor_workspace__toplevel_set_focus),
                               self,
                               G_CONNECT_SWAPPED);
    }
}

static gboolean
save_project_tree_position_timeout (gpointer data)
{
  GbEditorWorkspace *self = data;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  self->project_tree_position_timeout = 0;
  gb_project_tree_save_desired_width (self->project_tree);

  return G_SOURCE_REMOVE;
}

static void
gb_editor_workspace__project_paned_notify_position (GbEditorWorkspace *self,
                                                    GParamSpec        *pspec,
                                                    GtkPaned          *paned)
{
  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  if (self->project_tree_position_timeout != 0)
    g_source_remove (self->project_tree_position_timeout);

  self->project_tree_position_timeout =
    g_timeout_add_seconds (1, save_project_tree_position_timeout, self);
}

static void
gb_editor_workspace_views_foreach (GbWorkspace *workspace,
                                   GtkCallback  callback,
                                   gpointer     callback_data)
{
  GbEditorWorkspace *self = (GbEditorWorkspace *)workspace;
  GList *stacks;
  GList *stack;

  g_assert (GB_IS_EDITOR_WORKSPACE (self));

  stacks = gb_view_grid_get_stacks (self->view_grid);

  for (stack = stacks; stack; stack = stack->next)
    {
      GList *views;
      GList *iter;

      views = gb_view_stack_get_views (stack->data);

      for (iter = views; iter; iter = iter->next)
        callback (iter->data, callback_data);

      g_list_free (views);
    }

  g_list_free (stacks);
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
  GbEditorWorkspace *self = (GbEditorWorkspace *)object;

  IDE_ENTRY;

  if (self->project_tree_position_timeout)
    {
      g_source_remove (self->project_tree_position_timeout);
      self->project_tree_position_timeout = 0;
    }

  G_OBJECT_CLASS (gb_editor_workspace_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gb_editor_workspace_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GbEditorWorkspace *self = GB_EDITOR_WORKSPACE(object);

  switch (prop_id)
    {
    case PROP_SHOW_PROJECT_TREE:
      g_value_set_boolean (value, gb_editor_workspace_get_show_project_tree (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_editor_workspace_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GbEditorWorkspace *self = GB_EDITOR_WORKSPACE(object);

  switch (prop_id)
    {
    case PROP_SHOW_PROJECT_TREE:
      gb_editor_workspace_set_show_project_tree (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
gb_editor_workspace_class_init (GbEditorWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GbWorkspaceClass *workspace_class = GB_WORKSPACE_CLASS (klass);

  object_class->constructed = gb_editor_workspace_constructed;
  object_class->finalize = gb_editor_workspace_finalize;
  object_class->get_property = gb_editor_workspace_get_property;
  object_class->set_property = gb_editor_workspace_set_property;

  widget_class->grab_focus = gb_editor_workspace_grab_focus;
  widget_class->hierarchy_changed = gb_editor_workspace_hierarchy_changed;

  workspace_class->views_foreach = gb_editor_workspace_views_foreach;

  gParamSpecs [PROP_SHOW_PROJECT_TREE] =
    g_param_spec_boolean ("show-project-tree",
                          _("Show Project Tree"),
                          _("Show Project Tree"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-workspace.ui");

  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, cpu_graph);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, cpu_graph_sep);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_button);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_paned);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_popover);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_sidebar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_sidebar_header);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_spinner);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, project_tree);
  GB_WIDGET_CLASS_BIND (klass, GbEditorWorkspace, view_grid);

  g_type_ensure (GB_TYPE_PROJECT_TREE);
  g_type_ensure (GB_TYPE_VIEW_GRID);
  g_type_ensure (RG_TYPE_CPU_GRAPH);
}

static void
gb_editor_workspace_init (GbEditorWorkspace *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->project_tree_settings = g_settings_new ("org.gnome.builder.project-tree");

  g_signal_connect_object (self->project_paned,
                           "notify::position",
                           G_CALLBACK (gb_editor_workspace__project_paned_notify_position),
                           self,
                           G_CONNECT_SWAPPED);

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

static gboolean
gb_editor_workspace_reveal_file_cb (gconstpointer a,
                                    gconstpointer b)
{
  GFile *file = (GFile *)a;
  GObject *object = (GObject *)b;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_OBJECT (object));

  if (IDE_IS_PROJECT_FILE (object))
    {
      IdeProjectFile *pf = (IdeProjectFile *)object;
      GFile *pf_file;

      pf_file = ide_project_file_get_file (pf);
      return g_file_equal (pf_file, file);
    }

  return FALSE;
}

void
gb_editor_workspace_reveal_file (GbEditorWorkspace *self,
                                 GFile             *file)
{
  GbTreeNode *node;

  g_return_if_fail (GB_IS_EDITOR_WORKSPACE (self));
  g_return_if_fail (G_IS_FILE (file));

  node = gb_tree_find_custom (GB_TREE (self->project_tree),
                              gb_editor_workspace_reveal_file_cb,
                              file);

  if (node != NULL)
    {
      gb_tree_expand_to_node (GB_TREE (self->project_tree), node);
      gb_tree_scroll_to_node (GB_TREE (self->project_tree), node);
      gb_tree_node_select (node);
    }
}
