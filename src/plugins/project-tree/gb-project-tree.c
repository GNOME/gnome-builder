/* gb-project-tree.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
#include <ide.h>

#include "gb-project-file.h"
#include "gb-project-tree.h"
#include "gb-project-tree-actions.h"
#include "gb-project-tree-builder.h"
#include "gb-project-tree-private.h"
#include "gb-vcs-tree-builder.h"

G_DEFINE_TYPE (GbProjectTree, gb_project_tree, DZL_TYPE_TREE)

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
  DzlTreeNode *root;
  GObject *item;

  g_return_val_if_fail (GB_IS_PROJECT_TREE (self), NULL);

  if ((root = dzl_tree_get_root (DZL_TREE (self))) &&
      (item = dzl_tree_node_get_item (root)) &&
      IDE_IS_CONTEXT (item))
    return IDE_CONTEXT (item);

  return NULL;
}

static void
gb_project_tree_project_file_renamed (GbProjectTree *self,
                                      GFile         *src_file,
                                      GFile         *dst_file,
                                      IdeProject    *project)
{
  IDE_ENTRY;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (G_IS_FILE (src_file));
  g_assert (G_IS_FILE (dst_file));
  g_assert (IDE_IS_PROJECT (project));

  dzl_tree_rebuild (DZL_TREE (self));
  gb_project_tree_reveal (self, dst_file, FALSE, FALSE);

  IDE_EXIT;
}

static gboolean
compare_to_file (gconstpointer a,
                 gconstpointer b)
{
  GFile *file = (GFile *)a;
  GObject *item = (GObject *)b;

  /*
   * Our key (the GFile) is always @a.
   * The potential match (maybe a GbProjectFile) is @b.
   * @b may also be NULL.
   */

  g_assert (G_IS_FILE (file));
  g_assert (!item || G_IS_OBJECT (item));

  if (GB_IS_PROJECT_FILE (item))
    return g_file_equal (file, gb_project_file_get_file (GB_PROJECT_FILE (item)));

  return FALSE;
}

static void
gb_project_tree_project_file_trashed (GbProjectTree *self,
                                      GFile         *file,
                                      IdeProject    *project)
{
  DzlTreeNode *node;

  IDE_ENTRY;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PROJECT (project));

  node = dzl_tree_find_custom (DZL_TREE (self), compare_to_file, file);

  if (node != NULL)
    {
      DzlTreeNode *parent = dzl_tree_node_get_parent (node);

      dzl_tree_node_invalidate (parent);
      dzl_tree_node_expand (parent, TRUE);
      dzl_tree_node_select (parent);
    }

  IDE_EXIT;
}

static void
gb_project_tree_vcs_changed (GbProjectTree *self,
                             IdeVcs        *vcs)
{
  GActionGroup *group;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (IDE_IS_VCS (vcs));

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "project-tree");
  if (group != NULL)
    g_action_group_activate_action (group, "refresh", NULL);
}

static void
gb_project_tree_buffer_saved_cb (GbProjectTree    *self,
                                 IdeBuffer        *buffer,
                                 IdeBufferManager *buffer_manager)
{
  IdeContext *context;
  IdeVcs *vcs;
  DzlTreeNode *node;
  IdeFile *ifile;
  GFile *gfile;
  GFile *workdir;

  g_assert (GB_IS_PROJECT_TREE (self));
  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (IDE_IS_BUFFER (buffer));

  ifile = ide_buffer_get_file (buffer);
  gfile = ide_file_get_file (ifile);

  context = gb_project_tree_get_context (self);
  vcs = ide_context_get_vcs (context);
  if (NULL != (workdir = ide_vcs_get_working_directory (vcs)) &&
      g_file_has_prefix (gfile, workdir))
    {
      if (NULL == (node = dzl_tree_find_custom (DZL_TREE (self), compare_to_file, gfile)))
        dzl_tree_rebuild (DZL_TREE (self));

      gb_project_tree_reveal (self, gfile, FALSE, FALSE);
    }
}

