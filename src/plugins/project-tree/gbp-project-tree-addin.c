/* gbp-project-tree-addin.c
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

#define G_LOG_DOMAIN "gbp-project-tree-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-gui.h>
#include <libide-projects.h>
#include <libide-tree.h>
#include <libide-vcs.h>

#include "ide-buffer-private.h"
#include "ide-tree-private.h"

#include "gbp-project-tree.h"
#include "gbp-project-tree-addin.h"

struct _GbpProjectTreeAddin
{
  GObject       parent_instance;

  IdeTree      *tree;
  GSettings    *settings;

  guint         sort_directories_first : 1;
  guint         show_ignored_files : 1;
};

typedef struct
{
  GFile       *file;
  IdeTreeNode *node;
} FindFileNode;

static gboolean
project_file_is_ignored (IdeProjectFile *project_file,
                         IdeVcs         *vcs)
{
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_PROJECT_FILE (project_file));

  file = ide_project_file_ref_file (project_file);

  return ide_vcs_is_ignored (vcs, file, NULL);
}

static gint
compare_files (gconstpointer a,
               gconstpointer b,
               gpointer      user_data)
{
  GbpProjectTreeAddin *self = user_data;
  IdeProjectFile *file_a = *(IdeProjectFile **)a;
  IdeProjectFile *file_b = *(IdeProjectFile **)b;

  g_assert (IDE_IS_PROJECT_FILE (file_a));
  g_assert (IDE_IS_PROJECT_FILE (file_b));

  if (self->sort_directories_first)
    return ide_project_file_compare_directories_first (file_a, file_b);
  else
    return ide_project_file_compare (file_a, file_b);
}

static IdeTreeNode *
create_file_node (IdeProjectFile *file,
                  IdeVcs         *vcs)
{
  IdeTreeNode *child;

  g_assert (IDE_IS_PROJECT_FILE (file));

  child = ide_tree_node_new ();
  ide_tree_node_set_item (child, G_OBJECT (file));
  if (vcs != NULL && project_file_is_ignored(file, vcs))
    ide_tree_node_set_vcs_ignored (child, TRUE);
  ide_tree_node_set_title (child, ide_project_file_get_display_name (file));
  ide_tree_node_set_icon (child, ide_project_file_get_symbolic_icon (file));
  ide_tree_node_set_destroy_item (child, TRUE);

  if (ide_project_file_is_directory (file))
    {
      ide_tree_node_set_children_possible (child, TRUE);
      ide_tree_node_set_expanded_icon_name (child, "folder-open-symbolic");
    }

  return g_steal_pointer (&child);
}

static void
gbp_project_tree_addin_file_list_children_cb (GObject      *object,
                                              GAsyncResult *result,
                                              gpointer      user_data)
{
  IdeProjectFile *project_file = (IdeProjectFile *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GPtrArray) children = NULL;
  g_autoptr(GError) error = NULL;
  GbpProjectTreeAddin *self;
  IdeTreeNode *last = NULL;
  IdeTreeNode *node;
  IdeTreeNode *root;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_PROJECT_FILE (project_file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(children = ide_project_file_list_children_finish (project_file, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  IDE_PTR_ARRAY_SET_FREE_FUNC (children, g_object_unref);

  self = ide_task_get_source_object (task);
  node = ide_task_get_task_data (task);
  root = ide_tree_node_get_root (node);
  context = ide_tree_node_get_item (root);
  vcs = ide_vcs_from_context (context);

  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (node));

  g_ptr_array_sort_with_data (children, compare_files, self);

  for (guint i = 0; i < children->len; i++)
    {
      IdeProjectFile *file = g_ptr_array_index (children, i);
      g_autoptr(IdeTreeNode) child = NULL;

      if (!self->show_ignored_files)
        {
          if (project_file_is_ignored (file, vcs))
            continue;
        }

      ide_object_append (IDE_OBJECT (project_file), IDE_OBJECT (file));

      child = create_file_node (file, vcs);

      if (last == NULL)
        ide_tree_node_insert_before (child, node, NULL);
      else
        ide_tree_node_insert_after (child, node, last);

      last = child;
    }

  ide_task_return_boolean (task, TRUE);
}

static void
gbp_project_tree_addin_build_children_async (IdeTreeAddin        *addin,
                                             IdeTreeNode         *node,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));

  task = ide_task_new (addin, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_project_tree_addin_build_children_async);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  if (ide_tree_node_holds (node, IDE_TYPE_CONTEXT))
    {
      IdeContext *context = ide_tree_node_get_item (node);
      g_autoptr(IdeTreeNode) files = NULL;
      g_autoptr(IdeProjectFile) root_file = NULL;
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
      g_autoptr(GFile) parent = g_file_get_parent (workdir);
      g_autoptr(GFileInfo) info = NULL;
      g_autofree char *name = NULL;

      info = g_file_info_new ();
      name = g_file_get_basename (workdir);
      g_file_info_set_name (info, name);
      g_file_info_set_display_name (info, name);
      g_file_info_set_content_type (info, "inode/directory");
      g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
      g_file_info_set_is_symlink (info, FALSE);

      root_file = ide_project_file_new (parent, info);
      ide_object_append (IDE_OBJECT (context), IDE_OBJECT (root_file));

      files = create_file_node (root_file, NULL);
      ide_tree_node_set_title (files, _("Files"));
      ide_tree_node_set_icon_name (files, "view-list-symbolic");
      ide_tree_node_set_expanded_icon_name (files, "view-list-symbolic");
      ide_tree_node_set_is_header (files, TRUE);
      ide_tree_node_insert_before (files, node, NULL);
    }
  else if (ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    {
      IdeProjectFile *project_file = ide_tree_node_get_item (node);

      ide_project_file_list_children_async (project_file,
                                            cancellable,
                                            gbp_project_tree_addin_file_list_children_cb,
                                            g_steal_pointer (&task));

      return;
    }

  ide_task_return_boolean (task, TRUE);
}

static gboolean
gbp_project_tree_addin_build_children_finish (IdeTreeAddin  *addin,
                                              GAsyncResult  *result,
                                              GError       **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static gboolean
gbp_project_tree_addin_node_activated (IdeTreeAddin *addin,
                                       IdeTree      *tree,
                                       IdeTreeNode  *node)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));

  if (ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    {
      IdeProjectFile *project_file = ide_tree_node_get_item (node);
      g_autoptr(GFile) file = NULL;
      IdeWorkbench *workbench;

      /* Ignore directories, we want to expand them */
      if (ide_project_file_is_directory (project_file))
        return FALSE;

      file = ide_project_file_ref_file (project_file);
      workbench = ide_widget_get_workbench (GTK_WIDGET (tree));

      ide_workbench_open_async (workbench, file, NULL, 0, NULL, NULL, NULL, NULL);

      return TRUE;
    }

  return FALSE;
}

