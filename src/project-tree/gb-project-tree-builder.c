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

#include "gb-project-tree-builder.h"
#include "gb-project-file.h"
#include "gb-tree.h"
#include "gb-widget.h"
#include "gb-workbench.h"

struct _GbProjectTreeBuilder
{
  GbTreeBuilder  parent_instance;

  GSettings     *file_chooser_settings;

  guint          sort_directories_first : 1;
};

G_DEFINE_TYPE (GbProjectTreeBuilder, gb_project_tree_builder, GB_TYPE_TREE_BUILDER)

GbTreeBuilder *
gb_project_tree_builder_new (void)
{
  return g_object_new (GB_TYPE_PROJECT_TREE_BUILDER, NULL);
}

static void
build_context (GbProjectTreeBuilder *self,
               GbTreeNode           *node)
{
  g_autoptr(GbProjectFile) item = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autofree gchar *name = NULL;
  GbTreeNode *child;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  context = IDE_CONTEXT (gb_tree_node_get_item (node));
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  file_info = g_file_info_new ();

  g_file_info_set_file_type (file_info, G_FILE_TYPE_DIRECTORY);

  name = g_file_get_basename (workdir);
  g_file_info_set_name (file_info, name);
  g_file_info_set_display_name (file_info, name);

  item = g_object_new (GB_TYPE_PROJECT_FILE,
                       "file", workdir,
                       "file-info", file_info,
                       NULL);

  child = g_object_new (GB_TYPE_TREE_NODE,
                        "item", item,
                        "text", _("Files"),
                        "icon-name", "folder-symbolic",
                        NULL);
  gb_tree_node_append (node, child);
}

static IdeVcs *
get_vcs (GbTreeNode *node)
{
  GbTree *tree;
  GbTreeNode *root;
  IdeContext *context;

  g_assert (GB_IS_TREE_NODE (node));

  tree = gb_tree_node_get_tree (node);
  root = gb_tree_get_root (tree);
  context = IDE_CONTEXT (gb_tree_node_get_item (root));

  return ide_context_get_vcs (context);
}

static gint
compare_nodes_func (GbTreeNode *a,
                    GbTreeNode *b,
                    gpointer    user_data)
{
  GbProjectFile *file_a = GB_PROJECT_FILE (gb_tree_node_get_item (a));
  GbProjectFile *file_b = GB_PROJECT_FILE (gb_tree_node_get_item (b));
  GbProjectTreeBuilder *self = user_data;

  if (self->sort_directories_first)
    return gb_project_file_compare_directories_first (file_a, file_b);
  else
    return gb_project_file_compare (file_a, file_b);
}

static void
build_file (GbProjectTreeBuilder *self,
            GbTreeNode           *node)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  GbProjectFile *project_file;
  gpointer file_info_ptr;
  IdeVcs *vcs;
  GFile *file;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  project_file = GB_PROJECT_FILE (gb_tree_node_get_item (node));

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
      GbTreeNode *child;
      const gchar *name;
      const gchar *display_name;
      const gchar *icon_name;

      name = g_file_info_get_name (item_file_info);
      item_file = g_file_get_child (file, name);

      if (ide_vcs_is_ignored (vcs, item_file, NULL))
        continue;

      item = gb_project_file_new (item_file, item_file_info);

      display_name = gb_project_file_get_display_name (item);
      icon_name = gb_project_file_get_icon_name (item);

      child = g_object_new (GB_TYPE_TREE_NODE,
                            "icon-name", icon_name,
                            "text", display_name,
                            "item", item,
                            NULL);

      gb_tree_node_insert_sorted (node, child, compare_nodes_func, self);

      if (g_file_info_get_file_type (item_file_info) == G_FILE_TYPE_DIRECTORY)
        gb_tree_node_set_children_possible (child, TRUE);
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
gb_project_tree_builder_node_popup (GbTreeBuilder *builder,
                                    GbTreeNode    *node,
                                    GMenu         *menu)
{
  GtkApplication *app;
  GObject *item;
  GMenu *submenu;
  IdeVcs *vcs;
  GFile *workdir;
  GFile *file;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (builder));
  g_assert (GB_IS_TREE_NODE (node));
  g_assert (G_IS_MENU (menu));

  app = GTK_APPLICATION (g_application_get_default ());
  item = gb_tree_node_get_item (node);

  if (GB_IS_PROJECT_FILE (item))
    {
      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-build");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));
    }

  vcs = get_vcs (node);
  workdir = ide_vcs_get_working_directory (vcs);

  if (GB_IS_PROJECT_FILE (item) &&
      (file = gb_project_file_get_file (GB_PROJECT_FILE (item))) &&
      !g_file_equal (file, workdir))
    {
      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-move-to-trash");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-rename");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-open-containing");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-open");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-open-by-mime-section");
      populate_mime_handlers (submenu, GB_PROJECT_FILE (item));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-new");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));
    }
  else if (GB_IS_PROJECT_FILE (item))
    {
      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-open-containing");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-new");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));
    }

  submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-display-options");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (submenu));
}

static gboolean
gb_project_tree_builder_node_activated (GbTreeBuilder *builder,
                                        GbTreeNode    *node)
{
  GObject *item;

  g_assert (GB_IS_PROJECT_TREE_BUILDER (builder));

  item = gb_tree_node_get_item (node);

  if (GB_IS_PROJECT_FILE (item))
    {
      GbWorkbench *workbench;
      GbTree *tree;
      GFile *file;

      if (gb_project_file_get_is_directory (GB_PROJECT_FILE (item)))
        goto failure;

      file = gb_project_file_get_file (GB_PROJECT_FILE (item));
      if (!file)
        goto failure;

      tree = gb_tree_node_get_tree (node);
      if (!tree)
        goto failure;

      workbench = gb_widget_get_workbench (GTK_WIDGET (tree));
      gb_workbench_open (workbench, file);

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
  GbTree *tree;
  gboolean sort_directories_first;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));

  sort_directories_first = g_settings_get_boolean (settings, "sort-directories-first");

  if (sort_directories_first != self->sort_directories_first)
    {
      self->sort_directories_first = sort_directories_first;
      if ((tree = gb_tree_builder_get_tree (GB_TREE_BUILDER (self))))
        gb_tree_rebuild (tree);
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
  GbTreeBuilderClass *tree_builder_class = GB_TREE_BUILDER_CLASS (klass);

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

  g_signal_connect (self->file_chooser_settings,
                    "changed::sort-directories-first",
                    G_CALLBACK (gb_project_tree_builder_rebuild),
                    self);
}
