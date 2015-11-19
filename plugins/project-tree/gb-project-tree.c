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

#define G_LOG_DOMAIN "project-tree"

#include <glib/gi18n.h>

#include "gb-project-tree.h"
#include "gb-project-tree-actions.h"
#include "gb-project-tree-builder.h"
#include "gb-project-tree-private.h"

G_DEFINE_TYPE (GbProjectTree, gb_project_tree, IDE_TYPE_TREE)

enum {
  PROP_0,
  PROP_SHOW_IGNORED_FILES,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkWidget *
gb_project_tree_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_TREE, NULL);
}

IdeContext *
gb_project_tree_get_context (GbProjectTree *self)
{
  IdeTreeNode *root;
  GObject *item;

  g_return_val_if_fail (GB_IS_PROJECT_TREE (self), NULL);

  if ((root = ide_tree_get_root (IDE_TREE (self))) &&
      (item = ide_tree_node_get_item (root)) &&
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
  IdeTreeNode *root;

  g_return_if_fail (GB_IS_PROJECT_TREE (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self));

  root = ide_tree_node_new ();
  ide_tree_node_set_item (root, G_OBJECT (context));
  ide_tree_set_root (IDE_TREE (self), root);

  /*
   * If we only have one toplevel item (underneath root), expand it.
   */
  if ((gtk_tree_model_iter_n_children (model, NULL) == 1) &&
      gtk_tree_model_get_iter_first (model, &iter))
    {
      g_autoptr(IdeTreeNode) node = NULL;

      gtk_tree_model_get (model, &iter, 0, &node, -1);
      if (node != NULL)
        ide_tree_node_expand (node, FALSE);
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
gb_project_tree_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbProjectTree *self = GB_PROJECT_TREE(object);

  switch (prop_id)
    {
    case PROP_SHOW_IGNORED_FILES:
      g_value_set_boolean (value, gb_project_tree_get_show_ignored_files (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_tree_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbProjectTree *self = GB_PROJECT_TREE(object);

  switch (prop_id)
    {
    case PROP_SHOW_IGNORED_FILES:
      gb_project_tree_set_show_ignored_files (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_tree_class_init (GbProjectTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_project_tree_finalize;
  object_class->get_property = gb_project_tree_get_property;
  object_class->set_property = gb_project_tree_set_property;

  properties [PROP_SHOW_IGNORED_FILES] =
    g_param_spec_boolean ("show-ignored-files",
                          "Show Ignored Files",
                          "If files ignored by the VCS should be displayed.",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_project_tree_init (GbProjectTree *self)
{
  IdeTreeBuilder *builder;

  self->settings = g_settings_new ("org.gnome.builder.project-tree");

  g_settings_bind (self->settings, "show-icons",
                   self, "show-icons",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "show-ignored-files",
                   self, "show-ignored-files",
                   G_SETTINGS_BIND_DEFAULT);

  builder = gb_project_tree_builder_new ();
  ide_tree_add_builder (IDE_TREE (self), builder);

  g_signal_connect (self,
                    "notify::selection",
                    G_CALLBACK (gb_project_tree_notify_selection),
                    NULL);

  gb_project_tree_actions_init (self);
}

gboolean
gb_project_tree_get_show_ignored_files (GbProjectTree *self)
{
  g_return_val_if_fail (GB_IS_PROJECT_TREE (self), FALSE);

  return self->show_ignored_files;
}

void
gb_project_tree_set_show_ignored_files (GbProjectTree *self,
                                        gboolean       show_ignored_files)
{
  g_return_if_fail (GB_IS_PROJECT_TREE (self));

  show_ignored_files = !!show_ignored_files;

  if (show_ignored_files != self->show_ignored_files)
    {
      self->show_ignored_files = show_ignored_files;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_IGNORED_FILES]);
      ide_tree_rebuild (IDE_TREE (self));
    }
}