static IdeTreeNodeVisit
traverse_cb (IdeTreeNode *node,
             gpointer     user_data)
{
  FindFileNode *find = user_data;

  if (ide_tree_node_get_parent (node) == NULL)
    return IDE_TREE_NODE_VISIT_CHILDREN;

  if (ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    {
      IdeProjectFile *project_file = ide_tree_node_get_item (node);
      g_autoptr(GFile) file = ide_project_file_ref_file (project_file);

      if (g_file_equal (find->file, file))
        {
          find->node = node;
          return IDE_TREE_NODE_VISIT_BREAK;
        }

      if (g_file_has_prefix (find->file, file))
        return IDE_TREE_NODE_VISIT_CHILDREN;
    }

  return IDE_TREE_NODE_VISIT_CONTINUE;
}

static IdeTreeNode *
find_file_node (IdeTree *tree,
                GFile   *file)
{
  IdeTreeNode *root;
  FindFileNode find;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE (tree));
  g_assert (G_IS_FILE (file));

  root = ide_tree_get_root (tree);

  find.file = file;
  find.node = NULL;

  ide_tree_node_traverse (root,
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          -1,
                          traverse_cb,
                          &find);

  return find.node;
}

static void
get_parent (GFile **file)
{
  GFile *child = *file;
  *file = g_file_get_parent (child);
  g_object_unref (child);
}