void
gb_project_tree_set_context (GbProjectTree *self,
                             IdeContext    *context)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  DzlTreeNode *root;
  IdeProject *project;
  IdeBufferManager *buffer_manager;
  IdeVcs *vcs;

  g_return_if_fail (GB_IS_PROJECT_TREE (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));

  vcs = ide_context_get_vcs (context);

  g_signal_connect_object (vcs,
                           "changed",
                           G_CALLBACK (gb_project_tree_vcs_changed),
                           self,
                           G_CONNECT_SWAPPED);

  project = ide_context_get_project (context);

  g_signal_connect_object (project,
                           "file-renamed",
                           G_CALLBACK (gb_project_tree_project_file_renamed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (project,
                           "file-trashed",
                           G_CALLBACK (gb_project_tree_project_file_trashed),
                           self,
                           G_CONNECT_SWAPPED);

  buffer_manager = ide_context_get_buffer_manager (context);
  g_signal_connect_object (buffer_manager,
                           "buffer-saved",
                           G_CALLBACK (gb_project_tree_buffer_saved_cb),
                           self,
                           G_CONNECT_SWAPPED);

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self));

  root = dzl_tree_node_new ();
  dzl_tree_node_set_item (root, G_OBJECT (context));
  dzl_tree_set_root (DZL_TREE (self), root);

  /*
   * If we only have one toplevel item (underneath root), expand it.
   */
  if ((gtk_tree_model_iter_n_children (model, NULL) == 1) &&
      gtk_tree_model_get_iter_first (model, &iter))
    {
      g_autoptr(DzlTreeNode) node = NULL;

      gtk_tree_model_get (model, &iter, 0, &node, -1);
      if (node != NULL)
        dzl_tree_node_expand (node, FALSE);
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
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gb_project_tree_init (GbProjectTree *self)
{
  static const GtkTargetEntry drag_targets[] = {
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 },
    { "text/uri-list", 0, 0 },
  };
  DzlTreeBuilder *builder;
  GMenu *menu;

  dzl_gtk_widget_add_style_class (GTK_WIDGET (self), "project-tree");
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (self), TRUE);

  self->settings = g_settings_new ("org.gnome.builder.project-tree");

  g_settings_bind (self->settings, "show-icons", self, "show-icons",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "show-ignored-files", self, "show-ignored-files",
                   G_SETTINGS_BIND_DEFAULT);

  builder = gb_project_tree_builder_new ();
  dzl_tree_add_builder (DZL_TREE (self), builder);

  builder = gb_vcs_tree_builder_new ();
  dzl_tree_add_builder (DZL_TREE (self), builder);

  g_signal_connect (self,
                    "notify::selection",
                    G_CALLBACK (gb_project_tree_notify_selection),
                    NULL);

  gb_project_tree_actions_init (self);
  _gb_project_tree_init_shortcuts (self);

  menu = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT,
                                         "gb-project-tree-popup-menu");
  dzl_tree_set_context_menu (DZL_TREE (self), G_MENU_MODEL (menu));

  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (self),
                                          GDK_BUTTON1_MASK,
                                          drag_targets, G_N_ELEMENTS (drag_targets),
                                          GDK_ACTION_COPY | GDK_ACTION_MOVE);
  gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (self),
                                        drag_targets, G_N_ELEMENTS (drag_targets),
                                        GDK_ACTION_COPY | GDK_ACTION_MOVE);
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
      dzl_tree_rebuild (DZL_TREE (self));
    }
}

static gboolean
find_child_node (DzlTree     *tree,
                 DzlTreeNode *node,
                 DzlTreeNode *child,
                 gpointer    user_data)
{
  const gchar *name = user_data;
  GObject *item;

  g_assert (DZL_IS_TREE (tree));
  g_assert (DZL_IS_TREE_NODE (node));
  g_assert (DZL_IS_TREE_NODE (child));

  item = dzl_tree_node_get_item (child);

  if (GB_IS_PROJECT_FILE (item))
    {
      const gchar *item_name;

      item_name = gb_project_file_get_display_name (GB_PROJECT_FILE (item));

      return dzl_str_equal0 (item_name, name);
    }

  return FALSE;
}

static gboolean
find_files_node (DzlTree     *tree,
                 DzlTreeNode *node,
                 DzlTreeNode *child,
                 gpointer    user_data)
{
  GObject *item;

  g_assert (DZL_IS_TREE (tree));
  g_assert (DZL_IS_TREE_NODE (node));
  g_assert (DZL_IS_TREE_NODE (child));

  item = dzl_tree_node_get_item (child);

  return GB_IS_PROJECT_FILE (item);
}

