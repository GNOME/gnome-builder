/* gb-project-tree-builder.c
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

#include "gb-editor-workspace.h"
#include "gb-project-tree-builder.h"
#include "gb-tree.h"
#include "gb-widget.h"
#include "gb-workbench.h"

typedef struct
{
  IdeContext *context;
} GbProjectTreeBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GbProjectTreeBuilder,
                            gb_project_tree_builder,
                            GB_TYPE_TREE_BUILDER)

enum {
  PROP_0,
  PROP_CONTEXT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbProjectTreeBuilder *
gb_project_tree_builder_new (IdeContext *context)
{
  g_return_val_if_fail (!context || IDE_IS_CONTEXT (context), NULL);

  return g_object_new (GB_TYPE_PROJECT_TREE_BUILDER,
                       "context", context,
                       NULL);
}

IdeContext *
gb_project_tree_builder_get_context (GbProjectTreeBuilder *self)
{
  GbProjectTreeBuilderPrivate *priv = gb_project_tree_builder_get_instance_private (self);

  g_return_val_if_fail (GB_IS_PROJECT_TREE_BUILDER (self), NULL);

  return priv->context;
}

void
gb_project_tree_builder_set_context (GbProjectTreeBuilder *self,
                                     IdeContext           *context)
{
  GbProjectTreeBuilderPrivate *priv = gb_project_tree_builder_get_instance_private (self);

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (!context || IDE_IS_CONTEXT (context));

  if (g_set_object (&priv->context, context))
    {
      GtkWidget *tree;

      g_object_notify (G_OBJECT (self), "context");

      if ((tree = gb_tree_builder_get_tree (GB_TREE_BUILDER (self))))
        gb_tree_rebuild (GB_TREE (tree));
    }
}

static const gchar *
get_icon_name (GFileInfo *file_info)
{
  GFileType file_type;

  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  file_type = g_file_info_get_file_type (file_info);

  if (file_type == G_FILE_TYPE_DIRECTORY)
    return "folder-symbolic";

  return "text-x-generic";
}

static void
build_context (GbProjectTreeBuilder *self,
               GbTreeNode           *node)
{
  IdeProject *project;
  IdeContext *context;
  GbTreeNode *child;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  context = IDE_CONTEXT (gb_tree_node_get_item (node));
  project = ide_context_get_project (context);

  child = g_object_new (GB_TYPE_TREE_NODE,
                        "item", project,
                        NULL);
  g_object_bind_property (project, "name", child, "text",
                          G_BINDING_SYNC_CREATE);
  gb_tree_node_append (node, child);
}

static void
build_project (GbProjectTreeBuilder *self,
               GbTreeNode           *node)
{
  IdeProjectItem *root;
  GSequenceIter *iter;
  IdeProject *project;
  GSequence *children;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  project = IDE_PROJECT (gb_tree_node_get_item (node));

  root = ide_project_get_root (project);
  children = ide_project_item_get_children (root);

  if (children)
    {
      iter = g_sequence_get_begin_iter (children);

      for (iter = g_sequence_get_begin_iter (children);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter))
        {
          IdeProjectItem *item = g_sequence_get (iter);

          if (IDE_IS_PROJECT_FILES (item))
            {
              GbTreeNode *child;

              child = g_object_new (GB_TYPE_TREE_NODE,
                                    "icon-name", "folder-symbolic",
                                    "item", item,
                                    "text", _("Files"),
                                    NULL);
              gb_tree_node_append (node, child);
              break;
            }
        }
    }
}

static void
build_files (GbProjectTreeBuilder *self,
             GbTreeNode           *node)
{
  IdeProjectItem *files;
  GSequenceIter *iter;
  GSequence *children;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  files = IDE_PROJECT_ITEM (gb_tree_node_get_item (node));
  children = ide_project_item_get_children (files);

  if (children)
    {
      iter = g_sequence_get_begin_iter (children);

      for (iter = g_sequence_get_begin_iter (children);
           !g_sequence_iter_is_end (iter);
           iter = g_sequence_iter_next (iter))
        {
          IdeProjectItem *item = g_sequence_get (iter);
          const gchar *display_name;
          const gchar *icon_name;
          GbTreeNode *child;
          GFileInfo *file_info;

          if (!IDE_IS_PROJECT_FILE (item))
            continue;

          file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));

          display_name = g_file_info_get_display_name (file_info);
          icon_name = get_icon_name (file_info);

          child = g_object_new (GB_TYPE_TREE_NODE,
                                "text", display_name,
                                "icon-name", icon_name,
                                "item", item,
                                NULL);
          gb_tree_node_append (node, child);
        }
    }
}

static void
gb_project_tree_builder_build_node (GbTreeBuilder *builder,
                                    GbTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));

  item = gb_tree_node_get_item (node);

  if (IDE_IS_CONTEXT (item))
    build_context (self, node);
  else if (IDE_IS_PROJECT (item))
    build_project (self, node);
  else if (IDE_IS_PROJECT_FILES (item) || IDE_IS_PROJECT_FILE (item))
    build_files (self, node);
}

static gboolean
gb_project_tree_builder_node_activated (GbTreeBuilder *builder,
                                        GbTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_return_val_if_fail (GB_IS_PROJECT_TREE_BUILDER (self), FALSE);

  item = gb_tree_node_get_item (node);

  if (IDE_IS_PROJECT_FILE (item))
    {
      GbWorkbench *workbench;
      GbWorkspace *workspace;
      GFileInfo *file_info;
      GbTree *tree;
      GFile *file;

      file_info = ide_project_file_get_file_info (IDE_PROJECT_FILE (item));
      if (!file_info)
        goto failure;

      if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
        goto failure;

      file = ide_project_file_get_file (IDE_PROJECT_FILE (item));
      if (!file)
        goto failure;

      tree = gb_tree_node_get_tree (node);
      if (!tree)
        goto failure;

      workbench = gb_widget_get_workbench (GTK_WIDGET (tree));
      workspace = gb_workbench_get_workspace (workbench,
                                              GB_TYPE_EDITOR_WORKSPACE);
      gb_editor_workspace_open (GB_EDITOR_WORKSPACE (workspace), file);

      return TRUE;
    }

failure:
  return FALSE;
}

static void
gb_project_tree_builder_finalize (GObject *object)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)object;
  GbProjectTreeBuilderPrivate *priv = gb_project_tree_builder_get_instance_private (self);

  g_clear_object (&priv->context);

  G_OBJECT_CLASS (gb_project_tree_builder_parent_class)->finalize (object);
}

static void
gb_project_tree_builder_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GbProjectTreeBuilder *self = GB_PROJECT_TREE_BUILDER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, gb_project_tree_builder_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_tree_builder_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GbProjectTreeBuilder *self = GB_PROJECT_TREE_BUILDER (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      gb_project_tree_builder_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_project_tree_builder_class_init (GbProjectTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbTreeBuilderClass *tree_builder_class = GB_TREE_BUILDER_CLASS (klass);

  object_class->finalize = gb_project_tree_builder_finalize;
  object_class->get_property = gb_project_tree_builder_get_property;
  object_class->set_property = gb_project_tree_builder_set_property;

  tree_builder_class->build_node = gb_project_tree_builder_build_node;
  tree_builder_class->node_activated = gb_project_tree_builder_node_activated;

  gParamSpecs [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         _("Context"),
                         _("The ide context for the project tree."),
                         IDE_TYPE_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CONTEXT,
                                   gParamSpecs [PROP_CONTEXT]);
}

static void
gb_project_tree_builder_init (GbProjectTreeBuilder *self)
{
}