static GList *
collect_files (GFile *file,
               GFile *stop_at)
{
  GList *list = NULL;

  g_assert (g_file_equal (file, stop_at) ||
            g_file_has_prefix (file, stop_at));

  for (GFile *iter = g_object_ref (file); iter; get_parent (&iter))
    {
      list = g_list_prepend (list, g_object_ref (iter));

      if (g_file_equal (iter, stop_at))
        {
          g_object_unref (iter);
          break;
        }
    }

  return list;
}

static int
node_compare_directories_first (IdeTreeNode *node,
                                IdeTreeNode *child)
{
  gint cmp;
  const gchar *child_name, *node_name;
  g_autofree gchar *collated_child = NULL;
  g_autofree gchar *collated_node = NULL;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (IDE_IS_TREE_NODE (child));

  /* Child is a directory and *must* be last in line at this point
   * given that node is a regular file.
   * Hence break comparation for subsequent ide_tree_node_insert_before() */
  if (ide_tree_node_get_children_possible (child) &&
      !ide_tree_node_get_children_possible (node))
    return 0;

  /* Skip directories if child is a regular file */
  if (!ide_tree_node_get_children_possible (child) &&
      ide_tree_node_get_children_possible (node))
    return 1;

  child_name = ide_tree_node_get_title (child);
  node_name = ide_tree_node_get_title (node);

  collated_child = g_utf8_collate_key_for_filename (child_name, -1);
  collated_node = g_utf8_collate_key_for_filename (node_name, -1);

  cmp = g_strcmp0 (collated_child, collated_node);

  return cmp > 0 ? cmp : 0;
}

static int
node_compare (IdeTreeNode *node,
              IdeTreeNode *child)
{
  gint cmp;
  const gchar *child_name, *node_name;
  g_autofree gchar *collated_child = NULL;
  g_autofree gchar *collated_node = NULL;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (IDE_IS_TREE_NODE (child));

  child_name = ide_tree_node_get_title (child);
  node_name = ide_tree_node_get_title (node);

  collated_child = g_utf8_collate_key_for_filename (child_name, -1);
  collated_node = g_utf8_collate_key_for_filename (node_name, -1);

  cmp = g_strcmp0 (collated_child, collated_node);

  return cmp > 0 ? cmp : 0;
}

static IdeTreeNode *
find_child (IdeTreeNode *node,
            GFile       *file)
{
  for (IdeTreeNode *child = ide_tree_node_get_first_child (node);
       child != NULL;
       child = ide_tree_node_get_next_sibling (child))
    {
      g_autoptr(GFile) f = NULL;
      IdeProjectFile *pf;

      if (!ide_tree_node_holds (child, IDE_TYPE_PROJECT_FILE))
        continue;

      pf = ide_tree_node_get_item (child);
      f = ide_project_file_ref_file (pf);

      if (g_file_equal (f, file))
        return child;
    }

  return NULL;
}

static void
gbp_project_tree_addin_add_file (GbpProjectTreeAddin *self,
                                 GFile               *file)
{
  g_autolist(GFile) list = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeTreeNode *parent;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (G_IS_FILE (file));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (file);
    IDE_TRACE_MSG ("Adding file to tree \"%s\"", uri);
  }
