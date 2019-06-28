/* gbp-vcsui-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-vcsui-tree-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <libide-tree.h>
#include <libide-vcs.h>

#include "gbp-vcsui-tree-addin.h"

struct _GbpVcsuiTreeAddin
{
  GObject        parent_instance;

  IdeTree       *tree;
  IdeTreeModel  *model;
  IdeVcs        *vcs;
  IdeVcsMonitor *monitor;

  GdkRGBA        added_color;
  GdkRGBA        changed_color;
};

static void
get_foreground_for_class (GtkStyleContext   *style_context,
                          const gchar       *name,
                          GdkRGBA           *rgba)
{
  GtkStateFlags state;

  g_assert (GTK_IS_STYLE_CONTEXT (style_context));
  g_assert (name != NULL);
  g_assert (rgba != NULL);

  state = gtk_style_context_get_state (style_context);
  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, name);
  gtk_style_context_get_color (style_context, state, rgba);
  gtk_style_context_restore (style_context);
}

static void
on_tree_style_changed_cb (GbpVcsuiTreeAddin *self,
                          GtkStyleContext   *context)
{
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (GTK_IS_STYLE_CONTEXT (context));

  get_foreground_for_class (context, "vcs-added", &self->added_color);
  get_foreground_for_class (context, "vcs-changed", &self->changed_color);
}

static void
gbp_vcsui_tree_addin_switch_branch_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_vcs_switch_branch_finish (vcs, result, &error))
    g_warning ("%s", error->message);

  /* TODO: Force reload of files node */
}

static void
gbp_vcsui_tree_addin_switch_branch (GSimpleAction *action,
                                    GVariant      *param,
                                    gpointer       user_data)
{
  GbpVcsuiTreeAddin *self = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeBuildManager *build_manager;
  IdeVcsBranch *branch;
  IdeTreeNode *node;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));

  if (self->vcs == NULL ||
      !(node = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (node, IDE_TYPE_VCS_BRANCH))
    return;

  branch = ide_tree_node_get_item (node);

  context = ide_object_ref_context (IDE_OBJECT (self->vcs));

  /* Cancel any in-flight builds */
  build_manager = ide_build_manager_from_context (context);
  ide_build_manager_cancel (build_manager);

  ide_vcs_switch_branch_async (self->vcs,
                               branch,
                               NULL,
                               gbp_vcsui_tree_addin_switch_branch_cb,
                               g_object_ref (self));
}

static void
gbp_vcsui_tree_addin_push_branch_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_vcs_push_branch_finish (vcs, result, &error))
    ide_object_warning (vcs, "%s", error->message);
}

static void
gbp_vcsui_tree_addin_push_branch (GSimpleAction *action,
                                  GVariant      *param,
                                  gpointer       user_data)
{
  GbpVcsuiTreeAddin *self = user_data;
  g_autoptr(IdeContext) context = NULL;
  IdeVcsBranch *branch;
  IdeTreeNode *node;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));

  if (self->vcs == NULL ||
      !(node = ide_tree_get_selected_node (self->tree)) ||
      !ide_tree_node_holds (node, IDE_TYPE_VCS_BRANCH))
    return;

  branch = ide_tree_node_get_item (node);
  context = ide_object_ref_context (IDE_OBJECT (self->vcs));

  ide_vcs_push_branch_async (self->vcs,
                             branch,
                             NULL,
                             gbp_vcsui_tree_addin_push_branch_cb,
                             g_object_ref (self));
}

