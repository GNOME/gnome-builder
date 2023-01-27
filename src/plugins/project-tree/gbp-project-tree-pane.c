/* gbp-project-tree-pane.c
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

#define G_LOG_DOMAIN "gbp-project-tree-pane"

#include "config.h"

#include "gbp-project-tree-private.h"
#include "gbp-project-tree.h"

G_DEFINE_FINAL_TYPE (GbpProjectTreePane, gbp_project_tree_pane, IDE_TYPE_PANE)

static gboolean
gbp_project_tree_pane_grab_focus (GtkWidget *widget)
{
  return gtk_widget_grab_focus (GTK_WIDGET (GBP_PROJECT_TREE_PANE (widget)->tree));
}

static void
gbp_project_tree_pane_class_init (GbpProjectTreePaneClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = gbp_project_tree_pane_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/project-tree/gbp-project-tree-pane.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpProjectTreePane, tree);

  g_type_ensure (GBP_TYPE_PROJECT_TREE);
}

static void
gbp_project_tree_pane_init (GbpProjectTreePane *self)
{
  IdeApplication *app;
  GMenu *menu;

  gtk_widget_init_template (GTK_WIDGET (self));

  app = IDE_APPLICATION_DEFAULT;
  menu = ide_application_get_menu_by_id (IDE_APPLICATION (app), "project-tree-menu");
  ide_tree_set_menu_model (self->tree, G_MENU_MODEL (menu));

  g_signal_connect_object (self->tree,
                           "notify::selected-node",
                           G_CALLBACK (_gbp_project_tree_pane_update_actions),
                           self,
                           G_CONNECT_SWAPPED);

  _gbp_project_tree_pane_init_actions (self);
}

GbpProjectTree *
gbp_project_tree_pane_get_tree (GbpProjectTreePane *self)
{
  g_return_val_if_fail (GBP_IS_PROJECT_TREE_PANE (self), NULL);

  return GBP_PROJECT_TREE (self->tree);
}
