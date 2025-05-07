/* ide-tree-node.c
 *
 * Copyright 2018-2023 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tree-node"

#include "config.h"

#include <libdex.h>

#include <libide-gtk.h>

#include "ide-marshal.h"

#include "ide-tree-addin.h"
#include "ide-tree-enums.h"
#include "ide-tree-private.h"

struct _IdeTreeNode
{
  GObject parent_instance;

  IdeTreeNode *parent;
  GQueue children;
  GList link;

  char *title;
  GIcon *icon;
  GIcon *expanded_icon;

  DexFuture *expand;

  GObject *item;

  guint sequence;

  IdeTreeNodeFlags flags : 8;
  guint vcs_ignored : 1;
  guint children_built : 1;
  guint children_possible : 1;
  guint destroy_item : 1;
  guint has_error : 1;
  guint is_header : 1;
  guint reset_on_collapse : 1;
  guint use_markup : 1;
  guint loading : 1;
};

enum {
  PROP_0,
  PROP_CHILDREN_POSSIBLE,
  PROP_DESTROY_ITEM,
  PROP_EXPANDED_ICON,
  PROP_EXPANDED_ICON_NAME,
  PROP_FLAGS,
  PROP_VCS_IGNORED,
  PROP_HAS_ERROR,
  PROP_ICON,
  PROP_ICON_NAME,
  PROP_IS_HEADER,
  PROP_ITEM,
  PROP_LOADING,
  PROP_PARENT,
  PROP_RESET_ON_COLLAPSE,
  PROP_TITLE,
  PROP_USE_MARKUP,
  N_PROPS
};

enum {
  SHOW_POPOVER,
  N_SIGNALS
};

static guint
list_model_get_n_items (GListModel *model)
{
  return IDE_TREE_NODE (model)->children.length;
}

static GType
list_model_get_item_type (GListModel *model)
{
  return IDE_TYPE_TREE_NODE;
}

static gpointer
list_model_get_item (GListModel *model,
                     guint       position)
{
  IdeTreeNode *self = IDE_TREE_NODE (model);

  if (position >= self->children.length)
    return NULL;

  if (position == 0)
    return g_object_ref (g_queue_peek_head (&self->children));

  if (position == self->children.length-1)
    return g_object_ref (g_queue_peek_tail (&self->children));

  return g_object_ref (g_queue_peek_nth (&self->children, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = list_model_get_n_items;
  iface->get_item_type = list_model_get_item_type;
  iface->get_item = list_model_get_item;
}

G_DEFINE_TYPE_WITH_CODE (IdeTreeNode, ide_tree_node, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

IdeTree *
_ide_tree_node_get_tree (IdeTreeNode *self)
{
  IdeTreeNode *root = ide_tree_node_get_root (self);
  return IDE_TREE (g_object_get_data (G_OBJECT (root), "IDE_TREE"));
}

static void
_ide_tree_node_set_loading (IdeTreeNode *self,
                            gboolean     loading)
{
  g_assert (IDE_IS_TREE_NODE (self));

  loading = !!loading;

  if (loading != self->loading)
    {
      self->loading = loading;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_LOADING]);
    }
}

void
_ide_tree_node_collapsed (IdeTreeNode *self)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->reset_on_collapse)
    {
      g_autolist(IdeTreeNode) children = NULL;
      guint length;

      self->children_built = FALSE;
      dex_clear (&self->expand);

      children = g_list_copy (self->children.head);
      length = self->children.length;

      self->children = (GQueue) { NULL, NULL, 0 };

      for (const GList *iter = children; iter; iter = iter->next)
        {
          IdeTreeNode *child = iter->data;

          child->parent = NULL;
          child->link.prev = NULL;
          child->link.next = NULL;
        }

      if (children != NULL)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, length, 0);
    }
}

static void
ide_tree_node_dispose (GObject *object)
{
  IdeTreeNode *self = (IdeTreeNode *)object;

  while (self->children.head != NULL)
    ide_tree_node_unparent (self->children.head->data);

  if (self->parent != NULL)
    ide_tree_node_unparent (self);

  g_clear_pointer (&self->title, g_free);
  dex_clear (&self->expand);

  g_clear_object (&self->icon);
  g_clear_object (&self->expanded_icon);

  if (self->destroy_item && IDE_IS_OBJECT (self->item))
    ide_clear_and_destroy_object (&self->item);
  else
    g_clear_object (&self->item);

  G_OBJECT_CLASS (ide_tree_node_parent_class)->dispose (object);

  g_assert (self->parent == NULL);
  g_assert (self->children.head == NULL);
  g_assert (self->children.length == 0);
  g_assert (self->link.prev == NULL);
  g_assert (self->link.next == NULL);
  g_assert (self->expand == NULL);
}

static void
ide_tree_node_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeTreeNode *self = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      g_value_set_boolean (value, ide_tree_node_get_children_possible (self));
      break;

    case PROP_DESTROY_ITEM:
      g_value_set_boolean (value, ide_tree_node_get_destroy_item (self));
      break;

    case PROP_EXPANDED_ICON:
      g_value_set_object (value, ide_tree_node_get_expanded_icon (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, ide_tree_node_get_flags (self));
      break;

    case PROP_VCS_IGNORED:
      g_value_set_boolean (value, ide_tree_node_get_vcs_ignored (self));
      break;

    case PROP_HAS_ERROR:
      g_value_set_boolean (value, ide_tree_node_get_has_error (self));
      break;

    case PROP_ICON:
      g_value_set_object (value, ide_tree_node_get_icon (self));
      break;

    case PROP_IS_HEADER:
      g_value_set_boolean (value, ide_tree_node_get_is_header (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, ide_tree_node_get_item (self));
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, self->loading);
      break;

    case PROP_PARENT:
      g_value_set_object (value, ide_tree_node_get_parent (self));
      break;

    case PROP_RESET_ON_COLLAPSE:
      g_value_set_boolean (value, ide_tree_node_get_reset_on_collapse (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tree_node_get_title (self));
      break;

    case PROP_USE_MARKUP:
      g_value_set_boolean (value, ide_tree_node_get_use_markup (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_node_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeTreeNode *self = IDE_TREE_NODE (object);

  switch (prop_id)
    {
    case PROP_CHILDREN_POSSIBLE:
      ide_tree_node_set_children_possible (self, g_value_get_boolean (value));
      break;

    case PROP_DESTROY_ITEM:
      ide_tree_node_set_destroy_item (self, g_value_get_boolean (value));
      break;

    case PROP_EXPANDED_ICON:
      ide_tree_node_set_expanded_icon (self, g_value_get_object (value));
      break;

    case PROP_EXPANDED_ICON_NAME:
      ide_tree_node_set_expanded_icon_name (self, g_value_get_string (value));
      break;

    case PROP_FLAGS:
      ide_tree_node_set_flags (self, g_value_get_flags (value));
      break;

    case PROP_VCS_IGNORED:
      ide_tree_node_set_vcs_ignored (self, g_value_get_boolean (value));
      break;

    case PROP_HAS_ERROR:
      ide_tree_node_set_has_error (self, g_value_get_boolean (value));
      break;

    case PROP_ICON:
      ide_tree_node_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_NAME:
      ide_tree_node_set_icon_name (self, g_value_get_string (value));
      break;

    case PROP_IS_HEADER:
      ide_tree_node_set_is_header (self, g_value_get_boolean (value));
      break;

    case PROP_ITEM:
      ide_tree_node_set_item (self, g_value_get_object (value));
      break;

    case PROP_PARENT:
      ide_tree_node_set_parent (self, g_value_get_object (value));
      break;

    case PROP_RESET_ON_COLLAPSE:
      ide_tree_node_set_reset_on_collapse (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE:
      ide_tree_node_set_title (self, g_value_get_string (value));
      break;

    case PROP_USE_MARKUP:
      ide_tree_node_set_use_markup (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_node_class_init (IdeTreeNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tree_node_dispose;
  object_class->get_property = ide_tree_node_get_property;
  object_class->set_property = ide_tree_node_set_property;

  properties [PROP_CHILDREN_POSSIBLE] =
    g_param_spec_boolean ("children-possible", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_DESTROY_ITEM] =
    g_param_spec_boolean ("destroy-item", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_EXPANDED_ICON] =
    g_param_spec_object ("expanded-icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_EXPANDED_ICON_NAME] =
    g_param_spec_string ("expanded-icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags", NULL, NULL,
                        IDE_TYPE_TREE_NODE_FLAGS,
                        IDE_TREE_NODE_FLAGS_NONE,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_VCS_IGNORED] =
  g_param_spec_boolean ("vcs-ignored", NULL, NULL,
                        FALSE,
                        (G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  properties [PROP_HAS_ERROR] =
    g_param_spec_boolean ("has-error", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_ICON_NAME] =
    g_param_spec_string ("icon-name", NULL, NULL,
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_HEADER] =
    g_param_spec_boolean ("is-header", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PARENT] =
    g_param_spec_object ("parent", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_RESET_ON_COLLAPSE] =
    g_param_spec_boolean ("reset-on-collapse", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_MARKUP] =
    g_param_spec_boolean ("use-markup", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [SHOW_POPOVER] =
    g_signal_new ("show-popover",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled, NULL,
                  ide_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN,
                  1,
                  GTK_TYPE_POPOVER);
  g_signal_set_va_marshaller (signals [SHOW_POPOVER],
                              G_TYPE_FROM_CLASS (klass),
                              ide_marshal_BOOLEAN__OBJECTv);
}

static void
ide_tree_node_init (IdeTreeNode *self)
{
  self->link.data = self;
  self->reset_on_collapse = TRUE;
}

IdeTreeNode *
ide_tree_node_new (void)
{
  return g_object_new (IDE_TYPE_TREE_NODE, NULL);
}

const char *
ide_tree_node_get_title (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->title;
}

void
ide_tree_node_set_title (IdeTreeNode *self,
                         const char  *title)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

/**
 * ide_tree_node_get_icon:
 * @self: a #IdeTreeNode
 *
 * Gets the icon for the node.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_tree_node_get_icon (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->icon;
}

void
ide_tree_node_set_icon (IdeTreeNode *self,
                        GIcon       *icon)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  if (g_set_object (&self->icon, icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON]);
}

/**
 * ide_tree_node_get_expanded_icon:
 * @self: a #IdeTreeNode
 *
 * Gets the icon used when the node is expanded.
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
ide_tree_node_get_expanded_icon (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->expanded_icon;
}

void
ide_tree_node_set_expanded_icon (IdeTreeNode *self,
                                 GIcon       *expanded_icon)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (!expanded_icon || G_IS_ICON (expanded_icon));

  if (g_set_object (&self->expanded_icon, expanded_icon))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EXPANDED_ICON]);
}

void
ide_tree_node_set_icon_name (IdeTreeNode *self,
                             const char  *icon_name)
{
  g_autoptr(GIcon) icon = NULL;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (icon_name != NULL)
    icon = g_themed_icon_new (icon_name);

  ide_tree_node_set_icon (self, icon);
}

void
ide_tree_node_set_expanded_icon_name (IdeTreeNode *self,
                                      const char  *expanded_icon_name)
{
  g_autoptr(GIcon) expanded_icon = NULL;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (expanded_icon_name != NULL)
    expanded_icon = g_themed_icon_new (expanded_icon_name);

  ide_tree_node_set_expanded_icon (self, expanded_icon);
}

gboolean
ide_tree_node_get_has_error (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->has_error;
}

void
ide_tree_node_set_has_error (IdeTreeNode *self,
                             gboolean     has_error)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  has_error = !!has_error;

  if (has_error != self->has_error)
    {
      self->has_error = has_error;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_ERROR]);
    }
}

gboolean
ide_tree_node_get_children_possible (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->children_possible;
}

void
ide_tree_node_set_children_possible (IdeTreeNode *self,
                                     gboolean     children_possible)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  children_possible = !!children_possible;

  if (children_possible != self->children_possible)
    {
      self->children_possible = children_possible;

      /* If we are rooted then we need to tell the parent node that
       * our node has been added/removed so GtkTreeListModel will pickup
       * the changes to whether or not our node can be expanded.
       */
      if (self->parent != NULL)
        {
          guint position = g_queue_link_index (&self->parent->children, &self->link);
          g_list_model_items_changed (G_LIST_MODEL (self->parent), position, 1, 1);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CHILDREN_POSSIBLE]);
    }
}

