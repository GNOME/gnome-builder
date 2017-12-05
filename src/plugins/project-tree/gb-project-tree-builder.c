/* gb-project-tree-builder.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
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
#include <ide.h>

#include "gb-project-file.h"
#include "gb-project-tree.h"
#include "gb-project-tree-builder.h"

#define _ATOM gdk_atom_intern_static_string

struct _GbProjectTreeBuilder
{
  DzlTreeBuilder  parent_instance;

  GSettings      *settings;
  GHashTable     *expanded;

  guint           sort_directories_first : 1;
  guint           has_monitor : 1;
};

G_DEFINE_TYPE (GbProjectTreeBuilder, gb_project_tree_builder, DZL_TYPE_TREE_BUILDER)

static gint
compare_nodes_func (DzlTreeNode *a,
                    DzlTreeNode *b,
                    gpointer     user_data)
{
  GbProjectFile *file_a = GB_PROJECT_FILE (dzl_tree_node_get_item (a));
  GbProjectFile *file_b = GB_PROJECT_FILE (dzl_tree_node_get_item (b));
  GbProjectTreeBuilder *self = user_data;

  if (self->sort_directories_first)
    return gb_project_file_compare_directories_first (file_a, file_b);
  else
    return gb_project_file_compare (file_a, file_b);
}

DzlTreeBuilder *
gb_project_tree_builder_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_TREE_BUILDER, NULL);
}

static void
gb_project_tree_builder_add (GbProjectTreeBuilder *self,
                             DzlTreeNode          *parent,
                             GFile                *file)
{
  g_autofree gchar *name = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autoptr(GbProjectFile) item = NULL;
  DzlTreeNode *child;
  const gchar *display_name;
  const gchar *icon_name;
  const gchar *expanded = NULL;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (parent));
  g_assert (G_IS_FILE (file));

  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_STANDARD_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                 G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                 NULL, NULL);
  if (file_info == NULL)
    return;

  item = gb_project_file_new (file, file_info);
  display_name = gb_project_file_get_display_name (item);
  icon_name = gb_project_file_get_icon_name (item);

  if (g_strcmp0 (icon_name, "folder-symbolic") == 0)
    expanded = "folder-open-symbolic";

  child = g_object_new (DZL_TYPE_TREE_NODE,
                        "icon-name", icon_name,
                        "expanded-icon-name", expanded,
                        "text", display_name,
                        "item", item,
                        NULL);

  dzl_tree_node_insert_sorted (parent, child, compare_nodes_func, self);

  if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY)
    {
      dzl_tree_node_set_children_possible (child, TRUE);
      dzl_tree_node_set_reset_on_collapse (child, TRUE);
    }
}

static DzlTreeNode *
gb_project_tree_builder_find_child (GbProjectTreeBuilder *self,
                                    DzlTreeNode          *parent,
                                    GFile                *file)
{
  GtkTreeIter piter;
  GtkTreeIter iter;
  GtkTreeModel *model;
  DzlTree *tree;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (parent));
  g_assert (G_IS_FILE (file));

  if (!dzl_tree_node_get_iter (parent, &piter))
    return NULL;

  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  /* TODO: deal with filter? We don't currently use one. */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));

  if (gtk_tree_model_iter_children (model, &iter, &piter))
    {
      do
        {
          g_autoptr(DzlTreeNode) cur = NULL;
          GObject *item;
          GFile *cur_file;

          gtk_tree_model_get (model, &iter, 0, &cur, -1);

          item = dzl_tree_node_get_item (cur);
          if (!GB_IS_PROJECT_FILE (item))
            continue;

          cur_file = gb_project_file_get_file (GB_PROJECT_FILE (item));
          if (!G_IS_FILE (cur_file))
            continue;

          if (g_file_equal (cur_file, file))
            return g_steal_pointer (&cur);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return NULL;
}

static void
gb_project_tree_builder_remove (GbProjectTreeBuilder *self,
                                DzlTreeNode          *parent,
                                GFile                *file)
{
  g_autoptr(DzlTreeNode) child = NULL;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (parent));
  g_assert (G_IS_FILE (file));

  child = gb_project_tree_builder_find_child (self, parent, file);
  if (child != NULL)
    dzl_tree_node_remove (parent, child);
}

