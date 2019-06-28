/* ide-tree-model.c
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

#define G_LOG_DOMAIN "ide-tree-model"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-plugins.h>
#include <libide-threading.h>
#include <string.h>

#include "ide-tree-addin.h"
#include "ide-tree-model.h"
#include "ide-tree-node.h"
#include "ide-tree-private.h"
#include "ide-tree.h"

struct _IdeTreeModel
{
  IdeObject               parent_instance;
  IdeExtensionSetAdapter *addins;
  gchar                  *kind;
  IdeTreeNode            *root;
  IdeTree                *tree;
};

typedef struct
{
  IdeTreeNode      *drag_node;
  IdeTreeNode      *drop_node;
  GtkSelectionData *selection;
  GdkDragAction     actions;
  gint              n_active;
} DragDataReceived;

static void tree_model_iface_init       (GtkTreeModelIface      *iface);
static void tree_drag_dest_iface_init   (GtkTreeDragDestIface   *iface);
static void tree_drag_source_iface_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (IdeTreeModel, ide_tree_model, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL, tree_model_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST, tree_drag_dest_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE, tree_drag_source_iface_init))

enum {
  PROP_0,
  PROP_KIND,
  PROP_ROOT,
  PROP_TREE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
drag_data_received_free (DragDataReceived *data)
{
  g_assert (data != NULL);
  g_assert (!data->drag_node || IDE_IS_TREE_NODE (data->drag_node));
  g_assert (!data->drop_node || IDE_IS_TREE_NODE (data->drop_node));
  g_assert (data->n_active == 0);

  g_clear_object (&data->drag_node);
  g_clear_object (&data->drop_node);
  g_clear_pointer (&data->selection, gtk_selection_data_free);
  g_slice_free (DragDataReceived, data);
}

static IdeTreeNode *
create_root (void)
{
  return g_object_new (IDE_TYPE_TREE_NODE,
                       "children-possible", TRUE,
                       NULL);
}

static void
ide_tree_model_build_node_cb (IdeExtensionSetAdapter *set,
                              PeasPluginInfo         *plugin_info,
                              PeasExtension          *exten,
                              gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  IdeTreeNode *node = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));

  ide_tree_addin_build_node (addin, node);
}

void
_ide_tree_model_build_node (IdeTreeModel *self,
                            IdeTreeNode  *node)
{
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (IDE_IS_TREE_NODE (node));

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_build_node_cb,
                                     node);
}

static IdeTreeNodeVisit
ide_tree_model_addin_added_traverse_cb (IdeTreeNode *node,
                                        gpointer     user_data)
{
  IdeTreeAddin *addin = user_data;

  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_NODE (node));

  if (!ide_tree_node_is_empty (node))
    {
      ide_tree_addin_build_node (addin, node);

      if (ide_tree_node_get_children_possible (node))
        _ide_tree_node_set_needs_build_children (node, TRUE);
    }

  return IDE_TREE_NODE_VISIT_CHILDREN;
}

static void
ide_tree_model_addin_added_cb (IdeExtensionSetAdapter *adapter,
                               PeasPluginInfo         *plugin_info,
                               PeasExtension          *exten,
                               gpointer                user_data)
{
  IdeTreeModel *self = user_data;
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (IDE_IS_TREE (self->tree));

  ide_tree_addin_load (addin, self->tree, self);

  ide_tree_node_traverse (self->root,
                          G_PRE_ORDER,
                          G_TRAVERSE_ALL,
                          -1,
                          ide_tree_model_addin_added_traverse_cb,
                          addin);
}

static void
ide_tree_model_addin_removed_cb (IdeExtensionSetAdapter *adapter,
                                 PeasPluginInfo         *plugin_info,
                                 PeasExtension          *exten,
                                 gpointer                user_data)
{
  IdeTreeModel *self = user_data;
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TREE_MODEL (self));

  ide_tree_addin_unload (addin, self->tree, self);
}

static void
ide_tree_model_parent_set (IdeObject *object,
                           IdeObject *parent)
{
  IdeTreeModel *self = (IdeTreeModel *)object;
  g_autoptr(IdeContext) context = NULL;

  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (!parent || IDE_IS_OBJECT (parent));

  if (self->addins != NULL || parent == NULL ||
      !(context = ide_object_ref_context (IDE_OBJECT (self))))
    return;

  g_assert (IDE_IS_TREE (self->tree));

  self->addins = ide_extension_set_adapter_new (IDE_OBJECT (self),
                                                peas_engine_get_default (),
                                                IDE_TYPE_TREE_ADDIN,
                                                "Tree-Kind",
                                                self->kind);

  g_signal_connect_object (self->addins,
                           "extension-added",
                           G_CALLBACK (ide_tree_model_addin_added_cb),
                           self,
                           0);

  g_signal_connect_object (self->addins,
                           "extension-removed",
                           G_CALLBACK (ide_tree_model_addin_removed_cb),
                           self,
                           0);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_addin_added_cb,
                                     self);
}

static void
ide_tree_model_dispose (GObject *object)
{
  IdeTreeModel *self = (IdeTreeModel *)object;

  /* Clear the model back-pointer for root so that it cannot emit anu
   * further signals on our tree model.
   */
  if (self->root != NULL)
    _ide_tree_node_set_model (self->root, NULL);

  g_clear_object (&self->tree);
  ide_clear_and_destroy_object (&self->addins);
  g_clear_object (&self->root);
  g_clear_pointer (&self->kind, g_free);

  G_OBJECT_CLASS (ide_tree_model_parent_class)->dispose (object);
}

