/* gbp-grep-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-grep-tree-addin"

#include "config.h"

#include <libide-gui.h>
#include <libide-projects.h>
#include <libide-tree.h>

#include "gbp-grep-tree-addin.h"
#include "gbp-grep-popover.h"

struct _GbpGrepTreeAddin
{
  GObject             parent_instance;
  IdeTree            *tree;
  GSimpleActionGroup *group;
};

static void
find_in_files_action (GSimpleAction *action,
                      GVariant      *param,
                      gpointer       user_data)
{
  GbpGrepTreeAddin *self = user_data;
  g_autoptr(GFile) file = NULL;
  IdeProjectFile *project_file;
  IdeTreeNode *node;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GREP_TREE_ADDIN (self));
  g_assert (self->tree != NULL);
  g_assert (IDE_IS_TREE (self->tree));

  if ((node = ide_tree_get_selected_node (self->tree)) &&
      ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE) &&
      (project_file = ide_tree_node_get_item (node)) &&
      (file = ide_project_file_ref_file (project_file)))
    {
      gboolean is_dir = ide_project_file_is_directory (project_file);
      GtkPopover *popover;

      popover = g_object_new (GBP_TYPE_GREP_POPOVER,
                              "file", file,
                              "is-directory", is_dir,
                              "position", GTK_POS_RIGHT,
                              NULL);
      ide_tree_show_popover_at_node (self->tree, node, popover);
    }
}

static void
gbp_grep_tree_addin_load (IdeTreeAddin *addin,
                          IdeTree      *tree)
{
  static const GActionEntry actions[] = {
    { "find-in-files", find_in_files_action },
  };

  GbpGrepTreeAddin *self = (GbpGrepTreeAddin *)addin;
  GtkWidget *pane;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  self->tree = tree;

  pane = gtk_widget_get_ancestor (GTK_WIDGET (tree), IDE_TYPE_PANE);
  g_assert (IDE_IS_PANE (pane));

  self->group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (self->group),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (tree),
                                  "grep",
                                  G_ACTION_GROUP (self->group));
}

static void
gbp_grep_tree_addin_unload (IdeTreeAddin *addin,
                            IdeTree      *tree)
{
  GbpGrepTreeAddin *self = (GbpGrepTreeAddin *)addin;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  gtk_widget_insert_action_group (GTK_WIDGET (tree), "grep", NULL);
  g_clear_object (&self->group);
  self->tree = NULL;
}

static void
gbp_grep_tree_addin_selection_changed (IdeTreeAddin *addin,
                                       IdeTreeNode  *node)
{
  GbpGrepTreeAddin *self = (GbpGrepTreeAddin *)addin;
  GAction *action;
  gboolean enabled;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_GREP_TREE_ADDIN (self));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  enabled = node && ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE);
  action = g_action_map_lookup_action (G_ACTION_MAP (self->group), "find-in-files");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->load = gbp_grep_tree_addin_load;
  iface->unload = gbp_grep_tree_addin_unload;
  iface->selection_changed = gbp_grep_tree_addin_selection_changed;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpGrepTreeAddin, gbp_grep_tree_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_grep_tree_addin_class_init (GbpGrepTreeAddinClass *klass)
{
}

static void
gbp_grep_tree_addin_init (GbpGrepTreeAddin *self)
{
}