static void
gb_project_tree_builder_changed (GbProjectTreeBuilder    *self,
                                 GFile                   *file,
                                 GFile                   *other_file,
                                 GFileMonitorEvent        event,
                                 DzlRecursiveFileMonitor *monitor)
{
  g_assert (GB_PROJECT_TREE_BUILDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (!other_file || G_IS_FILE (other_file));
  g_assert (DZL_IS_RECURSIVE_FILE_MONITOR (monitor));

  if (FALSE) {}
  else if (event == G_FILE_MONITOR_EVENT_CREATED)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);
      DzlTreeNode *node = g_hash_table_lookup (self->expanded, parent);

      if (node != NULL)
        {
          g_autoptr(DzlTreeNode) child = NULL;

          child = gb_project_tree_builder_find_child (self, node, file);
          if (child == NULL)
            gb_project_tree_builder_add (self, node, file);
        }
    }
  else if (event == G_FILE_MONITOR_EVENT_DELETED)
    {
      g_autoptr(GFile) parent = g_file_get_parent (file);
      DzlTreeNode *node = g_hash_table_lookup (self->expanded, parent);

      if (node != NULL)
        gb_project_tree_builder_remove (self, node, file);
    }
}

static void
build_context (GbProjectTreeBuilder *self,
               DzlTreeNode          *node)
{
  g_autoptr(GbProjectFile) item = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autofree gchar *name = NULL;
  DzlTreeNode *child;
  IdeContext *context;
  IdeProject *project;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (DZL_IS_TREE_NODE (node));

  context = IDE_CONTEXT (dzl_tree_node_get_item (node));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  project = ide_context_get_project (context);

  if (!self->has_monitor)
    {
      DzlRecursiveFileMonitor *monitor = ide_context_get_monitor (context);

      self->has_monitor = TRUE;

      g_signal_connect_object (monitor,
                               "changed",
                               G_CALLBACK (gb_project_tree_builder_changed),
                               self,
                               G_CONNECT_SWAPPED);
    }

  file_info = g_file_info_new ();

  g_file_info_set_file_type (file_info, G_FILE_TYPE_DIRECTORY);

  name = g_file_get_basename (workdir);
  g_file_info_set_name (file_info, name);
  g_file_info_set_display_name (file_info, name);

  item = g_object_new (GB_TYPE_PROJECT_FILE,
                       "file", workdir,
                       "file-info", file_info,
                       NULL);

  child = g_object_new (DZL_TYPE_TREE_NODE,
                        "item", item,
                        "icon-name", "folder-symbolic",
                        "expanded-icon-name", "folder-open-symbolic",
                        "reset-on-collapse", TRUE,
                        NULL);
  g_object_bind_property (project, "name", child, "text", G_BINDING_SYNC_CREATE);
  dzl_tree_node_append (node, child);
}

static IdeVcs *
get_vcs (DzlTreeNode *node)
{
  DzlTree *tree;
  DzlTreeNode *root;
  IdeContext *context;

  g_assert (DZL_IS_TREE_NODE (node));

  tree = dzl_tree_node_get_tree (node);
  root = dzl_tree_get_root (tree);
  context = IDE_CONTEXT (dzl_tree_node_get_item (root));

  return ide_context_get_vcs (context);
}

static void
build_file (GbProjectTreeBuilder *self,
            DzlTreeNode          *node)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GbProjectFile *project_file;
  gpointer file_info_ptr;
  IdeVcs *vcs;
  GFile *file;
  DzlTree *tree;
  gint count = 0;
  gboolean show_ignored_files;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (DZL_IS_TREE_NODE (node));

  project_file = GB_PROJECT_FILE (dzl_tree_node_get_item (node));

  tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self));
  show_ignored_files = gb_project_tree_get_show_ignored_files (GB_PROJECT_TREE (tree));

  vcs = get_vcs (node);

  /*
   * TODO: Make this all async.
   */

  if (!gb_project_file_get_is_directory (project_file))
    return;

  file = gb_project_file_get_file (project_file);

  enumerator = g_file_enumerate_children (file,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          NULL,
                                          NULL);

  if (enumerator == NULL)
    return;

  while ((file_info_ptr = g_file_enumerator_next_file (enumerator, NULL, NULL)))
    {
      g_autoptr(GFileInfo) item_file_info = file_info_ptr;
      g_autoptr(GFile) item_file = NULL;
      g_autoptr(GbProjectFile) item = NULL;
      DzlTreeNode *child;
      const gchar *name;
      const gchar *display_name;
      const gchar *icon_name;
      const gchar *expanded = NULL;
      gboolean ignored;

      name = g_file_info_get_name (item_file_info);
      item_file = g_file_get_child (file, name);

      ignored = ide_vcs_is_ignored (vcs, item_file, NULL);
      if (ignored && !show_ignored_files)
        continue;

      item = gb_project_file_new (item_file, item_file_info);

      display_name = gb_project_file_get_display_name (item);
      icon_name = gb_project_file_get_icon_name (item);

      if (g_strcmp0 (icon_name, "folder-symbolic") == 0)
        expanded = "folder-open-symbolic";

      child = g_object_new (DZL_TYPE_TREE_NODE,
                            "icon-name", icon_name,
                            "expanded-icon-name", expanded,
                            "text", display_name,
                            "item", item,
                            "use-dim-label", ignored,
                            NULL);

      dzl_tree_node_insert_sorted (node, child, compare_nodes_func, self);

      if (g_file_info_get_file_type (item_file_info) == G_FILE_TYPE_DIRECTORY)
        {
          dzl_tree_node_set_children_possible (child, TRUE);
          dzl_tree_node_set_reset_on_collapse (child, TRUE);
        }

      count++;
    }

  /*
   * If we didn't add any children to this node, insert an empty node to
   * notify the user that nothing was found.
   */
  if (count == 0)
    {
      DzlTreeNode *child;

      child = g_object_new (DZL_TYPE_TREE_NODE,
                            "icon-name", NULL,
                            "text", _("Empty"),
                            "use-dim-label", TRUE,
                            NULL);
      dzl_tree_node_append (node, child);
    }
}