static void
ide_tree_model_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeTreeModel *self = IDE_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_string (value, ide_tree_model_get_kind (self));
      break;

    case PROP_ROOT:
      g_value_set_object (value, ide_tree_model_get_root (self));
      break;

    case PROP_TREE:
      g_value_set_object (value, self->tree);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_model_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeTreeModel *self = IDE_TREE_MODEL (object);

  switch (prop_id)
    {
    case PROP_KIND:
      ide_tree_model_set_kind (self, g_value_get_string (value));
      break;

    case PROP_ROOT:
      ide_tree_model_set_root (self, g_value_get_object (value));
      break;

    case PROP_TREE:
      self->tree = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_model_class_init (IdeTreeModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);

  object_class->dispose = ide_tree_model_dispose;
  object_class->get_property = ide_tree_model_get_property;
  object_class->set_property = ide_tree_model_set_property;

  i_object_class->parent_set = ide_tree_model_parent_set;

  properties [PROP_TREE] =
    g_param_spec_object ("tree",
                         "Tree",
                         "The tree the model belongs to",
                         IDE_TYPE_TREE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeModel:root:
   *
   * The "root" property contains the root #IdeTreeNode that is used to build
   * the tree. It should contain an object for the #IdeTreeNode:item property
   * so that #IdeTreeAddin's may use it to build the node and any children.
   *
   * Since: 3.32
   */
  properties [PROP_ROOT] =
    g_param_spec_object ("root",
                         "Root",
                         "The root IdeTreeNode",
                         IDE_TYPE_TREE_NODE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * IdeTreeModel:kind:
   *
   * The "kind" property is used to determine what #IdeTreeAddin plugins to
   * load. Only plugins which match the "kind" will be loaded to extend the
   * tree contents.
   *
   * For example, to extend the project-tree, plugins should set
   * "X-Tree-Kind=project-tree" in their .plugin manifest.
   *
   * Since: 3.32
   */
  properties [PROP_KIND] =
    g_param_spec_string ("kind",
                         "Kind",
                         "The kind of tree model that is being generated",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tree_model_init (IdeTreeModel *self)
{
  self->root = create_root ();
}

IdeTreeModel *
_ide_tree_model_new (IdeTree *tree)
{
  return g_object_new (IDE_TYPE_TREE_MODEL,
                       "tree", tree,
                       NULL);
}

void
_ide_tree_model_release_addins (IdeTreeModel *self)
{
  g_assert (IDE_IS_TREE_MODEL (self));

  ide_clear_and_destroy_object (&self->addins);
}

static GtkTreeModelFlags
ide_tree_model_get_flags (GtkTreeModel *model)
{
  return 0;
}

static gint
ide_tree_model_get_n_columns (GtkTreeModel *model)
{
  return 1;
}

static GType
ide_tree_model_get_column_type (GtkTreeModel *model,
                                gint          index_)
{
  return IDE_TYPE_TREE_NODE;
}

static GtkTreePath *
ide_tree_model_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter)
{
  g_autoptr(GArray) indexes = NULL;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE_MODEL (tree_model));
  g_assert (iter != NULL);
  g_assert (IDE_IS_TREE_NODE (iter->user_data));

  node = iter->user_data;

  if (ide_tree_node_is_root (node))
    return NULL;

  indexes = g_array_new (FALSE, FALSE, sizeof (gint));

  do
    {
      gint position;

      position = ide_tree_node_get_index (node);
      g_array_prepend_val (indexes, position);
    }
  while ((node = ide_tree_node_get_parent (node)) &&
         !ide_tree_node_is_root (node));

  return gtk_tree_path_new_from_indicesv (&g_array_index (indexes, gint, 0), indexes->len);
}

static gboolean
ide_tree_model_get_iter (GtkTreeModel *model,
                         GtkTreeIter  *iter,
                         GtkTreePath  *path)
{
  IdeTreeModel *self = (IdeTreeModel *)model;
  IdeTreeNode *node;
  gint *indices;
  gint depth = 0;

  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (iter != NULL);
  g_assert (path != NULL);

  memset (iter, 0, sizeof *iter);

  if (self->root == NULL)
    return FALSE;

  indices = gtk_tree_path_get_indices_with_depth (path, &depth);

  node = self->root;

  for (gint i = 0; i < depth; i++)
    {
      if (!(node = ide_tree_node_get_nth_child (node, indices[i])))
        return FALSE;
    }

  if (ide_tree_node_is_root (node))
    return FALSE;

  iter->user_data = node;
  return TRUE;
}

static void
ide_tree_model_get_value (GtkTreeModel *model,
                          GtkTreeIter  *iter,
                          gint          column,
                          GValue       *value)
{
  g_value_init (value, IDE_TYPE_TREE_NODE);
  g_value_set_object (value, iter->user_data);
}

static gboolean
ide_tree_model_iter_next (GtkTreeModel *model,
                          GtkTreeIter  *iter)
{
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (!iter->user_data || IDE_IS_TREE_NODE (iter->user_data));

  if (iter->user_data)
    iter->user_data = ide_tree_node_get_next (iter->user_data);

  return IDE_IS_TREE_NODE (iter->user_data);
}

static gboolean
ide_tree_model_iter_previous (GtkTreeModel *model,
                              GtkTreeIter  *iter)
{
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (!iter->user_data || IDE_IS_TREE_NODE (iter->user_data));

  if (iter->user_data)
    iter->user_data = ide_tree_node_get_previous (iter->user_data);

  return IDE_IS_TREE_NODE (iter->user_data);
}

static gboolean
ide_tree_model_iter_nth_child (GtkTreeModel *model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent,
                               gint          n)
{
  IdeTreeModel *self = (IdeTreeModel  *)model;
  IdeTreeNode *pnode;

  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (iter != NULL);

  if (self->root == NULL)
    return FALSE;

  g_assert (parent == NULL || IDE_IS_TREE_NODE (parent->user_data));

  n = CLAMP (n, 0, G_MAXINT);

  memset (iter, 0, sizeof *iter);

  if (parent == NULL)
    pnode = self->root;
  else
    pnode = parent->user_data;
  g_assert (IDE_IS_TREE_NODE (pnode));

  iter->user_data = ide_tree_node_get_nth_child (pnode, n);
  g_assert (!iter->user_data || IDE_IS_TREE_NODE (iter->user_data));

  return IDE_IS_TREE_NODE (iter->user_data);
}

static gboolean
ide_tree_model_iter_children (GtkTreeModel *model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *parent)
{
  return ide_tree_model_iter_nth_child (model, iter, parent, 0);
}

static gboolean
ide_tree_model_iter_has_child (GtkTreeModel *model,
                               GtkTreeIter  *iter)
{
  gboolean ret;

  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (IDE_IS_TREE_NODE (iter->user_data));

  ret = ide_tree_node_has_child (iter->user_data);

  IDE_TRACE_MSG ("%s has child -> %s",
                 ide_tree_node_get_display_name (iter->user_data),
                 ret ? "yes" : "no");

  return ret;
}

static gint
ide_tree_model_iter_n_children (GtkTreeModel *model,
                                GtkTreeIter  *iter)
{
  IdeTreeModel *self = (IdeTreeModel *)model;
  gint ret;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (self != NULL);
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (IDE_IS_TREE_NODE (self->root));
  g_assert (iter == NULL || IDE_IS_TREE_NODE (iter->user_data));

  if (iter == NULL)
    ret = ide_tree_node_get_n_children (self->root);
  else if (iter->user_data)
    ret = ide_tree_node_get_n_children (iter->user_data);
  else
    ret = 0;

  IDE_RETURN (ret);
}

static gboolean
ide_tree_model_iter_parent (GtkTreeModel *model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *child)
{
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (child != NULL);
  g_assert (IDE_IS_TREE_NODE (child->user_data));

  memset (iter, 0, sizeof *iter);

  iter->user_data = ide_tree_node_get_parent (child->user_data);

  return !ide_tree_node_is_root (iter->user_data);
}

static void
ide_tree_model_row_inserted (GtkTreeModel *model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter)
{
  IdeTreeModel *self = (IdeTreeModel *)model;
  IdeTreeNode *node;

  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (path != NULL);
  g_assert (iter != NULL);

  node = iter->user_data;

  g_assert (IDE_IS_TREE_NODE (node));

#if 0
  g_print ("Building %s (child of %s)\n",
           ide_tree_node_get_display_name (node),
           ide_tree_node_get_display_name (ide_tree_node_get_parent (node)));
#endif

  /*
   * If this node holds an IdeObject which is not rooted on our object
   * tree, add it to the object tree beneath us so that it can get destroy
   * propagation and access to the IdeContext.
   */
  if (ide_tree_node_holds (node, IDE_TYPE_OBJECT))
    {
      IdeObject *object = ide_tree_node_get_item (node);

      if (!ide_object_get_parent (object))
        ide_object_append (IDE_OBJECT (self), object);
    }

  _ide_tree_model_build_node (self, node);
}

static void
ide_tree_model_ref_node (GtkTreeModel *model,
                         GtkTreeIter  *iter)
{
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (!iter->user_data || IDE_IS_TREE_NODE (iter->user_data));

  if (iter->user_data)
    g_object_ref (iter->user_data);
}

static void
ide_tree_model_unref_node (GtkTreeModel *model,
                           GtkTreeIter  *iter)
{
  g_assert (IDE_IS_TREE_MODEL (model));
  g_assert (iter != NULL);
  g_assert (!iter->user_data || IDE_IS_TREE_NODE (iter->user_data));

  if (iter->user_data)
    g_object_unref (iter->user_data);
}

static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags = ide_tree_model_get_flags;
  iface->get_n_columns = ide_tree_model_get_n_columns;
  iface->get_column_type = ide_tree_model_get_column_type;
  iface->get_iter = ide_tree_model_get_iter;
  iface->get_path = ide_tree_model_get_path;
  iface->get_value = ide_tree_model_get_value;
  iface->iter_next = ide_tree_model_iter_next;
  iface->iter_previous = ide_tree_model_iter_previous;
  iface->iter_children = ide_tree_model_iter_children;
  iface->iter_has_child = ide_tree_model_iter_has_child;
  iface->iter_n_children = ide_tree_model_iter_n_children;
  iface->iter_nth_child = ide_tree_model_iter_nth_child;
  iface->iter_parent = ide_tree_model_iter_parent;
  iface->row_inserted = ide_tree_model_row_inserted;
  iface->ref_node = ide_tree_model_ref_node;
  iface->unref_node = ide_tree_model_unref_node;
}

/**
 * ide_tree_model_get_path_for_node:
 * @self: an #IdeTreeModel
 * @node: an #IdeTreeNode
 *
 * Gets the #GtkTreePath pointing at @node.
 *
 * Returns: (transfer full) (nullable): a new #GtkTreePath
 *
 * Since: 3.32
 */
GtkTreePath *
ide_tree_model_get_path_for_node (IdeTreeModel *self,
                                  IdeTreeNode  *node)
{
  GtkTreeIter iter;

  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), NULL);
  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  if (ide_tree_model_get_iter_for_node (self, &iter, node))
    return gtk_tree_model_get_path (GTK_TREE_MODEL (self), &iter);

  return NULL;
}

