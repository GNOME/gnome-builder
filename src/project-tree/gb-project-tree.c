/* gb-project-tree.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-project-tree.h"
#include "gb-project-tree-actions.h"
#include "gb-project-tree-builder.h"
#include "gb-project-tree-private.h"

#define WIDTH_MIN 1
#define WIDTH_MAX 1000

G_DEFINE_TYPE (GbProjectTree, gb_project_tree, GB_TYPE_TREE)

GtkWidget *
gb_project_tree_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_TREE, NULL);
}

guint
gb_project_tree_get_desired_width (GbProjectTree *self)
{
  return g_settings_get_int (self->settings, "width");
}

void
gb_project_tree_save_desired_width (GbProjectTree *self)
{
  GtkAllocation alloc;
  guint width;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  width = CLAMP (alloc.width, WIDTH_MIN, WIDTH_MAX);
  g_settings_set_int (self->settings, "width", width);
}

IdeContext *
gb_project_tree_get_context (GbProjectTree *self)
{
  GbTreeNode *root;
  GObject *item;

  g_return_val_if_fail (GB_IS_PROJECT_TREE (self), NULL);

  if ((root = gb_tree_get_root (GB_TREE (self))) &&
      (item = gb_tree_node_get_item (root)) &&
      IDE_IS_OBJECT (item))
    return ide_object_get_context (IDE_OBJECT (item));

  return NULL;
}

void
gb_project_tree_set_context (GbProjectTree *self,
                             IdeContext    *context)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GbTreeNode *root;

  g_return_if_fail (GB_IS_PROJECT_TREE (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self));

  root = gb_tree_node_new ();
  gb_tree_node_set_item (root, G_OBJECT (context));
  gb_tree_set_root (GB_TREE (self), root);

  /*
   * If we only have one toplevel item (underneath root), expand it.
   */
  if ((gtk_tree_model_iter_n_children (model, NULL) == 1) &&
      gtk_tree_model_get_iter_first (model, &iter))
    {
      g_autoptr(GbTreeNode) node = NULL;

      gtk_tree_model_get (model, &iter, 0, &node, -1);
      if (node != NULL)
        gb_tree_node_expand (node, FALSE);
    }
}


static void
gb_project_tree_notify_selection (GbProjectTree *self)
{
  g_assert (GB_IS_PROJECT_TREE (self));

  gb_project_tree_actions_update (self);
}

static void
gb_project_tree_finalize (GObject *object)
{
  GbProjectTree *self = (GbProjectTree *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gb_project_tree_parent_class)->finalize (object);
}

static void
gb_project_tree_class_init (GbProjectTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_project_tree_finalize;
}

static void
gb_project_tree_init (GbProjectTree *self)
{
  GbTreeBuilder *builder;

  self->settings = g_settings_new ("org.gnome.builder.project-tree");
  g_settings_bind (self->settings, "show-icons", self, "show-icons",
                   G_SETTINGS_BIND_DEFAULT);

  builder = gb_project_tree_builder_new ();
  gb_tree_add_builder (GB_TREE (self), builder);

  g_signal_connect (self,
                    "notify::selection",
                    G_CALLBACK (gb_project_tree_notify_selection),
                    NULL);

  gb_project_tree_actions_init (self);
}