#endif

  context = ide_widget_get_context (GTK_WIDGET (self->tree));
  workdir = ide_context_ref_workdir (context);

  if (!g_file_has_prefix (file, workdir))
    return;

  list = collect_files (file, workdir);
  parent = ide_tree_get_root (self->tree);

  for (const GList *iter = list; iter; iter = iter->next)
    {
      GFile *part = iter->data;
      IdeTreeNode *node;

      /* If node is not expanded (and not the root), then we
       * can bail immediately.
       */
      if (ide_tree_node_get_parent (parent) &&
          !ide_tree_is_node_expanded (self->tree, parent))
        break;

      if (!(node = find_child (parent, part)))
        {
          g_autoptr(IdeProjectFile) project_file = NULL;
          g_autoptr(GFileInfo) info = NULL;
          g_autoptr(GFile) directory = NULL;
          IdeProjectFile *parent_file = NULL;

          info = g_file_query_info (part,
                                    IDE_PROJECT_FILE_ATTRIBUTES,
                                    G_FILE_QUERY_INFO_NONE,
                                    NULL, NULL);

          if (info == NULL)
            break;

          if (iter->prev)
            directory = g_object_ref (iter->prev->data);
          else
            directory = g_file_get_parent (part);

          parent_file = IDE_PROJECT_FILE (ide_tree_node_get_item (parent));
          g_assert (IDE_IS_PROJECT_FILE (parent_file));

          project_file = ide_project_file_new (directory, info);
          g_assert (IDE_IS_PROJECT_FILE (project_file));

          ide_object_append (IDE_OBJECT (parent_file), IDE_OBJECT (project_file));

          node = create_file_node (project_file, NULL);

          if (self->sort_directories_first)
            ide_tree_node_insert_sorted (parent, node, node_compare_directories_first);
          else
            ide_tree_node_insert_sorted (parent, node, node_compare);

          g_object_unref (node);
        }

      parent = node;
    }

  IDE_EXIT;
}

static void
gbp_project_tree_addin_remove_file (GbpProjectTreeAddin *self,
                                    GFile               *file)
{
  IdeTreeNode *selected;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (G_IS_FILE (file));

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *uri = g_file_get_uri (file);
    IDE_TRACE_MSG ("Removing file from tree \"%s\"", uri);
  }
#endif

  if ((selected = find_file_node (self->tree, file)))
    {
      IdeTreeNode *parent = ide_tree_node_get_parent (selected);

      ide_tree_node_remove (parent, selected);
    }

  IDE_EXIT;
}

static void
gbp_project_tree_addin_changed_cb (GbpProjectTreeAddin *self,
                                   GFile               *file,
                                   GFile               *other_file,
                                   GFileMonitorEvent    event,
                                   IdeVcsMonitor       *monitor)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (IDE_IS_VCS_MONITOR (monitor));

  if (event == G_FILE_MONITOR_EVENT_CREATED)
    gbp_project_tree_addin_add_file (self, file);
  else if (event == G_FILE_MONITOR_EVENT_DELETED)
    gbp_project_tree_addin_remove_file (self, file);
}

static void
gbp_project_tree_addin_reloaded_cb (GbpProjectTreeAddin *self,
                                    IdeVcsMonitor       *monitor)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_VCS_MONITOR (monitor));

  /* TODO: Need to update visible tree node flags */
}