/**
 * ide_tree_model_get_iter_for_node:
 * @self: an #IdeTreeModel
 * @iter: (out): a #GtkTreeIter
 * @node: an #IdeTreeNode
 *
 * Gets a #GtkTreeIter that points at @node.
 *
 * Returns: %TRUE if @iter was set; otherwise %FALSE
 *
 * Since: 3.32
 */
gboolean
ide_tree_model_get_iter_for_node (IdeTreeModel *self,
                                  GtkTreeIter  *iter,
                                  IdeTreeNode  *node)
{
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), FALSE);

  if (_ide_tree_model_contains_node (self, node))
    {
      memset (iter, 0, sizeof *iter);
      iter->user_data = node;
      return TRUE;
    }

  return FALSE;
}

/**
 * ide_tree_model_get_root:
 * @self: a #IdeTreeModel
 *
 * Gets the root #IdeTreeNode. This node is never visualized in the tree, but
 * is used to build the immediate children which are displayed in the tree.
 *
 * Returns: (transfer none) (not nullable): an #IdeTreeNode
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_model_get_root (IdeTreeModel *self)
{
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), NULL);

  return self->root;
}

static IdeTreeNodeVisit
ide_tree_model_remove_all_cb (IdeTreeNode *node,
                              gpointer     user_data)
{
  IdeTreeModel *self = user_data;

  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (IDE_IS_TREE_MODEL (self));

  if (node != self->root)
    {
      GtkTreePath *tree_path;

      tree_path = ide_tree_model_get_path_for_node (self, node);
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (self), tree_path);
      gtk_tree_path_free (tree_path);
    }

  return IDE_TREE_NODE_VISIT_CHILDREN;
}

static void
ide_tree_model_remove_all (IdeTreeModel *self)
{
  g_return_if_fail (IDE_IS_TREE_MODEL (self));

  ide_tree_node_traverse (self->root,
                          G_POST_ORDER,
                          G_TRAVERSE_ALL,
                          -1,
                          ide_tree_model_remove_all_cb,
                          self);
}

void
ide_tree_model_set_root (IdeTreeModel *self,
                         IdeTreeNode  *root)
{
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (!root || IDE_IS_TREE_NODE (root));

  if (root != self->root)
    {
      ide_tree_model_remove_all (self);
      g_clear_object (&self->root);

      if (root != NULL)
        self->root = g_object_ref (root);
      else
        self->root = create_root ();

      _ide_tree_node_set_model (self->root, self);

      /* Root always requires building children */
      if (!ide_tree_node_get_children_possible (self->root))
        ide_tree_node_set_children_possible (self->root, TRUE);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ROOT]);
    }
}

