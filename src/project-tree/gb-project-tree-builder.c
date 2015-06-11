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

static const gchar *
get_icon_name (GFileInfo *file_info)
{
  GFileType file_type;

  g_return_val_if_fail (G_IS_FILE_INFO (file_info), NULL);

  file_type = g_file_info_get_file_type (file_info);

  if (file_type == G_FILE_TYPE_DIRECTORY)
    return "folder-symbolic";

  return "text-x-generic-symbolic";
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

  ide_project_reader_lock (project);

  root = ide_project_get_root (project);
  children = ide_project_item_get_children (root);

  if (children)
    {
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

  ide_project_reader_unlock (project);
}

static gint
sort_files (IdeProjectItem *item_a,
            IdeProjectItem *item_b,
            gpointer        directories_first)
{
  GFileInfo *file_info_a;
  GFileInfo *file_info_b;
  const gchar *display_name_a;
  const gchar *display_name_b;
  g_autofree gchar *casefold_a = NULL;
  g_autofree gchar *casefold_b = NULL;

  file_info_a = ide_project_file_get_file_info (IDE_PROJECT_FILE (item_a));
  file_info_b = ide_project_file_get_file_info (IDE_PROJECT_FILE (item_b));

  if (directories_first)
    {
      GFileType file_type_a;
      GFileType file_type_b;

      file_type_a = g_file_info_get_file_type (file_info_a);
      file_type_b = g_file_info_get_file_type (file_info_b);

      if (file_type_a != file_type_b &&
          (file_type_a == G_FILE_TYPE_DIRECTORY ||
           file_type_b == G_FILE_TYPE_DIRECTORY))
        {
          return file_type_a == G_FILE_TYPE_DIRECTORY ? -1 : +1;
        }
    }

  display_name_a = g_file_info_get_display_name (file_info_a);
  display_name_b = g_file_info_get_display_name (file_info_b);

  casefold_a = g_utf8_collate_key_for_filename (display_name_a, -1);
  casefold_b = g_utf8_collate_key_for_filename (display_name_b, -1);

  return g_utf8_collate (casefold_a, casefold_b);
}

static void
build_files (GbProjectTreeBuilder *self,
             GbTreeNode           *node)
{
  IdeProjectItem *files;
  GSequenceIter *iter;
  GSequence *children;
  gboolean directories_first;

  g_return_if_fail (GB_IS_PROJECT_TREE_BUILDER (self));
  g_return_if_fail (GB_IS_TREE_NODE (node));

  files = IDE_PROJECT_ITEM (gb_tree_node_get_item (node));
  children = ide_project_item_get_children (files);

  if (children)
    {
      directories_first = g_settings_get_boolean (self->file_chooser_settings,
                                                  "sort-directories-first");
      g_sequence_sort (children,
                       (GCompareDataFunc) sort_files,
                       GINT_TO_POINTER (directories_first));

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

  if (IDE_IS_PROJECT (item))
    build_project (self, node);
  else if (IDE_IS_PROJECT_FILES (item) || IDE_IS_PROJECT_FILE (item))
    build_files (self, node);
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
populate_mime_handlers (GMenu          *menu,
                        IdeProjectFile *project_file)
{
  g_autofree gchar *content_type = NULL;
  GList *list;
  GList *iter;
  GFile *file;

  g_assert (G_IS_MENU (menu));
  g_assert (IDE_IS_PROJECT_FILE (project_file));

  g_menu_remove_all (menu);

  file = ide_project_file_get_file (project_file);
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

  g_assert (GB_IS_PROJECT_TREE_BUILDER (builder));
  g_assert (GB_IS_TREE_NODE (node));
  g_assert (G_IS_MENU (menu));

  app = GTK_APPLICATION (g_application_get_default ());
  item = gb_tree_node_get_item (node);

  if (IDE_IS_PROJECT_ITEM (item) || IDE_IS_PROJECT (item))
    {
      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-build");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));
    }

  if (IDE_IS_PROJECT_FILE (item))
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
      populate_mime_handlers (submenu, IDE_PROJECT_FILE (item));

      submenu = gtk_application_get_menu_by_id (app, "gb-project-tree-new");
      g_menu_prepend_section (menu, NULL, G_MENU_MODEL (submenu));
    }
  else if (IDE_IS_PROJECT_FILES (item))
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

  if (IDE_IS_PROJECT_FILE (item))
    {
      GbWorkbench *workbench;
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
  GtkWidget *tree;
  gboolean sort_directories_first;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (GB_IS_PROJECT_TREE_BUILDER (self));

  sort_directories_first = g_settings_get_boolean (settings, "sort-directories-first");

  if (sort_directories_first != self->sort_directories_first)
    {
      self->sort_directories_first = sort_directories_first;
      if ((tree = gb_tree_builder_get_tree (GB_TREE_BUILDER (self))))
        gb_tree_rebuild (GB_TREE (tree));
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
