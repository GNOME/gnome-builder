/* gb-vcs-tree-builder.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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
   * The context for our current project. This is set when the IdeContext
   * node is built (usually the first node).
   */
  IdeContext *context;

  /*
   * This is a mapping of a #GFile to the node that was built for it.
   * After we've returned to the main loop, an operation is requested
   * to get all of the project file status and then each node is updated
   * with the resulting information.
   */
  GHashTable *queued;

  /*
   * Our registered idle handler to flush the requests into a VCS query.
   * This will start the async process which queries the VCS (and then we
   * can handle the results to update the nodes).
   */
  guint queued_handler;
};

struct
{
  GdkRGBA added;
  GdkRGBA changed;
  GdkRGBA renamed;
} colors;

G_DEFINE_TYPE (GbVcsTreeBuilder, gb_vcs_tree_builder, DZL_TYPE_TREE_BUILDER)

static void gb_vcs_tree_builder_build_node (DzlTreeBuilder *builder,
                                            DzlTreeNode    *node);

static void
gb_vcs_tree_builder_monitor_changed_cb (GbVcsTreeBuilder        *self,
                                        GFile                   *file,
                                        GFile                   *other_file,
                                        GFileMonitorEvent        event,
                                        DzlRecursiveFileMonitor *monitor)
{
  g_autoptr(DzlTreeNode) node = NULL;
  DzlTree *tree;

  g_assert (GB_IS_VCS_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (DZL_IS_RECURSIVE_FILE_MONITOR (monitor));

  /* Rebuild the node if we found it */
  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  node = gb_project_tree_find_file_node (GB_PROJECT_TREE (tree), file);

  for (; node != NULL; node = dzl_tree_node_get_parent (node))
    gb_vcs_tree_builder_build_node (DZL_TREE_BUILDER (self), node);

}

static void
gb_vcs_tree_builder_set_context (GbVcsTreeBuilder *self,
                                 IdeContext       *context)
{
  DzlRecursiveFileMonitor *monitor;

  g_assert (GB_IS_VCS_TREE_BUILDER (self));
  g_assert (IDE_IS_CONTEXT (context));

  if (self->context == context)
    return;

  dzl_set_weak_pointer (&self->context, context);

  monitor = ide_context_get_monitor (context);

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gb_vcs_tree_builder_monitor_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_vcs_tree_builder_list_status_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeVcs *vcs = (IdeVcs *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GListModel) model = NULL;
  GbVcsTreeBuilder *self;
  DzlTree *tree;
  GHashTable *queued;
  GFile *workdir;
  guint n_items;

  g_assert (IDE_IS_VCS (vcs));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  g_assert (GB_IS_VCS_TREE_BUILDER (self));

  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE (tree));

  model = ide_vcs_list_status_finish (vcs, result, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    return;

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      return;
    }

  queued = g_task_get_task_data (task);
  g_assert (queued != NULL);

  workdir = ide_vcs_get_working_directory (vcs);
  g_assert (G_IS_FILE (workdir));

  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeVcsFileInfo) info = g_list_model_get_item (model, i);
      g_autoptr(GFile) file = g_object_ref (ide_vcs_file_info_get_file (info));
      IdeVcsFileStatus status = ide_vcs_file_info_get_status (info);

      /*
       * We need to walk our way up to the root from the status file
       * so that we ensure we get all of the parent nodes colored to
       * match their descendants.
       */

      while (g_file_has_prefix (file, workdir))
        {
          g_autoptr(GFile) parent = g_file_get_parent (file);
          DzlTreeNode *node = g_hash_table_lookup (queued, file);

          if (node != NULL)
            {
              switch (status)
                {
                case IDE_VCS_FILE_STATUS_UNTRACKED:
                case IDE_VCS_FILE_STATUS_ADDED:
                  /* Only set foreground if parent is unset */
                  if (!dzl_tree_node_get_foreground_rgba (node))
                    dzl_tree_node_set_foreground_rgba (node, &colors.added);
                  break;

                case IDE_VCS_FILE_STATUS_CHANGED:
                  /* Unconditionally set changed, as it is higher priority
                   * than "added" when applying to paents
                   */
                  dzl_tree_node_set_foreground_rgba (node, &colors.changed);
                  break;

                case IDE_VCS_FILE_STATUS_RENAMED:
                  /* Do not inherit unnamed in parents */
                  dzl_tree_node_set_foreground_rgba (node, &colors.renamed);
                  goto next;

                case IDE_VCS_FILE_STATUS_DELETED:
                case IDE_VCS_FILE_STATUS_IGNORED:
                case IDE_VCS_FILE_STATUS_UNCHANGED:
                default:
                  /* Do not inherit defaults in parents */
                  dzl_tree_node_set_foreground_rgba (node, NULL);
                  goto next;
                }
            }

          g_set_object (&file, parent);
        }

      next:
        continue;
    }

  gtk_widget_queue_draw (GTK_WIDGET (tree));
}