/**
 * ide_tree_model_get_kind:
 * @self: a #IdeTreeModel
 *
 * Gets the kind of model that is being generated. See #IdeTreeModel:kind
 * for more information.
 *
 * Returns: (nullable): a string containing the kind, or %NULL
 *
 * Since: 3.32
 */
const gchar *
ide_tree_model_get_kind (IdeTreeModel *self)
{
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), NULL);

  return self->kind;
}

/**
 * ide_tree_model_set_kind:
 * @self: a #IdeTreeModel
 * @kind: a string describing the kind of model
 *
 * Sets the kind of model that is being created. This determines what plugins
 * are used to generate the tree contents.
 *
 * This should be set before adding the #IdeTreeModel to an #IdeObject to
 * ensure the tree builds the proper contents.
 *
 * Since: 3.32
 */
void
ide_tree_model_set_kind (IdeTreeModel *self,
                         const gchar  *kind)
{
  g_return_if_fail (IDE_IS_TREE_MODEL (self));

  if (!ide_str_equal0 (kind, self->kind))
    {
      g_free (self->kind);
      self->kind = g_strdup (kind);

      if (self->addins != NULL)
        ide_extension_set_adapter_set_value (self->addins, kind);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

typedef struct
{
  IdeTreeNode *node;
  IdeTree     *tree;
  gboolean     handled;
} RowActivated;

static void
ide_tree_model_row_activated_cb (IdeExtensionSetAdapter *set,
                                 PeasPluginInfo         *plugin_info,
                                 PeasExtension          *exten,
                                 gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  RowActivated *state = user_data;

  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (state != NULL);

  if (state->handled)
    return;

  state->handled = ide_tree_addin_node_activated (addin, state->tree, state->node);
}

gboolean
_ide_tree_model_row_activated (IdeTreeModel *self,
                               IdeTree      *tree,
                               GtkTreePath  *path)
{
  GtkTreeIter iter;

  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (IDE_IS_TREE (tree));
  g_assert (path != NULL);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path))
    {
      RowActivated state = {
        .node = iter.user_data,
        .tree = tree,
        .handled = FALSE,
      };

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_tree_model_row_activated_cb,
                                         &state);

      return state.handled;
    }

  return FALSE;
}