gboolean
ide_tree_node_get_reset_on_collapse (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->reset_on_collapse;
}

void
ide_tree_node_set_reset_on_collapse (IdeTreeNode *self,
                                     gboolean     reset_on_collapse)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  reset_on_collapse = !!reset_on_collapse;

  if (reset_on_collapse != self->reset_on_collapse)
    {
      self->reset_on_collapse = reset_on_collapse;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RESET_ON_COLLAPSE]);
    }
}

gboolean
ide_tree_node_get_use_markup (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->use_markup;
}

void
ide_tree_node_set_use_markup (IdeTreeNode *self,
                              gboolean     use_markup)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  use_markup = !!use_markup;

  if (use_markup != self->use_markup)
    {
      self->use_markup = use_markup;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_MARKUP]);
    }
}

gboolean
ide_tree_node_get_is_header (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->is_header;
}

void
ide_tree_node_set_is_header (IdeTreeNode *self,
                             gboolean     is_header)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  is_header = !!is_header;

  if (is_header != self->is_header)
    {
      self->is_header = is_header;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_HEADER]);
    }
}

/**
 * ide_tree_node_get_item:
 * @self: a #IdeTreeNode
 *
 * Gets the #IdeTreeNode:item property.
 *
 * Returns: (transfer none) (nullable): a #GObject or %NULL
 */