/**
 * gb_project_tree_find_file_node:
 * @self: a #GbProjectTree
 * @file: a #GFile
 *
 * Tries to locate the #DzlTreeNode that contains #GFile.
 *
 * If the node does not exist, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): A #DzlTreeNode or %NULL.
 *
 * Since: 3.28
 */
DzlTreeNode *
gb_project_tree_find_file_node (GbProjectTree *self,
                                GFile         *file)
{
  g_autofree gchar *relpath = NULL;
  g_auto(GStrv) parts = NULL;
  DzlTreeNode *node;
  DzlTreeNode *last_node;
  IdeContext *context;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_val_if_fail (GB_IS_PROJECT_TREE (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  context = gb_project_tree_get_context (self);
  if (context == NULL)
    return NULL;

  g_assert (IDE_IS_CONTEXT (context));

  node = dzl_tree_find_child_node (DZL_TREE (self), NULL, find_files_node, NULL);
  if (node == NULL)
    return NULL;

  g_assert (DZL_IS_TREE_NODE (node));

  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  if (!g_file_has_prefix (file, workdir))
    return NULL;

  relpath = g_file_get_relative_path (workdir, file);
  if (relpath == NULL)
    return NULL;

  parts = g_strsplit (relpath, G_DIR_SEPARATOR_S, 0);

  last_node = node;
  for (guint i = 0; parts[i] != NULL; i++)
    {
      /*
       * Only scan children if we know they exist, otherwise we risk
       * creating the children nodes as part of the scan. (Which we are
       * explicitely trying to avoid.
       */
      if (dzl_tree_node_n_children (node) == 0)
        return NULL;

      /*
       * Scan the children for this particular path part. If we don't
       * locate it, we can short cirtcuit immediately.
       */
      node = dzl_tree_find_child_node (DZL_TREE (self), node, find_child_node, parts[i]);
      if (node == NULL)
        return NULL;

      last_node = node;
    }

  return g_object_ref (last_node);
}

/**
 * gb_project_tree_reveal:
 * @self: a #GbProjectTree
 * @file: the #GFile to reveal
 * @focus_tree_view: whether to focus the tree
 * @expand_folder: whether the given file should be expanded (if it's a folder)
 *
 * Expand the tree so the node for the specified file is visible and selected.
 * In the case that the file has been deleted, expand the tree as far as possible.
 */
void
gb_project_tree_reveal (GbProjectTree *self,
                        GFile         *file,
                        gboolean       focus_tree_view,
                        gboolean       expand_folder)
{
  g_autofree gchar *relpath = NULL;
  g_auto(GStrv) parts = NULL;
  IdeContext *context;
  DzlTreeNode *node = NULL;
  DzlTreeNode *last_node = NULL;
  IdeVcs *vcs;
  GFile *workdir;
  gboolean reveal_parent = FALSE;

  g_return_if_fail (GB_IS_PROJECT_TREE (self));
  g_return_if_fail (G_IS_FILE (file));

  if (NULL == (context = gb_project_tree_get_context (self)))
    return;

  g_assert (IDE_IS_CONTEXT (context));

  node = dzl_tree_find_child_node (DZL_TREE (self), NULL, find_files_node, NULL);
  if (node == NULL)
    return;

  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);

  if (!g_file_has_prefix (file, workdir))
    return;

  relpath = g_file_get_relative_path (workdir, file);
  if (relpath == NULL)
    return;

  parts = g_strsplit (relpath, G_DIR_SEPARATOR_S, 0);

  last_node = node;
  for (guint i = 0; parts [i]; i++)
    {
      node = dzl_tree_find_child_node (DZL_TREE (self), node, find_child_node, parts [i]);
      if (node == NULL)
        {
          node = last_node;
          reveal_parent = TRUE;
          break;
        }
      else
        {
          last_node = node;
        }
    }

  /* If the specified node wasn't found, still expand its ancestor */
  if (expand_folder || reveal_parent)
    dzl_tree_node_expand (node, TRUE);
  else
    dzl_tree_expand_to_node (DZL_TREE (self), node);

  dzl_tree_scroll_to_node (DZL_TREE (self), node);
  dzl_tree_node_select (node);

  if (focus_tree_view)
    ide_workbench_focus (ide_widget_get_workbench (GTK_WIDGET (self)), GTK_WIDGET (self));
}