static void
gbp_vcsui_tree_addin_load (IdeTreeAddin *addin,
                           IdeTree      *tree,
                           IdeTreeModel *model)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  GtkStyleContext *style_context;
  IdeWorkbench *workbench;
  IdeVcsMonitor *monitor;
  IdeVcs *vcs;
  static const GActionEntry actions[] = {
    { "switch-branch", gbp_vcsui_tree_addin_switch_branch },
    { "push-branch", gbp_vcsui_tree_addin_push_branch },
  };

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  self->model = model;
  self->tree = tree;

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (tree),
                                  "vcsui",
                                  G_ACTION_GROUP (group));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (tree));
  g_signal_connect_object (style_context,
                           "changed",
                           G_CALLBACK (on_tree_style_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_tree_style_changed_cb (self, style_context);

  if ((workbench = ide_widget_get_workbench (GTK_WIDGET (tree))) &&
      (vcs = ide_workbench_get_vcs (workbench)) &&
      (monitor = ide_workbench_get_vcs_monitor (workbench)))
    {
      self->vcs = g_object_ref (vcs);
      self->monitor = g_object_ref (monitor);
      g_signal_connect_object (self->monitor,
                               "changed",
                               G_CALLBACK (gtk_widget_queue_draw),
                               tree,
                               G_CONNECT_SWAPPED);
    }
}

static void
gbp_vcsui_tree_addin_unload (IdeTreeAddin *addin,
                             IdeTree      *tree,
                             IdeTreeModel *model)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  GtkStyleContext *style_context;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (IDE_IS_TREE_MODEL (model));

  gtk_widget_insert_action_group (GTK_WIDGET (tree), "vcsui", NULL);

  style_context = gtk_widget_get_style_context (GTK_WIDGET (tree));
  g_signal_handlers_disconnect_by_func (style_context,
                                        G_CALLBACK (on_tree_style_changed_cb),
                                        self);

  g_clear_object (&self->monitor);
  g_clear_object (&self->vcs);
  self->model = NULL;
  self->tree = NULL;
}

static void
gbp_vcsui_tree_addin_selection_changed (IdeTreeAddin *addin,
                                        IdeTreeNode  *node)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  gboolean is_branch = FALSE;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  if (node != NULL)
    is_branch = ide_tree_node_holds (node, IDE_TYPE_VCS_BRANCH);

  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "vcsui", "switch-branch",
                             "enabled", is_branch,
                             NULL);
  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "vcsui", "push-branch",
                             "enabled", is_branch,
                             NULL);
}