/**
 * ide_tree_model_get_node:
 * @self: a #IdeTreeModel
 * @iter: a #GtkTreeIter
 *
 * Gets the #IdeTreeNode found at @iter.
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 *
 * Since: 3.32
 */
IdeTreeNode *
ide_tree_model_get_node (IdeTreeModel *self,
                         GtkTreeIter  *iter)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  if (IDE_IS_TREE_NODE (iter->user_data))
    return iter->user_data;

  return NULL;
}

gboolean
_ide_tree_model_contains_node (IdeTreeModel *self,
                               IdeTreeNode  *node)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), FALSE);
  g_return_val_if_fail (!node || IDE_IS_TREE_NODE (node), FALSE);

  if (node == NULL)
    return FALSE;

  return self->root == ide_tree_node_get_root (node);
}

static void
inc_active (IdeTask *task)
{
  gint n_active = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "N_ACTIVE"));
  n_active++;
  g_object_set_data (G_OBJECT (task), "N_ACTIVE", GINT_TO_POINTER (n_active));
}

static gboolean
dec_active_and_test (IdeTask *task)
{
  gint n_active = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "N_ACTIVE"));
  n_active--;
  g_object_set_data (G_OBJECT (task), "N_ACTIVE", GINT_TO_POINTER (n_active));
  return n_active == 0;
}

static void
ide_tree_model_addin_build_children_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ide_tree_addin_build_children_finish (addin, result, &error);

  if (dec_active_and_test (task))
    {
#if 0
      {
        IdeTreeNode *node = ide_task_get_task_data (task);
        _ide_tree_node_dump (ide_tree_node_get_root (node));
      }
#endif

      ide_task_return_boolean (task, TRUE);
    }
}