static void
gb_project_tree_builder_build_node (DzlTreeBuilder *builder,
                                    DzlTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));

  item = dzl_tree_node_get_item (node);

  if (IDE_IS_CONTEXT (item))
    build_context (self, node);
  else if (GB_IS_PROJECT_FILE (item))
    build_file (self, node);
}

static gchar *
get_content_type (GFile *file)
{
  g_autofree gchar *name = NULL;

  g_assert (G_IS_FILE (file));

  name = g_file_get_basename (file);

  return g_content_type_guess (name, NULL, 0, NULL);
}

static void
populate_mime_handlers (GMenu         *menu,
                        GbProjectFile *project_file)
{
  g_autofree gchar *content_type = NULL;
  GList *list;
  GList *iter;
  GFile *file;

  g_assert (G_IS_MENU (menu));
  g_assert (GB_IS_PROJECT_FILE (project_file));

  g_menu_remove_all (menu);

  file = gb_project_file_get_file (project_file);
  if (file == NULL)
    return;

  content_type = get_content_type (file);
  if (content_type == NULL)
    return;

  list = g_app_info_get_all_for_type (content_type);

  for (iter = list; iter; iter = iter->next)
    {
      g_autoptr(GMenuItem) menu_item = NULL;
      g_autofree gchar *detailed_action = NULL;
      GAppInfo *app_info = iter->data;
      const gchar *display_name;
      const gchar *app_id;

      display_name = g_app_info_get_display_name (app_info);
      app_id = g_app_info_get_id (app_info);

      detailed_action = g_strdup_printf ("project-tree.open-with('%s')", app_id);
      menu_item = g_menu_item_new (display_name, detailed_action);

      g_menu_append_item (menu, menu_item);
    }

  g_list_free_full (list, g_object_unref);
}

static void
gb_project_tree_builder_node_popup (DzlTreeBuilder *builder,
                                    DzlTreeNode    *node,
                                    GMenu          *menu)
{
  GObject *item;
  IdeVcs *vcs;
  GFile *workdir;
  GFile *file;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (builder));
  g_assert (DZL_IS_TREE_NODE (node));
  g_assert (G_IS_MENU (menu));

  item = dzl_tree_node_get_item (node);
  vcs = get_vcs (node);
  workdir = ide_vcs_get_working_directory (vcs);

  if (GB_IS_PROJECT_FILE (item) &&
      (file = gb_project_file_get_file (GB_PROJECT_FILE (item))) &&
      !g_file_equal (file, workdir))
    {
      GMenu *mime_section;

      mime_section = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT,
                                                     "gb-project-tree-open-by-mime-section");
      populate_mime_handlers (mime_section, GB_PROJECT_FILE (item));
    }
}

static gboolean
gb_project_tree_builder_node_activated (DzlTreeBuilder *builder,
                                        DzlTreeNode    *node)
{
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (builder));

  item = dzl_tree_node_get_item (node);

  if (GB_IS_PROJECT_FILE (item))
    {
      GtkWidget *workbench;
      DzlTree *tree;
      GFile *file;

      if (gb_project_file_get_is_directory (GB_PROJECT_FILE (item)))
        goto failure;

      file = gb_project_file_get_file (GB_PROJECT_FILE (item));
      if (!file)
        goto failure;

      tree = dzl_tree_node_get_tree (node);
      if (!tree)
        goto failure;

      workbench = gtk_widget_get_ancestor (GTK_WIDGET (tree), IDE_TYPE_WORKBENCH);
      ide_workbench_open_files_async (IDE_WORKBENCH (workbench),
                                      &file,
                                      1,
                                      NULL,
                                      IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                      NULL,
                                      NULL,
                                      NULL);

      return TRUE;
    }

