/* gbp-project-tree-frame-addin.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-project-tree-frame-addin"

#include "config.h"

#include <libide-gui.h>

#include "gbp-project-tree-frame-addin.h"
#include "gbp-project-tree-workspace-addin.h"
#include "gbp-project-tree.h"

struct _GbpProjectTreeFrameAddin
{
  GObject     parent_instance;
  IdeFrame   *frame;
  IdePage    *page;
  GActionMap *actions;
};

static void
gbp_project_tree_frame_addin_reveal (GSimpleAction *action,
                                     GVariant      *params,
                                     gpointer       user_data)
{
  GbpProjectTreeFrameAddin *self = user_data;
  g_autoptr(GFile) file = NULL;
  GtkWidget *workspace;
  IdeWorkspaceAddin *addin;
  GbpProjectTree *tree;

  g_assert (GBP_IS_PROJECT_TREE_FRAME_ADDIN (self));

  if (self->page == NULL ||
     !(file = ide_page_get_file_or_directory (self->page)) ||
     !(workspace = gtk_widget_get_ancestor (GTK_WIDGET (self->page), IDE_TYPE_WORKSPACE)) ||
     !(addin = ide_workspace_addin_find_by_module_name (IDE_WORKSPACE (workspace), "project-tree")) ||
     !GBP_IS_PROJECT_TREE_WORKSPACE_ADDIN (addin) ||
     !(tree = gbp_project_tree_workspace_addin_get_tree (GBP_PROJECT_TREE_WORKSPACE_ADDIN (addin))))
    return;

  gbp_project_tree_reveal (tree, file);
}

static void
gbp_project_tree_frame_addin_load (IdeFrameAddin *addin,
                                   IdeFrame      *frame)
{
  GbpProjectTreeFrameAddin *self = (GbpProjectTreeFrameAddin *)addin;
  g_autoptr(GSimpleActionGroup) group = NULL;
  static const GActionEntry actions[] = {
    { "reveal", gbp_project_tree_frame_addin_reveal },
  };

  g_assert (GBP_IS_PROJECT_TREE_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (frame));

  group = g_simple_action_group_new ();

  self->frame = frame;
  self->actions = g_object_ref (G_ACTION_MAP (group));

  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (frame), "project-tree", G_ACTION_GROUP (group));
}

static void
gbp_project_tree_frame_addin_unload (IdeFrameAddin *addin,
                                     IdeFrame      *frame)
{
  GbpProjectTreeFrameAddin *self = (GbpProjectTreeFrameAddin *)addin;

  g_assert (GBP_IS_PROJECT_TREE_FRAME_ADDIN (self));
  g_assert (IDE_IS_FRAME (frame));

  self->page = NULL;
  self->frame = NULL;
  g_clear_object (&self->actions);
  gtk_widget_insert_action_group (GTK_WIDGET (frame), "project-tree", NULL);
}

static void
gbp_project_tree_frame_addin_set_page (IdeFrameAddin *addin,
                                       IdePage       *page)
{
  g_assert (GBP_IS_PROJECT_TREE_FRAME_ADDIN (addin));
  g_assert (!page || IDE_IS_PAGE (page));

  GBP_PROJECT_TREE_FRAME_ADDIN (addin)->page = page;
}

static void
frame_addin_iface_init (IdeFrameAddinInterface *iface)
{
  iface->load = gbp_project_tree_frame_addin_load;
  iface->unload = gbp_project_tree_frame_addin_unload;
  iface->set_page = gbp_project_tree_frame_addin_set_page;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpProjectTreeFrameAddin, gbp_project_tree_frame_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_FRAME_ADDIN, frame_addin_iface_init))

static void
gbp_project_tree_frame_addin_class_init (GbpProjectTreeFrameAddinClass *klass)
{
}

static void
gbp_project_tree_frame_addin_init (GbpProjectTreeFrameAddin *self)
{
}
