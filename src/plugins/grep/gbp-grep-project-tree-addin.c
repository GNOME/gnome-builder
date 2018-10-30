/* gbp-grep-project-tree-addin.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#define G_LOG_DOMAIN "gbp-grep-project-tree-addin"

#include "gbp-grep-popover.h"
#include "gbp-grep-project-tree-addin.h"

/* This crosses the plugin boundary, but it is easier for now
 * until we get some of the project tree stuff moved into a
 * libide-project library (or similar).
 */
#include "../project-tree/gb-project-file.h"

struct _GbpGrepProjectTreeAddin
{
  GObject         parent_instance;
  DzlTree        *tree;
  DzlTreeBuilder *builder;
};

static GFile *
get_file_for_node (DzlTreeNode *node,
                   gboolean    *is_dir)
{
  GObject *item;

  g_return_val_if_fail (!node || DZL_IS_TREE_NODE (node), NULL);

  if (is_dir)
    *is_dir = FALSE;

  if (node == NULL)
    return NULL;

  if (!(item = dzl_tree_node_get_item (node)))
    return NULL;

  if (GB_IS_PROJECT_FILE (item))
    {
      if (is_dir)
        *is_dir = gb_project_file_get_is_directory (GB_PROJECT_FILE (item));
      return gb_project_file_get_file (GB_PROJECT_FILE (item));
    }

  return NULL;
}

static void
popover_closed_cb (GtkPopover *popover)
{
  IdeWorkbench *workbench;

  g_assert (GTK_IS_POPOVER (popover));

  /*
   * Clear focus before destroying popover, or we risk some
   * re-entrancy issues in libdazzle. Needs safer tracking of
   * focus widgets as gtk is not clearing pointers in destroy.
   */

  workbench = ide_widget_get_workbench (GTK_WIDGET (popover));
  gtk_window_set_focus (GTK_WINDOW (workbench), NULL);
  gtk_widget_destroy (GTK_WIDGET (popover));
}

static void
find_in_files_action (GSimpleAction *action,
                      GVariant      *param,
                      gpointer       user_data)
{
  GbpGrepProjectTreeAddin *self = user_data;
  DzlTreeNode *node;
  GFile *file;
  gboolean is_dir = FALSE;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_GREP_PROJECT_TREE_ADDIN (self));

  if ((node = dzl_tree_get_selected (self->tree)) &&
      (file = get_file_for_node (node, &is_dir)))
    {
      GtkPopover *popover;

      popover = g_object_new (GBP_TYPE_GREP_POPOVER,
                              "file", file,
                              "is-directory", is_dir,
                              "position", GTK_POS_RIGHT,
                              NULL);
      g_signal_connect_after (popover,
                              "closed",
                              G_CALLBACK (popover_closed_cb),
                              NULL);
      dzl_tree_node_show_popover (node, popover);
    }
}

static void
on_node_selected_cb (GbpGrepProjectTreeAddin *self,
                     DzlTreeNode             *node,
                     DzlTreeBuilder          *builder)
{
  GFile *file;
  gboolean is_dir = FALSE;

  g_assert (GBP_IS_GREP_PROJECT_TREE_ADDIN (self));
  g_assert (!node || DZL_IS_TREE_NODE (node));
  g_assert (DZL_IS_TREE_BUILDER (builder));

  file = get_file_for_node (node, &is_dir);

  dzl_gtk_widget_action_set (GTK_WIDGET (self->tree), "grep", "find-in-files",
                             "enabled", (file != NULL),
                             NULL);
}

static void
gbp_grep_project_tree_addin_load (IdeProjectTreeAddin *addin,
                                  DzlTree             *tree)
{
  GbpGrepProjectTreeAddin *self = (GbpGrepProjectTreeAddin *)addin;
  g_autoptr(GActionMap) group = NULL;
  static GActionEntry actions[] = {
    { "find-in-files", find_in_files_action },
  };

  g_assert (GBP_IS_GREP_PROJECT_TREE_ADDIN (self));
  g_assert (DZL_IS_TREE (tree));
  g_assert (self->builder == NULL);
  g_assert (self->tree == NULL);

  self->tree = tree;

  group = G_ACTION_MAP (g_simple_action_group_new ());
  g_action_map_add_action_entries (group, actions, G_N_ELEMENTS (actions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (tree), "grep", G_ACTION_GROUP (group));

  self->builder = g_object_ref_sink (dzl_tree_builder_new ());
  g_signal_connect_object (self->builder,
                           "node-selected",
                           G_CALLBACK (on_node_selected_cb),
                           self,
                           G_CONNECT_SWAPPED);
  dzl_tree_add_builder (tree, self->builder);
}

static void
gbp_grep_project_tree_addin_unload (IdeProjectTreeAddin *addin,
                                    DzlTree             *tree)
{
  GbpGrepProjectTreeAddin *self = (GbpGrepProjectTreeAddin *)addin;

  g_assert (GBP_IS_GREP_PROJECT_TREE_ADDIN (self));
  g_assert (DZL_IS_TREE (tree));
  g_assert (self->builder != NULL);
  g_assert (self->tree == tree);

  gtk_widget_insert_action_group (GTK_WIDGET (tree), "grep", NULL);
  dzl_tree_remove_builder (tree, self->builder);
  g_clear_object (&self->builder);

  self->tree = NULL;
}

static void
project_tree_addin_iface_init (IdeProjectTreeAddinInterface *iface)
{
  iface->load = gbp_grep_project_tree_addin_load;
  iface->unload = gbp_grep_project_tree_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpGrepProjectTreeAddin, gbp_grep_project_tree_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_PROJECT_TREE_ADDIN,
                                                project_tree_addin_iface_init))

static void
gbp_grep_project_tree_addin_class_init (GbpGrepProjectTreeAddinClass *klass)
{
}

static void
gbp_grep_project_tree_addin_init (GbpGrepProjectTreeAddin *self)
{
}