static void
ide_tree_model_expand_foreach_cb (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  PeasExtension          *exten,
                                  gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  IdeTreeNode *node;
  IdeTask *task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (task));

  node = ide_task_get_task_data (task);

  g_assert (IDE_IS_TREE_NODE (node));

  inc_active (task);

  ide_tree_addin_build_children_async (addin,
                                       node,
                                       ide_task_get_cancellable (task),
                                       ide_tree_model_addin_build_children_cb,
                                       g_object_ref (task));

  _ide_tree_node_set_needs_build_children (node, FALSE);
}

static void
ide_tree_model_expand_completed (IdeTreeNode *node,
                                 GParamSpec  *pspec,
                                 IdeTask     *task)
{
  g_assert (IDE_IS_TREE_NODE (node));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_TASK (task));

  _ide_tree_node_set_loading (node, FALSE);
}

void
ide_tree_model_expand_async (IdeTreeModel        *self,
                             IdeTreeNode         *node,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (IDE_IS_TREE_NODE (node));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_tree_model_expand_async);
  ide_task_set_task_data (task, g_object_ref (node), g_object_unref);

  g_signal_connect_object (task,
                           "notify::completed",
                           G_CALLBACK (ide_tree_model_expand_completed),
                           node,
                           G_CONNECT_SWAPPED);

  /* If no building is necessary, then just skip any work here */
  if (!_ide_tree_node_get_needs_build_children (node) ||
      ide_extension_set_adapter_get_n_extensions (self->addins) == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  _ide_tree_node_set_loading (node, TRUE);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_expand_foreach_cb,
                                     task);
}

gboolean
ide_tree_model_expand_finish (IdeTreeModel  *self,
                              GAsyncResult  *result,
                              GError       **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static IdeTreeNodeVisit
ide_tree_model_invalidate_traverse_cb (IdeTreeNode *node,
                                       gpointer     user_data)
{
  g_assert (IDE_IS_TREE_NODE (node));

  if (!ide_tree_node_is_root (node))
    ide_tree_node_remove (ide_tree_node_get_parent (node), node);

  return IDE_TREE_NODE_VISIT_CHILDREN;
}

/**
 * ide_tree_model_invalidate:
 * @self: a #IdeTreeModel
 * @node: (nullable): an #IdeTreeNode or %NULL
 *
 * Invalidates @model starting from @node so that those items
 * are rebuilt using the configured tree addins.
 *
 * If @node is %NULL, the root of the tree is invalidated.
 *
 * Since: 3.32
 */
void
ide_tree_model_invalidate (IdeTreeModel *self,
                           IdeTreeNode  *node)
{
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (!node || IDE_IS_TREE_NODE (node));

  if (node == NULL)
    node = self->root;

  ide_tree_node_traverse (node,
                          G_POST_ORDER,
                          G_TRAVERSE_ALL,
                          -1,
                          ide_tree_model_invalidate_traverse_cb,
                          NULL);

  _ide_tree_node_set_needs_build_children (node, TRUE);
  ide_tree_model_expand_async (self, node, NULL, NULL, NULL);
}

static void
ide_tree_model_propagate_selection_changed_cb (IdeExtensionSetAdapter *set,
                                               PeasPluginInfo         *plugin_info,
                                               PeasExtension          *exten,
                                               gpointer                user_data)
{
  IdeTreeNode *node = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  ide_tree_addin_selection_changed (IDE_TREE_ADDIN (exten), node);
}

void
_ide_tree_model_selection_changed (IdeTreeModel *self,
                                   GtkTreeIter  *iter)
{
  IdeTreeNode *node = NULL;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (!iter || IDE_IS_TREE_NODE (iter->user_data));

  if (self->addins == NULL)
    return;

  if (iter != NULL)
    node = ide_tree_model_get_node (self, iter);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_propagate_selection_changed_cb,
                                     node);
}

static void
ide_tree_model_propagate_node_expanded_cb (IdeExtensionSetAdapter *set,
                                           PeasPluginInfo         *plugin_info,
                                           PeasExtension          *exten,
                                           gpointer                user_data)
{
  IdeTreeNode *node = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  ide_tree_addin_node_expanded (IDE_TREE_ADDIN (exten), node);
}

void
_ide_tree_model_row_expanded (IdeTreeModel *self,
                              IdeTree      *tree,
                              GtkTreePath  *path)
{
  GtkTreeIter iter;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (path != NULL);

  if (self->addins == NULL)
    return;

  if (ide_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path))
    {
      IdeTreeNode *node = ide_tree_model_get_node (self, &iter);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_tree_model_propagate_node_expanded_cb,
                                         node);
    }
}

static void
ide_tree_model_propagate_node_collapsed_cb (IdeExtensionSetAdapter *set,
                                            PeasPluginInfo         *plugin_info,
                                            PeasExtension          *exten,
                                            gpointer                user_data)
{
  IdeTreeNode *node = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (!node || IDE_IS_TREE_NODE (node));

  ide_tree_addin_node_collapsed (IDE_TREE_ADDIN (exten), node);
}