failure:
  return FALSE;
}

static void
gb_project_tree_builder_rebuild (GSettings            *settings,
                                 const gchar          *key,
                                 GbProjectTreeBuilder *self)
{
  DzlTree *tree;
  gboolean sort_directories_first;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));

  sort_directories_first = g_settings_get_boolean (settings, "sort-directories-first");

  if (sort_directories_first != self->sort_directories_first)
    {
      self->sort_directories_first = sort_directories_first;
      if ((tree = dzl_tree_builder_get_tree (DZL_TREE_BUILDER (self))))
        dzl_tree_rebuild (tree);
    }
}

static void
gb_project_tree_builder_node_expanded (DzlTreeBuilder *builder,
                                       DzlTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;
  GFile *file;

  IDE_ENTRY;

  g_assert (DZL_IS_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);
  if (!GB_IS_PROJECT_FILE (item))
    IDE_EXIT;

  file = gb_project_file_get_file (GB_PROJECT_FILE (item));
  if (!G_IS_FILE (file))
    IDE_EXIT;

  g_hash_table_insert (self->expanded,
                       g_object_ref (file),
                       g_object_ref (node));

  IDE_EXIT;
}

static void
gb_project_tree_builder_node_collapsed (DzlTreeBuilder *builder,
                                        DzlTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;
  GFile *file;
  GHashTableIter iter;
  gpointer key, value;

  IDE_ENTRY;

  g_assert (DZL_IS_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);
  if (!GB_IS_PROJECT_FILE (item))
    return;

  file = gb_project_file_get_file (GB_PROJECT_FILE (item));
  if (!G_IS_FILE (file))
    return;

  /*
   * Stop tracking all nodes from this directory on down. We have to
   * iterate the hashtable so that we can keep our changed handler fast
   * with a direct file lookup.
   */

  g_hash_table_iter_init (&iter, self->expanded);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GFile *dir = key;
      DzlTreeNode *dir_node = value;

      if (g_file_has_prefix (dir, file) || g_file_equal (dir, file))
        {
          g_hash_table_iter_steal (&iter);
          g_object_unref (dir);
          g_object_unref (dir_node);
        }
    }

  IDE_EXIT;
}

static gboolean
gb_project_tree_builder_node_draggable (DzlTreeBuilder *builder,
                                        DzlTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);

  return GB_IS_PROJECT_FILE (item);
}

static gboolean
gb_project_tree_builder_node_droppable (DzlTreeBuilder   *builder,
                                        DzlTreeNode      *node,
                                        GtkSelectionData *data)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  item = dzl_tree_node_get_item (node);

  return GB_IS_PROJECT_FILE (item);
}

static gboolean
gb_project_tree_builder_drag_data_get (DzlTreeBuilder   *builder,
                                       DzlTreeNode      *node,
                                       GtkSelectionData *data)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  if (gtk_selection_data_get_target (data) != _ATOM ("text/uri-list"))
    return FALSE;

  item = dzl_tree_node_get_item (node);

  if (GB_IS_PROJECT_FILE (item))
    {
      GFile *file = gb_project_file_get_file (GB_PROJECT_FILE (item));
      g_autofree gchar *uri = g_file_get_uri (file);
      gchar *uris[] = { uri, NULL };

      return gtk_selection_data_set_uris (data, uris);
    }

  return FALSE;
}

static gboolean
gb_project_tree_builder_drag_data_received (DzlTreeBuilder      *builder,
                                            DzlTreeNode         *drop_node,
                                            DzlTreeDropPosition  position,
                                            GtkSelectionData    *data)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (drop_node));
  g_assert (data != NULL);

  /* We don't care about positioning before/after for the file tree.
   * So if we get one of those, we will just ignore it and look at
   * the parent instead.
   */
  if (position != DZL_TREE_DROP_INTO)
    {
      if (NULL == (drop_node = dzl_tree_node_get_parent (drop_node)) ||
          dzl_tree_node_is_root (drop_node))
        return FALSE;
    }

  /* For inter-process DnD, we only support dropping a URI list.
   * We need to copy all those files into the directory represented
   * by our drop node.
   */
  if (gtk_selection_data_get_target (data) == _ATOM ("text/uri-list"))
    {
      GObject *item = dzl_tree_node_get_item (drop_node);

      if (GB_IS_PROJECT_FILE (item))
        {
          GFile *file = gb_project_file_get_file (GB_PROJECT_FILE (item));
          g_auto(GStrv) uris = gtk_selection_data_get_uris (data);

          if (uris != NULL && uris[0] != NULL)
            {
              g_autofree gchar *joined = g_strjoinv (" ", uris);
              g_autofree gchar *dst_uri = g_file_get_uri (file);

              g_debug ("Drop %s onto %s with position %d",
                       joined, dst_uri, position);

              return TRUE;
            }
        }
    }

  return FALSE;
}

