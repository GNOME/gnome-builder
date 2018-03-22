/* gb-vcs-tree-builder.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gb-vcs-tree-builder"

#include <ide.h>

#include "gb-project-file.h"
#include "gb-project-tree.h"
#include "gb-vcs-tree-builder.h"

struct _GbVcsTreeBuilder
{
  DzlTreeBuilder parent_instance;

  /*
   * The monitor is used to lookup the status of a file, such as if it
   * is untracked, added, changed, etc.
   */
  IdeVcsMonitor *monitor;
};

struct
{
  GdkRGBA added;
  GdkRGBA changed;
  GdkRGBA renamed;
} colors;

G_DEFINE_TYPE (GbVcsTreeBuilder, gb_vcs_tree_builder, DZL_TYPE_TREE_BUILDER)

static void
gb_vcs_tree_builder_cell_data_func (DzlTreeBuilder  *builder,
                                    DzlTreeNode     *node,
                                    GtkCellRenderer *cell)
{
  GbVcsTreeBuilder *self = (GbVcsTreeBuilder *)builder;
  IdeVcsFileInfo *info;
  GObject *item;
  GFile *file;
  IdeVcsFileStatus status;

  g_assert (DZL_IS_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));
  g_assert (GTK_IS_CELL_RENDERER (cell));

  if (!GTK_IS_CELL_RENDERER_TEXT (cell))
    goto unset;

  /* try to not touch anything if we're NULL (the empty node) */
  item = dzl_tree_node_get_item (node);
  if (item == NULL)
    return;

  if (!GB_IS_PROJECT_FILE (item))
    goto unset;

  file = gb_project_file_get_file (GB_PROJECT_FILE (item));
  if (file == NULL)
    goto unset;

  if G_UNLIKELY (self->monitor == NULL)
    {
      DzlTree *tree;
      DzlTreeNode *root;
      IdeContext *context;

      tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
      root = dzl_tree_get_root (tree);
      context = IDE_CONTEXT (dzl_tree_node_get_item (root));

      self->monitor = g_object_ref (ide_context_get_monitor (context));
    }

  info = ide_vcs_monitor_get_info (self->monitor, file);
  if (info == NULL)
    goto unset;

  status = ide_vcs_file_info_get_status (info);

  switch (status)
    {
    case IDE_VCS_FILE_STATUS_UNTRACKED:
    case IDE_VCS_FILE_STATUS_ADDED:
      g_object_set (cell,
                    "foreground-rgba", &colors.added,
                    "weight", PANGO_WEIGHT_BOLD,
                    NULL);
      break;

    case IDE_VCS_FILE_STATUS_CHANGED:
      g_object_set (cell,
                    "foreground-rgba", &colors.changed,
                    "weight", PANGO_WEIGHT_BOLD,
                    NULL);
      break;

    case IDE_VCS_FILE_STATUS_RENAMED:
      g_object_set (cell,
                    "foreground-rgba", &colors.renamed,
                    "weight", PANGO_WEIGHT_BOLD,
                    NULL);
      break;

    case IDE_VCS_FILE_STATUS_DELETED:
    case IDE_VCS_FILE_STATUS_IGNORED:
    case IDE_VCS_FILE_STATUS_UNCHANGED:
    default:
      goto unset;
    }

  return;

unset:
  g_object_set (cell,
                "foreground-set", FALSE,
                "weight-set", FALSE,
                NULL);
}

static void
gb_vcs_tree_builder_dispose (GObject *object)
{
  GbVcsTreeBuilder *self = (GbVcsTreeBuilder *)object;

  g_clear_object (&self->monitor);

  G_OBJECT_CLASS (gb_vcs_tree_builder_parent_class)->dispose (object);
}

static void
gb_vcs_tree_builder_class_init (GbVcsTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlTreeBuilderClass *builder_class = DZL_TREE_BUILDER_CLASS (klass);

  object_class->dispose = gb_vcs_tree_builder_dispose;

  builder_class->cell_data_func = gb_vcs_tree_builder_cell_data_func;

  /* TODO: We need a better way to define and handle colors here.
   *       We probably want it to come from the theme, but if we decided
   *       to style the project tree using the colors from the editor, then
   *       we'll need to use the style-scheme for diff::changes/etc.
   */
  gdk_rgba_parse (&colors.added, "#739216");
  gdk_rgba_parse (&colors.changed, "#f57900");
  gdk_rgba_parse (&colors.renamed, "#346514");
}

static void
gb_vcs_tree_builder_init (GbVcsTreeBuilder *self)
{
}

DzlTreeBuilder *
gb_vcs_tree_builder_new (void)
{
  return g_object_new (GB_TYPE_VCS_TREE_BUILDER, NULL);
}