gpointer
ide_tree_node_get_item (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);
  g_return_val_if_fail (self->item == NULL || G_IS_OBJECT (self->item), NULL);

  return self->item;
}

/**
 * ide_tree_node_set_item:
 * @self: a #IdeTreeNode
 * @item: (type GObject) (nullable): a #GObject or %NULL
 *
 * Sets the #IdeTreeNode:item property.
 *
 * This item is typically used so that #IdeTreeAddin can annotate
 * the node with additional data.
 */
void
ide_tree_node_set_item (IdeTreeNode *self,
                        gpointer     item)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (item == NULL || G_IS_OBJECT (item));

  if (item == self->item)
    return;

  if (self->item)
    {
      if (self->destroy_item && IDE_IS_OBJECT (self->item))
        ide_clear_and_destroy_object (&self->item);
      else
        g_clear_object (&self->item);
    }

  g_set_object (&self->item, item);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_HEADER]);
}

/**
 * ide_tree_node_get_parent:
 * @self: a #IdeTreeNode
 *
 * Gets the parent node, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_node_get_parent (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->parent;
}

void
ide_tree_node_set_parent (IdeTreeNode *self,
                          IdeTreeNode *parent)
{
  int pos;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (!parent || IDE_IS_TREE_NODE (parent));
  g_return_if_fail (!parent || self->parent == NULL);
  g_return_if_fail (!parent || self->link.prev == NULL);
  g_return_if_fail (!parent || self->link.next == NULL);
  g_return_if_fail (self->link.data == self);

  if (parent == self->parent)
    return;

  if (parent == NULL)
    {
      ide_tree_node_unparent (self);
      return;
    }

  g_object_ref (self);
  self->parent = parent;
  g_queue_push_tail_link (&parent->children, &self->link);
  pos = g_queue_link_index (&parent->children, &self->link);

  g_list_model_items_changed (G_LIST_MODEL (parent), pos, 0, 1);
}

void
ide_tree_node_unparent (IdeTreeNode *self)
{
  IdeTreeNode *parent;
  int child_position;

  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->parent == NULL)
    return;

  parent = self->parent;
  child_position = g_queue_link_index (&parent->children, &self->link);
  g_return_if_fail (child_position > -1);
  g_queue_unlink (&parent->children, &self->link);
  self->parent = NULL;

  g_list_model_items_changed (G_LIST_MODEL (parent), child_position, 1, 0);

  g_object_unref (self);
}

/**
 * ide_tree_node_remove:
 * @self: a #IdeTreeNode
 *
 * Like ide_tree_node_unparent() but checks parent first.
 */
