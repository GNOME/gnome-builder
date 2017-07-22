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
#include <ide.h>

#include "gb-project-file.h"
#include "gb-project-tree.h"
#include "gb-project-tree-builder.h"

struct _GbProjectTreeBuilder
{
  DzlTreeBuilder  parent_instance;

  GSettings      *file_chooser_settings;

  guint           sort_directories_first : 1;
};

G_DEFINE_TYPE (GbProjectTreeBuilder, gb_project_tree_builder, DZL_TYPE_TREE_BUILDER)

DzlTreeBuilder *
gb_project_tree_builder_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_TREE_BUILDER, NULL);
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
                                          G_FILE_QUERY_INFO_NONE,
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
        dzl_tree_node_set_children_possible (child, TRUE);

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
gb_project_tree_builder_finalize (GObject *object)
{
  GbProjectTreeBuilder *self = (GbProjectTreeBuilder *)object;

  g_clear_object (&self->file_chooser_settings);

  G_OBJECT_CLASS (gb_project_tree_builder_parent_class)->finalize (object);
}

static void
gb_project_tree_builder_class_init (GbProjectTreeBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  DzlTreeBuilderClass *tree_builder_class = DZL_TREE_BUILDER_CLASS (klass);

  object_class->finalize = gb_project_tree_builder_finalize;

  tree_builder_class->build_node = gb_project_tree_builder_build_node;
  tree_builder_class->node_activated = gb_project_tree_builder_node_activated;
  tree_builder_class->node_popup = gb_project_tree_builder_node_popup;
}

static void
gb_project_tree_builder_init (GbProjectTreeBuilder *self)
{
  self->file_chooser_settings = g_settings_new ("org.gtk.Settings.FileChooser");
  self->sort_directories_first = g_settings_get_boolean (self->file_chooser_settings,
                                                         "sort-directories-first");

  g_signal_connect_object (self->file_chooser_settings,
                           "changed::sort-directories-first",
                           G_CALLBACK (gb_project_tree_builder_rebuild),
                           self,
                           0);
}