static void
gbp_vcsui_tree_addin_cell_data_func (IdeTreeAddin    *addin,
                                     IdeTreeNode     *node,
                                     GtkCellRenderer *cell)
{
  GbpVcsuiTreeAddin *self = (GbpVcsuiTreeAddin *)addin;
  g_autoptr(IdeVcsFileInfo) info = NULL;
  g_autoptr(GFile) file = NULL;
  IdeProjectFile *project_file;
  IdeTreeNodeFlags flags = 0;

  g_assert (GBP_IS_VCSUI_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (GTK_IS_CELL_RENDERER (cell));

  if (self->monitor == NULL)
    return;

  if (!ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    return;

  project_file = ide_tree_node_get_item (node);
  file = ide_project_file_ref_file (project_file);

  if ((info = ide_vcs_monitor_ref_info (self->monitor, file)))
    {
      IdeVcsFileStatus status = ide_vcs_file_info_get_status (info);

      if (status == IDE_VCS_FILE_STATUS_ADDED)
        flags = IDE_TREE_NODE_FLAGS_ADDED;
      else if (status == IDE_VCS_FILE_STATUS_CHANGED)
        flags = IDE_TREE_NODE_FLAGS_CHANGED;

      if (flags && ide_tree_node_has_child (node))
        flags |= IDE_TREE_NODE_FLAGS_DESCENDANT;
    }

  ide_tree_node_set_flags (node, flags);
}

static void
gbp_vcsui_tree_addin_list_branches_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) branches = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if ((branches = ide_vcs_list_branches_finish (vcs, result, &error)))
    {
      IdeTreeNode *parent = ide_task_get_task_data (task);

      for (guint i = 0; i < branches->len; i++)
        {
          IdeVcsBranch *branch = g_ptr_array_index (branches, i);
          g_autofree gchar *name = ide_vcs_branch_get_name (branch);
          g_autoptr(IdeTreeNode) child = NULL;

          child = g_object_new (IDE_TYPE_TREE_NODE,
                                "display-name", name,
                                "icon-name", "builder-vcs-git-symbolic",
                                "item", branch,
                                "tag", "vcs-branch",
                                NULL);
          ide_tree_node_append (parent, child);
        }
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (branches, g_object_unref);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_vcsui_tree_addin_list_tags_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) tags = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if ((tags = ide_vcs_list_tags_finish (vcs, result, &error)))
    {
      IdeTreeNode *parent = ide_task_get_task_data (task);

      for (guint i = 0; i < tags->len; i++)
        {
          IdeVcsTag *tag = g_ptr_array_index (tags, i);
          g_autofree gchar *name = ide_vcs_tag_get_name (tag);
          g_autoptr(IdeTreeNode) child = NULL;

          child = g_object_new (IDE_TYPE_TREE_NODE,
                                "display-name", name,
                                "icon-name", "builder-vcs-git-symbolic",
                                "item", tag,
                                "tag", "vcs-tag",
                                NULL);
          ide_tree_node_append (parent, child);
        }
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (tags, g_object_unref);

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_vcsui_tree_addin_build_children_async (IdeTreeAddin        *addin,
                                           IdeTreeNode         *node,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_vcsui_tree_addin_build_children_async);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  if (ide_tree_node_holds (node, IDE_TYPE_CONTEXT))
    {
      IdeContext *context = ide_tree_node_get_item (node);
      IdeVcs *vcs = ide_vcs_from_context (context);

      if (!IDE_IS_DIRECTORY_VCS (vcs))
        {
          g_autoptr(IdeTreeNode) vcs_node = NULL;

          vcs_node = g_object_new (IDE_TYPE_TREE_NODE,
                                   "children-possible", TRUE,
                                   "display-name", _("Version Control"),
                                   "icon-name", "builder-vcs-git-symbolic",
                                   "is-header", TRUE,
                                   "item", vcs,
                                   "tag", "vcs",
                                   NULL);
          ide_tree_node_prepend (node, vcs_node);
        }
    }
  else if (ide_tree_node_holds (node, IDE_TYPE_VCS) &&
           ide_tree_node_is_tag (node, "vcs-branches"))
    {
      IdeVcs *vcs = ide_tree_node_get_item (node);

      ide_vcs_list_branches_async (vcs,
                                   cancellable,
                                   gbp_vcsui_tree_addin_list_branches_cb,
                                   g_steal_pointer (&task));
      return;
    }
  else if (ide_tree_node_holds (node, IDE_TYPE_VCS) &&
           ide_tree_node_is_tag (node, "vcs-tags"))
    {
      IdeVcs *vcs = ide_tree_node_get_item (node);

      ide_vcs_list_tags_async (vcs,
                               cancellable,
                               gbp_vcsui_tree_addin_list_tags_cb,
                               g_steal_pointer (&task));
      return;
    }
  else if (ide_tree_node_holds (node, IDE_TYPE_VCS) &&
           ide_tree_node_is_tag (node, "vcs"))
    {
      IdeVcs *vcs = ide_tree_node_get_item (node);
      g_autoptr(IdeTreeNode) branches = NULL;
      g_autoptr(IdeTreeNode) tags = NULL;

      branches = g_object_new (IDE_TYPE_TREE_NODE,
                               "children-possible", TRUE,
                               "display-name", _("Branches"),
                               "icon-name", "folder-symbolic",
                               "expanded-icon-name", "folder-open-symbolic",
                               "item", vcs,
                               "tag", "vcs-branches",
                               NULL);
      ide_tree_node_append (node, branches);

      tags = g_object_new (IDE_TYPE_TREE_NODE,
                           "children-possible", TRUE,
                           "display-name", _("Tags"),
                           "icon-name", "folder-symbolic",
                           "expanded-icon-name", "folder-open-symbolic",
                           "item", vcs,
                           "tag", "vcs-tags",
                           NULL);
      ide_tree_node_append (node, tags);
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_vcsui_tree_addin_build_children_finish (IdeTreeAddin  *addin,
                                            GAsyncResult  *result,
                                            GError       **error)
{
  g_assert (GBP_IS_VCSUI_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->cell_data_func = gbp_vcsui_tree_addin_cell_data_func;
  iface->load = gbp_vcsui_tree_addin_load;
  iface->selection_changed = gbp_vcsui_tree_addin_selection_changed;
  iface->unload = gbp_vcsui_tree_addin_unload;
  iface->build_children_async = gbp_vcsui_tree_addin_build_children_async;
  iface->build_children_finish = gbp_vcsui_tree_addin_build_children_finish;
}

G_DEFINE_TYPE_WITH_CODE (GbpVcsuiTreeAddin, gbp_vcsui_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_vcsui_tree_addin_class_init (GbpVcsuiTreeAddinClass *klass)
{
}

static void
gbp_vcsui_tree_addin_init (GbpVcsuiTreeAddin *self)
{
}