void
ide_tree_node_remove (IdeTreeNode *self,
                      IdeTreeNode *child)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == self);

  ide_tree_node_unparent (child);
}

/**
 * ide_tree_node_get_first_child:
 * @self: a #IdeTreeNode
 *
 * Gets the first child of @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_node_get_first_child (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return g_queue_peek_head (&self->children);
}

/**
 * ide_tree_node_get_last_child:
 * @self: a #IdeTreeNode
 *
 * Gets the last child of @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_node_get_last_child (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return g_queue_peek_tail (&self->children);
}

/**
 * ide_tree_node_get_prev_sibling:
 * @self: a #IdeTreeNode
 *
 * Gets the previous sibling, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_node_get_prev_sibling (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->link.prev ? self->link.prev->data : NULL;
}

/**
 * ide_tree_node_get_next_sibling:
 * @self: a #IdeTreeNode
 *
 * Gets the nextious sibling, if any.
 *
 * Returns: (transfer none) (nullable): an #IdeTreeNode or %NULL
 */
IdeTreeNode *
ide_tree_node_get_next_sibling (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  return self->link.next ? self->link.next->data : NULL;
}

/**
 * ide_tree_node_get_root:
 * @self: a #IdeTreeNode
 *
 * Gets the root #IdeTreeNode, or @self if it has no parent.
 *
 * Returns: (transfer none): an #IdeTreeNode
 */