static void
gbp_project_tree_addin_load (IdeTreeAddin *addin,
                             IdeTree      *tree)
{
  GbpProjectTreeAddin *self = (GbpProjectTreeAddin *)addin;
  IdeVcsMonitor *monitor;
  IdeWorkbench *workbench;

  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  self->tree = tree;

  workbench = ide_widget_get_workbench (GTK_WIDGET (tree));
  monitor = ide_workbench_get_vcs_monitor (workbench);

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (gbp_project_tree_addin_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (monitor,
                           "reloaded",
                           G_CALLBACK (gbp_project_tree_addin_reloaded_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gbp_project_tree_addin_unload (IdeTreeAddin *addin,
                               IdeTree      *tree)
{
  GbpProjectTreeAddin *self = (GbpProjectTreeAddin *)addin;

  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE (tree));

  self->tree = NULL;
}

static GdkContentProvider *
gbp_project_tree_addin_node_draggable (IdeTreeAddin *addin,
                                       IdeTreeNode  *node)
{
  if (ide_tree_node_holds (node, IDE_TYPE_PROJECT_FILE))
    {
      IdeProjectFile *pf = ide_tree_node_get_item (node);

      return gdk_content_provider_new_typed (GDK_TYPE_FILE_LIST,
                                             g_slist_prepend (NULL, ide_project_file_ref_file (pf)));
    }

  return NULL;
}

static GdkDragAction
gbp_project_tree_addin_node_droppable (IdeTreeAddin  *addin,
                                       GtkDropTarget *drop_target,
                                       IdeTreeNode   *drop_node,
                                       GArray        *gtypes)
{
  GdkContentFormats *formats;
  GType file_list_type = GDK_TYPE_FILE_LIST;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (addin));
  g_assert (GTK_IS_DROP_TARGET (drop_target));
  g_assert (!drop_node || IDE_IS_TREE_NODE (drop_node));
  g_assert (gtypes != NULL);

  g_array_append_val (gtypes, file_list_type);

  /* Must drop on a file */
  if (drop_node == NULL ||
      !ide_tree_node_holds (drop_node, IDE_TYPE_PROJECT_FILE))
    return 0;

  /* Make sure it's a GDK_TYPE_FILE_LIST */
  if (!(formats = gtk_drop_target_get_formats (drop_target)) ||
      !gdk_content_formats_contain_gtype (formats, file_list_type))
    return 0;

  return GDK_ACTION_COPY | GDK_ACTION_MOVE;
}

static void
gbp_project_tree_addin_notify_progress_cb (IdeFileTransfer *transfer,
                                           GParamSpec      *pspec,
                                           IdeNotification *notif)
{
  g_autofree gchar *body = NULL;
  IdeFileTransferStat stbuf;
  gchar count[16];
  gchar total[16];
  gdouble progress;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FILE_TRANSFER (transfer));
  g_assert (IDE_IS_NOTIFICATION (notif));

  ide_file_transfer_stat (transfer, &stbuf);

  progress = ide_file_transfer_get_progress (transfer);
  ide_notification_set_progress (notif, progress);

  g_snprintf (count, sizeof count, "%"G_GINT64_FORMAT, stbuf.n_files);
  g_snprintf (total, sizeof total, "%"G_GINT64_FORMAT, stbuf.n_files_total);

  if (stbuf.n_files_total == 1)
    body = g_strdup_printf (_("Copying 1 file"));
  else
    /* translators: first %s is replaced with completed number of files, second %s with total number of files */
    body = g_strdup_printf (_("Copying %s of %s files"), count, total);

  ide_notification_set_body (notif, body);
}

static void
gbp_project_tree_addin_transfer_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeFileTransfer *transfer = (IdeFileTransfer *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpProjectTreeAddin *self;
  IdeNotification *notif;
  IdeFileTransferStat stbuf;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FILE_TRANSFER (transfer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  notif = ide_task_get_task_data (task);

  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (notif != NULL);
  g_assert (IDE_IS_NOTIFICATION (notif));

  gbp_project_tree_addin_notify_progress_cb (transfer, NULL, notif);
  ide_notification_set_progress (notif, 1.0);

  if (!ide_file_transfer_execute_finish (transfer, result, &error))
    {
      ide_notification_set_title (notif, _("Failed to copy files"));
      ide_notification_set_body (notif, error->message);
      ide_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      g_autofree gchar *format = NULL;
      GPtrArray *sources;
      gchar count[16];

      ide_notification_set_title (notif, _("Files copied"));

      ide_file_transfer_stat (transfer, &stbuf);
      g_snprintf (count, sizeof count, "%"G_GINT64_FORMAT, stbuf.n_files_total);
      format = g_strdup_printf (ngettext ("Copied %s file", "Copied %s files", stbuf.n_files_total), count);
      ide_notification_set_body (notif, format);

      sources = g_object_get_data (G_OBJECT (task), "SOURCE_FILES");

      if (sources != NULL)
        {
          IdeContext *context;
          IdeProject *project;

          /*
           * We avoid deleting files here and instead just trash the
           * existing files to help reduce any chance that we delete
           * user data.
           *
           * Also, this will only trash files that are within our
           * project directory. Currently, I'm considering that a
           * feature, but when I trust file-deletion more, we can
           * open it up in IdeProject.
           */

          context = ide_widget_get_context (GTK_WIDGET (self->tree));
          project = ide_project_from_context (context);

          for (guint i = 0; i < sources->len; i++)
            {
              GFile *source = g_ptr_array_index (sources, i);

              g_assert (G_IS_FILE (source));

              ide_project_trash_file_async (project, source, NULL, NULL, NULL);
            }
        }

      ide_task_return_boolean (task, TRUE);
    }

  ide_notification_withdraw_in_seconds (notif, -1);

  IDE_EXIT;
}

static void
gbp_project_tree_addin_rename_buffer_cb (IdeBuffer *buffer,
                                         gpointer   user_data)
{
  struct {
    GFile *src;
    GFile *dst;
  } *foreach = user_data;
  GFile *file;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (foreach != NULL);
  g_assert (G_IS_FILE (foreach->src));
  g_assert (G_IS_FILE (foreach->dst));

  file = ide_buffer_get_file (buffer);

  if (ide_buffer_get_is_temporary (buffer))
    return;

  if (g_file_has_prefix (file, foreach->src) || g_file_equal (file, foreach->src))
    {
      g_autofree gchar *suffix = g_file_get_relative_path (foreach->src, file);
      g_autoptr(GFile) new_dst = NULL;

      if (suffix == NULL)
        new_dst = g_file_dup (foreach->dst);
      else
        new_dst = g_file_get_child (foreach->dst, suffix);

      _ide_buffer_set_file (buffer, new_dst);
    }
}

static void
gbp_project_tree_addin_node_dropped_async (IdeTreeAddin        *addin,
                                           GtkDropTarget       *drop_target,
                                           IdeTreeNode         *drop_node,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GbpProjectTreeAddin *self = (GbpProjectTreeAddin *)addin;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeFileTransfer) transfer = NULL;
  g_autoptr(GFile) src_file = NULL;
  g_autoptr(GFile) dst_dir = NULL;
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GPtrArray) srcs = NULL;
  IdeBufferManager *buffer_manager;
  IdeProjectFile *drop_file;
  IdeContext *context;
  const GList *files = NULL;
  const GValue *value;
  GdkDrop *drop;
  GdkDragAction action;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (IDE_IS_TREE_NODE (drop_node));
  g_assert (ide_tree_node_holds (drop_node, IDE_TYPE_PROJECT_FILE));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_project_tree_addin_node_dropped_async);

  value = gtk_drop_target_get_value (drop_target);
  drop = gtk_drop_target_get_current_drop (drop_target);
  action = gdk_drop_get_actions (drop);

  g_assert (G_IS_VALUE (value));
  g_assert (gdk_drag_action_is_unique (action));

  srcs = g_ptr_array_new_with_free_func (g_object_unref);
  if (G_VALUE_HOLDS (value, GDK_TYPE_FILE_LIST))
    files = g_value_get_boxed (value);

  if (files != NULL)
    {
      for (const GList *iter = files; iter; iter = iter->next)
        g_ptr_array_add (srcs, g_object_ref (iter->data));
    }

  drop_file = IDE_PROJECT_FILE (ide_tree_node_get_item (drop_node));

  if (ide_project_file_is_directory (drop_file))
    dst_dir = ide_project_file_ref_file (drop_file);
  else
    dst_dir = g_object_ref (ide_project_file_get_directory (drop_file));

  g_assert (G_IS_FILE (dst_dir));

  transfer = ide_file_transfer_new ();
  ide_file_transfer_set_flags (transfer, IDE_FILE_TRANSFER_FLAGS_NONE);
  g_signal_connect_object (transfer,
                           "notify::progress",
                           G_CALLBACK (gbp_project_tree_addin_notify_progress_cb),
                           notif,
                           0);

  context = ide_widget_get_context (GTK_WIDGET (self->tree));
  buffer_manager = ide_buffer_manager_from_context (context);

  for (guint i = 0; i < srcs->len; i++)
    {
      GFile *source = g_ptr_array_index (srcs, i);
      g_autofree gchar *name = NULL;
      g_autoptr(GFile) dst_file = NULL;
      struct {
        GFile *src;
        GFile *dst;
      } foreach;

      name = g_file_get_basename (source);
      g_assert (name != NULL);

      dst_file = g_file_get_child (dst_dir, name);
      g_assert (G_IS_FILE (dst_file));

      if (srcs->len == 1 && g_file_equal (source, dst_file))
        {
          ide_task_return_boolean (task, TRUE);
          IDE_EXIT;
        }

      ide_file_transfer_add (transfer, source, dst_file);

      /* If there are any buffers that are open with this file as an
       * ancester, then we need to rename there file to point at the
       * new location.
       */
      foreach.src = source;
      foreach.dst = dst_file;
      ide_buffer_manager_foreach (buffer_manager,
                                  gbp_project_tree_addin_rename_buffer_cb,
                                  &foreach);
    }

  if (action == GDK_ACTION_MOVE)
    {
      ide_file_transfer_set_flags (transfer, IDE_FILE_TRANSFER_FLAGS_MOVE);
      g_object_set_data_full (G_OBJECT (task),
                              "SOURCE_FILES",
                              g_steal_pointer (&srcs),
                              (GDestroyNotify)g_ptr_array_unref);
    }

  notif = ide_notification_new ();
  ide_notification_set_title (notif, _("Copying files…"));
  ide_notification_set_body (notif, _("Files will be copied in a moment"));
  ide_notification_set_has_progress (notif, TRUE);
  ide_notification_attach (notif, IDE_OBJECT (context));
  ide_task_set_task_data (task, g_object_ref (notif), g_object_unref);

  ide_file_transfer_execute_async (transfer,
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   gbp_project_tree_addin_transfer_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
gbp_project_tree_addin_node_dropped_finish (IdeTreeAddin  *addin,
                                            GAsyncResult  *result,
                                            GError       **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (result));

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static void
tree_addin_iface_init (IdeTreeAddinInterface *iface)
{
  iface->load = gbp_project_tree_addin_load;
  iface->unload = gbp_project_tree_addin_unload;
  iface->build_children_async = gbp_project_tree_addin_build_children_async;
  iface->build_children_finish = gbp_project_tree_addin_build_children_finish;
  iface->node_activated = gbp_project_tree_addin_node_activated;
  iface->node_draggable = gbp_project_tree_addin_node_draggable;
  iface->node_droppable = gbp_project_tree_addin_node_droppable;
  iface->node_dropped_async = gbp_project_tree_addin_node_dropped_async;
  iface->node_dropped_finish = gbp_project_tree_addin_node_dropped_finish;
}

static gboolean
invalidate_in_idle_cb (GbpProjectTree *tree)
{
  ide_tree_invalidate_all (IDE_TREE (tree));
  gbp_project_tree_expand_files (tree);
  return G_SOURCE_REMOVE;
}

static void
gbp_project_tree_addin_settings_changed (GbpProjectTreeAddin *self,
                                         const gchar         *key,
                                         GSettings           *settings)
{
  g_assert (GBP_IS_PROJECT_TREE_ADDIN (self));
  g_assert (G_IS_SETTINGS (settings));

  self->sort_directories_first = g_settings_get_boolean (self->settings, "sort-directories-first");
  self->show_ignored_files = g_settings_get_boolean (self->settings, "show-ignored-files");

  if (self->tree != NULL)
    g_idle_add_full (G_PRIORITY_LOW,
                     (GSourceFunc) invalidate_in_idle_cb,
                     g_object_ref (self->tree),
                     g_object_unref);
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpProjectTreeAddin, gbp_project_tree_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_TREE_ADDIN, tree_addin_iface_init))

static void
gbp_project_tree_addin_dispose (GObject *object)
{
  GbpProjectTreeAddin *self = (GbpProjectTreeAddin *)object;

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gbp_project_tree_addin_parent_class)->dispose (object);
}

static void
gbp_project_tree_addin_class_init (GbpProjectTreeAddinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_project_tree_addin_dispose;
}

static void
gbp_project_tree_addin_init (GbpProjectTreeAddin *self)
{
  self->settings = g_settings_new ("org.gnome.builder.project-tree");

  g_signal_connect_object (self->settings,
                           "changed",
                           G_CALLBACK (gbp_project_tree_addin_settings_changed),
                           self,
                           G_CONNECT_SWAPPED);

  gbp_project_tree_addin_settings_changed (self, NULL, self->settings);
}