void
_ide_tree_model_row_collapsed (IdeTreeModel *self,
                               IdeTree      *tree,
                               GtkTreePath  *path)
{
  GtkTreeIter iter;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (IDE_IS_TREE (tree));
  g_return_if_fail (path != NULL);

  if (self->addins == NULL)
    return;

  if (ide_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path))
    {
      IdeTreeNode *node = ide_tree_model_get_node (self, &iter);

      ide_extension_set_adapter_foreach (self->addins,
                                         ide_tree_model_propagate_node_collapsed_cb,
                                         node);
    }
}

/**
 * ide_tree_model_get_tree:
 * @self: a #IdeTreeModel
 *
 * Returns: (transfer none): an #IdeTree
 *
 * Since: 3.32
 */
IdeTree *
ide_tree_model_get_tree (IdeTreeModel *self)
{
  g_return_val_if_fail (IDE_IS_TREE_MODEL (self), NULL);

  return self->tree;
}

static void
ide_tree_model_cell_data_func_cb (IdeExtensionSetAdapter *set,
                                  PeasPluginInfo         *plugin_info,
                                  PeasExtension          *exten,
                                  gpointer                user_data)
{
  struct {
    IdeTreeNode     *node;
    GtkCellRenderer *cell;
  } *state = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));
  g_assert (GTK_IS_CELL_RENDERER (state->cell));

  ide_tree_addin_cell_data_func (IDE_TREE_ADDIN (exten), state->node, state->cell);
}

void
_ide_tree_model_cell_data_func (IdeTreeModel    *self,
                                GtkTreeIter     *iter,
                                GtkCellRenderer *cell)
{
  struct {
    IdeTreeNode     *node;
    GtkCellRenderer *cell;
  } state;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_TREE_MODEL (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  state.node = iter->user_data;
  state.cell = cell;

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_cell_data_func_cb,
                                     &state);
}

static void
ide_tree_model_row_draggable_cb (IdeExtensionSetAdapter *set,
                                 PeasPluginInfo         *plugin_info,
                                 PeasExtension          *exten,
                                 gpointer                user_data)
{
  struct {
    IdeTreeNode *node;
    gboolean     draggable;
  } *state = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));

  state->draggable |= ide_tree_addin_node_draggable (IDE_TREE_ADDIN (exten), state->node);
}

static gboolean
ide_tree_model_row_draggable (GtkTreeDragSource *source,
                              GtkTreePath       *path)
{
  IdeTreeModel *self = (IdeTreeModel *)source;
  GtkTreeIter iter;
  struct {
    IdeTreeNode *node;
    gboolean     draggable;
  } state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (self));

  if (!ide_tree_model_get_iter (GTK_TREE_MODEL (source), &iter, path))
    return FALSE;

  if (!IDE_IS_TREE_NODE (iter.user_data))
    return FALSE;

  state.node = iter.user_data;
  state.draggable = FALSE;

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_row_draggable_cb,
                                     &state);

  return state.draggable;
}

static gboolean
ide_tree_model_drag_data_get (GtkTreeDragSource *source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection)
{
  IdeTreeModel *self = (IdeTreeModel *)source;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (path != NULL);
  g_assert (selection != NULL);

  return gtk_tree_set_row_drag_data (selection, GTK_TREE_MODEL (self), path);
}

static gboolean
ide_tree_model_drag_data_delete (GtkTreeDragSource *source,
                                 GtkTreePath       *path)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (source));
  g_assert (path != NULL);

  return FALSE;
}

static void
tree_drag_source_iface_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = ide_tree_model_row_draggable;
  iface->drag_data_get = ide_tree_model_drag_data_get;
  iface->drag_data_delete = ide_tree_model_drag_data_delete;
}

static void
ide_tree_model_drag_data_received_addin_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  DragDataReceived *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_tree_addin_node_dropped_finish (addin, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("%s: %s", G_OBJECT_TYPE_NAME (addin), error->message);
    }

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (!state->drag_node || IDE_IS_TREE_NODE (state->drag_node));
  g_assert (!state->drop_node || IDE_IS_TREE_NODE (state->drop_node));
  g_assert (state->n_active > 0);

  state->n_active--;

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
ide_tree_model_drag_data_received_cb (IdeExtensionSetAdapter *set,
                                      PeasPluginInfo         *plugin_info,
                                      PeasExtension          *exten,
                                      gpointer                user_data)
{
  IdeTreeAddin *addin = (IdeTreeAddin *)exten;
  IdeTask *task = user_data;
  DragDataReceived *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (addin));
  g_assert (IDE_IS_TASK (task));

  state = ide_task_get_task_data (task);
  g_assert (state != NULL);
  g_assert (!state->drag_node || IDE_IS_TREE_NODE (state->drag_node));
  g_assert (!state->drop_node || IDE_IS_TREE_NODE (state->drop_node));

  state->n_active++;

  ide_tree_addin_node_dropped_async (addin,
                                     state->drag_node,
                                     state->drop_node,
                                     state->selection,
                                     state->actions,
                                     NULL,
                                     ide_tree_model_drag_data_received_addin_cb,
                                     g_object_ref (task));
}