IdeTreeNode *
ide_tree_node_get_root (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);

  while (self->parent != NULL)
    self = self->parent;

  return self;
}

gboolean
ide_tree_node_holds (IdeTreeNode *self,
                     GType        type)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return G_TYPE_CHECK_INSTANCE_TYPE (self->item, type);
}

void
ide_tree_node_insert_after (IdeTreeNode *self,
                            IdeTreeNode *parent,
                            IdeTreeNode *previous_sibling)
{
  int child_position;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (parent));
  g_return_if_fail (!previous_sibling || IDE_IS_TREE_NODE (previous_sibling));
  g_return_if_fail (self->parent == NULL);
  g_return_if_fail (self->link.prev == NULL);
  g_return_if_fail (self->link.next == NULL);
  g_return_if_fail (self->link.data == self);

  g_object_ref (self);

  self->parent = parent;

  if (previous_sibling != NULL)
    g_queue_insert_after_link (&parent->children, &previous_sibling->link, &self->link);
  else
    g_queue_push_head_link (&parent->children, &self->link);

  child_position = g_queue_link_index (&parent->children, &self->link);

  g_list_model_items_changed (G_LIST_MODEL (parent), child_position, 0, 1);
}

void
ide_tree_node_insert_before (IdeTreeNode *self,
                             IdeTreeNode *parent,
                             IdeTreeNode *next_sibling)
{
  int child_position;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (parent));
  g_return_if_fail (!next_sibling || IDE_IS_TREE_NODE (next_sibling));
  g_return_if_fail (self->parent == NULL);
  g_return_if_fail (self->link.prev == NULL);
  g_return_if_fail (self->link.next == NULL);
  g_return_if_fail (self->link.data == self);

  g_object_ref (self);

  self->parent = parent;

  if (next_sibling != NULL)
    g_queue_insert_before_link (&parent->children, &next_sibling->link, &self->link);
  else
    g_queue_push_tail_link (&parent->children, &self->link);

  child_position = g_queue_link_index (&parent->children, &self->link);

  g_list_model_items_changed (G_LIST_MODEL (parent), child_position, 0, 1);
}

/**
 * ide_tree_node_insert_sorted:
 * @self: an #IdeTreeNode
 * @child: an #IdeTreeNode
 * @cmpfn: (scope call): an #IdeTreeNodeCompare
 *
 * Insert @child as a child of @self at the sorted position
 * determined by @cmpfn.
 */