static void
add_parent_nodes (GHashTable *queued)
{
  g_autoptr(GHashTable) parents = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_assert (queued != NULL);

  parents = g_hash_table_new (NULL, NULL);

  g_hash_table_iter_init (&iter, queued);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      DzlTreeNode *node = value;
      DzlTreeNode *parent;

      g_assert (DZL_IS_TREE_NODE (node));
      g_assert (G_IS_FILE (key));

      for (parent = dzl_tree_node_get_parent (node);
           parent != NULL;
           parent = dzl_tree_node_get_parent (parent))
        {
          GObject *item = dzl_tree_node_get_item (parent);
          GFile *file;

          if (!GB_IS_PROJECT_FILE (item))
            break;

          file = gb_project_file_get_file (GB_PROJECT_FILE (item));
          if (!g_hash_table_contains (queued, file))
            g_hash_table_insert (parents, file, parent);
        }
    }

  g_hash_table_iter_init (&iter, parents);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GFile *file = key;
      DzlTreeNode *node = value;

      g_assert (G_IS_FILE (file));
      g_assert (DZL_IS_TREE_NODE (node));

      g_hash_table_insert (queued, g_object_ref (file), g_object_ref (node));
    }
}

static gboolean
gb_vcs_tree_builder_flush (gpointer data)
{
  GbVcsTreeBuilder *self = data;
  g_autoptr(GHashTable) queued = NULL;
  DzlTreeNode *root;
  IdeContext *context;
  DzlTree *tree;

  g_assert (GB_IS_VCS_TREE_BUILDER (self));

  self->queued_handler = 0;

  queued = g_steal_pointer (&self->queued);
  if (queued == NULL)
    return G_SOURCE_REMOVE;

  /*
   * For each of the node's that are queued, add their parent
   * nodes so that we can update their status based on the vcs
   * result too.
   */
  add_parent_nodes (queued);

  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  root = dzl_tree_get_root (tree);
  context = IDE_CONTEXT (dzl_tree_node_get_item (root));

  if (context != NULL && queued != NULL)
    {
      g_autoptr(GTask) task = NULL;
      IdeVcs *vcs;

      task = g_task_new (self, NULL, NULL, NULL);
      g_task_set_source_tag (task, gb_vcs_tree_builder_flush);
      g_task_set_priority (task, G_PRIORITY_LOW);
      g_task_set_task_data (task,
                            g_steal_pointer (&queued),
                            (GDestroyNotify) g_hash_table_unref);

      vcs = ide_context_get_vcs (context);

      /* TODO: We could possibly reduce how much we look at here
       *       instead of querying the whole data set.
       */

      ide_vcs_list_status_async (vcs,
                                 NULL,
                                 TRUE,
                                 G_PRIORITY_LOW,
                                 NULL,
                                 gb_vcs_tree_builder_list_status_cb,
                                 g_steal_pointer (&task));
    }

  return G_SOURCE_REMOVE;
}

static void
gb_vcs_tree_builder_build_node (DzlTreeBuilder *builder,
                                DzlTreeNode    *node)
{
  GbVcsTreeBuilder *self = (GbVcsTreeBuilder *)builder;
  GObject *item;
  GFile *file;

  g_assert (GB_IS_VCS_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);

  if (IDE_IS_CONTEXT (item))
    {
      gb_vcs_tree_builder_set_context (self, IDE_CONTEXT (item));
      return;
    }

  if (!GB_IS_PROJECT_FILE (item))
    return;

  file = gb_project_file_get_file (GB_PROJECT_FILE (item));

  if (!G_IS_FILE (file))
    return;

  if (self->queued == NULL)
    self->queued = g_hash_table_new_full (g_file_hash,
                                          (GEqualFunc) g_file_equal,
                                          g_object_unref,
                                          g_object_unref);

  g_assert (G_IS_FILE (file));
  g_assert (DZL_IS_TREE_NODE (node));

  g_hash_table_insert (self->queued, g_object_ref (file), g_object_ref (node));

  if (self->queued_handler == 0)
    self->queued_handler = g_idle_add_full (G_PRIORITY_LOW,
                                            gb_vcs_tree_builder_flush,
                                            g_object_ref (self),
                                            g_object_unref);
}

static void
gb_vcs_tree_builder_dispose (GObject *object)
{
  GbVcsTreeBuilder *self = (GbVcsTreeBuilder *)object;

  g_clear_pointer (&self->queued, g_hash_table_unref);
  dzl_clear_weak_pointer (&self->context);
  dzl_clear_source (&self->queued_handler);

  G_OBJECT_CLASS (gb_vcs_tree_builder_parent_class)->dispose (object);
}

static void
gb_vcs_tree_builder_class_init (GbVcsTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlTreeBuilderClass *builder_class = DZL_TREE_BUILDER_CLASS (klass);

  object_class->dispose = gb_vcs_tree_builder_dispose;

  builder_class->build_node = gb_vcs_tree_builder_build_node;

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