static gboolean
ide_tree_model_drag_data_received (GtkTreeDragDest  *dest,
                                   GtkTreePath      *path,
                                   GtkSelectionData *selection)
{
  IdeTreeModel *self = (IdeTreeModel *)dest;
  g_autoptr(GtkTreePath) source_path = NULL;
  g_autoptr(IdeTask) task = NULL;
  GtkTreeModel *source_model = NULL;
  DragDataReceived *state;
  IdeTreeNode *drag_node = NULL;
  IdeTreeNode *drop_node = NULL;
  GtkTreeIter iter;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (path != NULL);
  g_assert (selection != NULL);

  if (gtk_tree_get_row_drag_data (selection, &source_model, &source_path))
    {
      if (IDE_IS_TREE_MODEL (source_model))
        {
          if (ide_tree_model_get_iter (source_model, &iter, source_path))
            drag_node = IDE_TREE_NODE (iter.user_data);
        }
    }

  drop_node = _ide_tree_get_drop_node (self->tree);

  state = g_slice_new0 (DragDataReceived);
  g_set_object (&state->drag_node, drag_node);
  g_set_object (&state->drop_node, drop_node);
  state->selection = gtk_selection_data_copy (selection);
  state->actions = _ide_tree_get_drop_actions (self->tree);


  task = ide_task_new (self, NULL, NULL, NULL);
  ide_task_set_source_tag (task, ide_tree_model_drag_data_received);
  ide_task_set_task_data (task, state, drag_data_received_free);

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_drag_data_received_cb,
                                     task);

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);

  return TRUE;
}

static void
ide_tree_model_row_drop_possible_cb (IdeExtensionSetAdapter *set,
                                     PeasPluginInfo         *plugin_info,
                                     PeasExtension          *exten,
                                     gpointer                user_data)
{
  struct {
    IdeTreeNode      *drag_node;
    IdeTreeNode      *drop_node;
    GtkSelectionData *selection;
    gboolean          drop_possible;
  } *state = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_TREE_ADDIN (exten));
  g_assert (state != NULL);
  g_assert (state->selection != NULL);

  state->drop_possible |= ide_tree_addin_node_droppable (IDE_TREE_ADDIN (exten),
                                                         state->drag_node,
                                                         state->drop_node,
                                                         state->selection);
}

static gboolean
ide_tree_model_row_drop_possible (GtkTreeDragDest  *dest,
                                  GtkTreePath      *path,
                                  GtkSelectionData *selection)
{
  IdeTreeModel *self = (IdeTreeModel *)dest;
  g_autoptr(GtkTreePath) source_path = NULL;
  GtkTreeModel *source_model = NULL;
  IdeTreeNode *drag_node = NULL;
  IdeTreeNode *drop_node = NULL;
  GtkTreeIter iter = {0};
  struct {
    IdeTreeNode      *drag_node;
    IdeTreeNode      *drop_node;
    GtkSelectionData *selection;
    gboolean          drop_possible;
  } state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TREE_MODEL (self));
  g_assert (path != NULL);
  g_assert (selection != NULL);

  if (gtk_tree_get_row_drag_data (selection, &source_model, &source_path))
    {
      if (IDE_IS_TREE_MODEL (source_model))
        {
          if (ide_tree_model_get_iter (source_model, &iter, source_path))
            drag_node = IDE_TREE_NODE (iter.user_data);
        }
    }

  if (ide_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, path))
    {
      drop_node = IDE_TREE_NODE (iter.user_data);
    }
  else
    {
      g_autoptr(GtkTreePath) copy = gtk_tree_path_copy (path);

      gtk_tree_path_up (copy);

      if (ide_tree_model_get_iter (GTK_TREE_MODEL (self), &iter, copy))
        drop_node = IDE_TREE_NODE (iter.user_data);
    }

  state.drag_node = drag_node;
  state.drop_node = drop_node;
  state.selection = selection;
  state.drop_possible = FALSE;

  ide_extension_set_adapter_foreach (self->addins,
                                     ide_tree_model_row_drop_possible_cb,
                                     &state);

  return state.drop_possible;
}

static void
tree_drag_dest_iface_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = ide_tree_model_drag_data_received;
  iface->row_drop_possible = ide_tree_model_row_drop_possible;
}