void
ide_tree_node_insert_sorted (IdeTreeNode        *self,
                             IdeTreeNode        *child,
                             IdeTreeNodeCompare  cmpfn)
{
  GList *link;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (IDE_IS_TREE_NODE (child));
  g_return_if_fail (child->parent == NULL);

  link = g_queue_find_custom (&self->children, child, (GCompareFunc)cmpfn);

  if (link != NULL)
    ide_tree_node_insert_before (child, self, link->data);
  else
    ide_tree_node_insert_before (child, self, NULL);
}

IdeTreeNodeFlags
ide_tree_node_get_flags (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), 0);

  return self->flags;
}

void
ide_tree_node_set_flags (IdeTreeNode *self,
                         IdeTreeNodeFlags flags)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->flags != flags)
    {
      self->flags = flags;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FLAGS]);
    }
}

gboolean
ide_tree_node_get_vcs_ignored (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->vcs_ignored;
}

void
ide_tree_node_set_vcs_ignored (IdeTreeNode *self,
                               gboolean     ignored)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  if (self->vcs_ignored != ignored)
    {
      self->vcs_ignored = ignored;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VCS_IGNORED]);
    }
}

typedef struct
{
  GTraverseType       type;
  GTraverseFlags      flags;
  gint                depth;
  IdeTreeTraverseFunc callback;
  gpointer            user_data;
} IdeTreeTraversal;

static inline gboolean
can_callback_node (IdeTreeNode    *node,
                   GTraverseFlags  flags)
{
  return ((flags & G_TRAVERSE_LEAVES) && node->children.length == 0) ||
         ((flags & G_TRAVERSE_NON_LEAVES) && node->children.length > 0);
}

static gboolean
do_traversal (IdeTreeNode      *node,
              IdeTreeTraversal *traversal)
{
  const GList *iter;
  IdeTreeNodeVisit ret = IDE_TREE_NODE_VISIT_BREAK;

  if (traversal->depth < 0)
    return IDE_TREE_NODE_VISIT_CONTINUE;

  traversal->depth--;

  if (traversal->type == G_PRE_ORDER && can_callback_node (node, traversal->flags))
    {
      ret = traversal->callback (node, traversal->user_data);

      if (node->parent != NULL &&
          (ret == IDE_TREE_NODE_VISIT_CONTINUE || ret == IDE_TREE_NODE_VISIT_BREAK))
        goto finish;
    }

  iter = node->children.head;

  while (iter != NULL)
    {
      IdeTreeNode *child = iter->data;

      iter = iter->next;

      ret = do_traversal (child, traversal);

      if (ret == IDE_TREE_NODE_VISIT_BREAK)
        goto finish;
    }

  if (traversal->type == G_POST_ORDER && can_callback_node (node, traversal->flags))
    ret = traversal->callback (node, traversal->user_data);

finish:
  traversal->depth++;

  return ret;
}

/**
 * ide_tree_node_traverse:
 * @self: a #IdeTreeNode
 * @traverse_type: the type of traversal, pre and post supported
 * @traverse_flags: the flags for what nodes to match
 * @max_depth: the max depth for the traversal or -1 for all
 * @traverse_func: (scope call): the callback for each matching node
 * @user_data: user data for @traverse_func
 *
 * Calls @traverse_func for each node that matches the requested
 * type, flags, and depth.
 *
 * Traversal is stopped if @traverse_func returns %TRUE.
 */
void
ide_tree_node_traverse (IdeTreeNode         *self,
                        GTraverseType        traverse_type,
                        GTraverseFlags       traverse_flags,
                        gint                 max_depth,
                        IdeTreeTraverseFunc  traverse_func,
                        gpointer             user_data)
{
  IdeTreeTraversal traverse;

  g_return_if_fail (IDE_IS_TREE_NODE (self));
  g_return_if_fail (traverse_type == G_PRE_ORDER ||
                    traverse_type == G_POST_ORDER);
  g_return_if_fail (traverse_func != NULL);

  traverse.type = traverse_type;
  traverse.flags = traverse_flags;
  traverse.depth = max_depth < 0 ? G_MAXINT : max_depth;
  traverse.callback = traverse_func;
  traverse.user_data = user_data;

  do_traversal (self, &traverse);
}