static gboolean
gb_project_tree_builder_drag_node_received (DzlTreeBuilder      *builder,
                                            DzlTreeNode         *drag_node,
                                            DzlTreeNode         *drop_node,
                                            DzlTreeDropPosition  position,
                                            GtkSelectionData    *data)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;
  GObject *drag_item;
  GObject *drop_item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (drag_node));
  g_assert (DZL_IS_TREE_NODE (drop_node));
  g_assert (data != NULL);

  /* We don't care about positioning before/after for the file tree.
   * So if we get one of those, we will just ignore it and look at
   * the parent instead.
   */
  if (position != DZL_TREE_DROP_INTO)
    {
      if (NULL == (drop_node = dzl_tree_node_get_parent (drop_node)) ||
          dzl_tree_node_is_root (drop_node))
        return FALSE;
    }

  /*
   * Get our files and determine what we are copying to the new location.
   * We always do a copy because the drag_delete signal will be used to
   * remove the old files. Not exactly the most efficient, but it is fine
   * for our purposes.
   */
  drag_item = dzl_tree_node_get_item (drag_node);
  drop_item = dzl_tree_node_get_item (drop_node);

  if (GB_IS_PROJECT_FILE (drag_item) && GB_IS_PROJECT_FILE (drop_item))
    {
      GFile *drag_file = gb_project_file_get_file (GB_PROJECT_FILE (drag_item));
      GFile *drop_file = gb_project_file_get_file (GB_PROJECT_FILE (drop_item));

      if (G_IS_FILE (drag_file) && G_IS_FILE (drop_file))
        {
          g_autofree gchar *src_uri = g_file_get_uri (drag_file);
          g_autofree gchar *dst_uri = g_file_get_uri (drop_file);

          g_debug ("Need to copy %s into %s", src_uri, dst_uri);

          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gb_project_tree_builder_drag_node_delete (DzlTreeBuilder *builder,
                                          DzlTreeNode    *node)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)builder;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));
  g_assert (DZL_IS_TREE_NODE (node));

  /*
   * We must have done a GTK_ACTION_MOVE, which means that we need to
   * cleanup whatever it is that we moved. In our case, that means
   * removing the files.
   */

  return FALSE;
}

static void
gb_project_tree_builder_dispose (GObject *object)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)object;

  g_clear_pointer (&self->expanded, g_hash_table_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gb_project_tree_builder_parent_class)->dispose (object);
}

static void
gb_project_tree_builder_class_init (GbProjectTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlTreeBuilderClass *tree_builder_class = DZL_TREE_BUILDER_CLASS (klass);

  object_class->dispose = gb_project_tree_builder_dispose;

  tree_builder_class->build_node = gb_project_tree_builder_build_node;
  tree_builder_class->drag_data_get = gb_project_tree_builder_drag_data_get;
  tree_builder_class->drag_data_received = gb_project_tree_builder_drag_data_received;
  tree_builder_class->drag_node_received = gb_project_tree_builder_drag_node_received;
  tree_builder_class->drag_node_delete = gb_project_tree_builder_drag_node_delete;
  tree_builder_class->node_activated = gb_project_tree_builder_node_activated;
  tree_builder_class->node_collapsed = gb_project_tree_builder_node_collapsed;
  tree_builder_class->node_expanded = gb_project_tree_builder_node_expanded;
  tree_builder_class->node_popup = gb_project_tree_builder_node_popup;
  tree_builder_class->node_draggable = gb_project_tree_builder_node_draggable;
  tree_builder_class->node_droppable = gb_project_tree_builder_node_droppable;
}

static void
gb_project_tree_builder_init (GbProjectTreeBuilder *self)
{
  self->settings = g_settings_new ("org.gnome.builder.project-tree");
  self->sort_directories_first = g_settings_get_boolean (self->settings, "sort-directories-first");
  self->expanded = g_hash_table_new_full (g_file_hash,
                                          (GEqualFunc) g_file_equal,
                                          g_object_unref,
                                          g_object_unref);

  g_signal_connect_object (self->settings,
                           "changed::sort-directories-first",
                           G_CALLBACK (gb_project_tree_builder_rebuild),
                           self,
                           0);
}