typedef struct
{
  IdeTreeNode *node;
  GListModel  *addins;
  guint        sequence;
} Expand;

static void
expand_free (Expand *state)
{
  g_clear_object (&state->node);
  g_clear_object (&state->addins);
  g_free (state);
}

static DexFuture *
ide_tree_node_expand_fiber (gpointer user_data)
{
  Expand *state = user_data;
  g_autoptr(GPtrArray) futures = NULL;
  GListModel *model;
  guint n_items;

  g_assert (state != NULL);
  g_assert (IDE_IS_TREE_NODE (state->node));
  g_assert (G_IS_LIST_MODEL (state->addins));

  _ide_tree_node_set_loading (state->node, TRUE);

  futures = g_ptr_array_new_with_free_func (dex_unref);
  model = G_LIST_MODEL (state->addins);
  n_items = g_list_model_get_n_items (model);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(IdeTreeAddin) addin = g_list_model_get_item (model, i);

      g_ptr_array_add (futures, ide_tree_addin_build_children (addin, state->node));
    }

  if (futures->len > 0)
    dex_await (dex_future_allv ((DexFuture **)futures->pdata, futures->len), NULL);

  if (state->node->sequence == state->sequence)
    {
      state->node->children_built = TRUE;

      for (guint i = 0; i < n_items; i++)
        {
          g_autoptr(IdeTreeAddin) addin = g_list_model_get_item (model, i);

          for (IdeTreeNode *child = ide_tree_node_get_first_child (state->node);
               child != NULL;
               child = ide_tree_node_get_next_sibling (child))
            ide_tree_addin_build_node (addin, child);
        }
    }

  state->node->children_built = TRUE;

  _ide_tree_node_set_loading (state->node, FALSE);

  return dex_future_new_for_boolean (TRUE);
}

DexFuture *
_ide_tree_node_expand (IdeTreeNode *self,
                       GListModel  *addins)
{
  Expand *state;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), NULL);
  g_return_val_if_fail (!addins || G_IS_LIST_MODEL (addins), NULL);

  if (addins == NULL || self->children_built)
    return dex_future_new_for_boolean (TRUE);

  if (self->expand)
    return dex_ref (self->expand);

  state = g_new0 (Expand, 1);
  state->addins = g_object_ref (addins);
  state->node = g_object_ref (self);
  state->sequence = ++self->sequence;

  self->expand = dex_scheduler_spawn (NULL, 0,
                                      ide_tree_node_expand_fiber,
                                      state,
                                      (GDestroyNotify)expand_free);

  return dex_ref (self->expand);
}

gboolean
_ide_tree_node_children_built (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->children_built;
}

guint
_ide_tree_node_get_child_index (IdeTreeNode *parent,
                                IdeTreeNode *child)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (parent), 0);
  g_return_val_if_fail (IDE_IS_TREE_NODE (child), 0);

  return g_queue_link_index (&parent->children, &child->link);
}

guint
ide_tree_node_get_n_children (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), 0);

  return self->children.length;
}

gboolean
_ide_tree_node_show_popover (IdeTreeNode *self,
                             GtkPopover  *popover)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);
  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);
  g_return_val_if_fail (gtk_widget_get_parent (GTK_WIDGET (popover)) == NULL, FALSE);

  g_signal_emit (self, signals [SHOW_POPOVER], 0, popover, &ret);

  return ret;
}

gboolean
ide_tree_node_get_destroy_item (IdeTreeNode *self)
{
  g_return_val_if_fail (IDE_IS_TREE_NODE (self), FALSE);

  return self->destroy_item;
}

void
ide_tree_node_set_destroy_item (IdeTreeNode *self,
                                gboolean     destroy_item)
{
  g_return_if_fail (IDE_IS_TREE_NODE (self));

  destroy_item = !!destroy_item;

  if (destroy_item != self->destroy_item)
    {
      self->destroy_item = destroy_item;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DESTROY_ITEM]);
    }
}
