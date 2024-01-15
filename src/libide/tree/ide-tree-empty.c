/* ide-tree-empty.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-tree-empty"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-tree-empty.h"
#include "ide-tree-node.h"

struct _IdeTreeEmpty
{
  GObject      parent_instance;
  GListModel  *model;
  IdeTreeNode *child;
  guint        n_items;
  guint        empty : 1;
};

enum {
  PROP_0,
  PROP_MODEL,
  N_PROPS
};

static GType
ide_tree_empty_get_item_type (GListModel *model)
{
  return IDE_TYPE_TREE_NODE;
}

static guint
ide_tree_empty_get_n_items (GListModel *model)
{
  IdeTreeEmpty *self = IDE_TREE_EMPTY (model);

  return self->empty ? 1 : self->n_items;
}

static gpointer
ide_tree_empty_get_item (GListModel *model,
                         guint       position)
{
  IdeTreeEmpty *self = IDE_TREE_EMPTY (model);

  g_assert (!self->empty || position == 0);
  g_assert (!self->empty || self->n_items == 0);
  g_assert (self->empty || self->n_items > 0);

  if (self->empty)
    return g_object_ref (self->child);

  if (position >= self->n_items)
    return NULL;

  return g_list_model_get_item (self->model, position);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = ide_tree_empty_get_item_type;
  iface->get_item = ide_tree_empty_get_item;
  iface->get_n_items = ide_tree_empty_get_n_items;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeTreeEmpty, ide_tree_empty, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];
static GIcon *loading_icon;

static void
ide_tree_empty_items_changed_cb (IdeTreeEmpty *self,
                                 guint         position,
                                 guint         removed,
                                 guint         added,
                                 GListModel   *model)
{
  g_assert (IDE_IS_TREE_EMPTY (self));
  g_assert (G_IS_LIST_MODEL (model));
  g_assert (self->empty || self->n_items > 0);
  g_assert (!self->empty || self->n_items == 0);
  g_assert (!self->empty || position == 0);

  if (removed == 0 && added == 0)
    return;

  /* We can enter here in a number of states:
   *
   * 1) "empty" meaning we have a single empty element
   *    and are adding items to the list. We need to remove
   *    our synthetic first element.
   *
   *    The underlying model absolutely must be emitting from
   *    a position of 0 since it would be otherwise empty.
   *
   * 2) "!empty" meaning we have items in our model and all
   *    of them are to be removed with no additions, meaning
   *    we need to synthesize a new empty item.
   *
   * 3) "!empty" meaning we have items in our model, but not
   *    enough items are being removed to cause us to need to
   *    add back a synthetic empty item.
   */

  if (self->empty)
    {
      g_assert (position == 0);
      g_assert (removed == 0);
      g_assert (added > 0);

      self->empty = FALSE;
      self->n_items = added;

      g_list_model_items_changed (G_LIST_MODEL (self), 0, 1, added);
    }
  else
    {
      g_assert (self->n_items > 0);

      if (removed == self->n_items && added == 0)
        {
          self->empty = TRUE;
          self->n_items = 0;

          g_assert (position == 0);

          g_list_model_items_changed (G_LIST_MODEL (self), 0, removed, 1);
        }
      else
        {
          g_assert (removed <= self->n_items);

          self->n_items -= removed;
          self->n_items += added;

          g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
        }

      g_assert (self->empty || self->n_items > 0);
      g_assert (!self->empty || self->n_items == 0);
    }
}

static void
ide_tree_empty_dispose (GObject *object)
{
  IdeTreeEmpty *self = (IdeTreeEmpty *)object;

  g_clear_object (&self->child);
  g_clear_object (&self->model);

  G_OBJECT_CLASS (ide_tree_empty_parent_class)->dispose (object);
}

static void
ide_tree_empty_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeTreeEmpty *self = IDE_TREE_EMPTY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_empty_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeTreeEmpty *self = IDE_TREE_EMPTY (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      self->model = g_value_dup_object (value);
      self->n_items = g_list_model_get_n_items (self->model);
      self->empty = self->n_items == 0;
      g_signal_connect_object (self->model,
                               "items-changed",
                               G_CALLBACK (ide_tree_empty_items_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tree_empty_class_init (IdeTreeEmptyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tree_empty_dispose;
  object_class->get_property = ide_tree_empty_get_property;
  object_class->set_property = ide_tree_empty_set_property;

  properties [PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  loading_icon = g_themed_icon_new ("content-loading-symbolic");
}

static void
ide_tree_empty_init (IdeTreeEmpty *self)
{
  self->empty = TRUE;
  self->child = ide_tree_node_new ();
  ide_tree_node_set_use_markup (self->child, TRUE);
}

static gboolean
boolean_to_title (GBinding     *binding,
                  const GValue *from_value,
                  GValue       *to_value,
                  gpointer      user_data)
{
  if (g_value_get_boolean (from_value))
    g_value_set_static_string (to_value, _("<i>Loading</i>"));
  else
    g_value_set_static_string (to_value, _("Empty"));

  return TRUE;
}

static gboolean
boolean_to_icon (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  if (g_value_get_boolean (from_value))
    g_value_set_object (to_value, loading_icon);
  else
    g_value_set_object (to_value, NULL);

  return TRUE;
}

GListModel *
ide_tree_empty_new (IdeTreeNode *node)
{
  IdeTreeEmpty *self;

  g_return_val_if_fail (IDE_IS_TREE_NODE (node), NULL);

  self = g_object_new (IDE_TYPE_TREE_EMPTY,
                       "model", node,
                       NULL);

  g_object_bind_property_full (node, "loading", self->child, "title", G_BINDING_SYNC_CREATE,
                               boolean_to_title, NULL, NULL, NULL);
  g_object_bind_property_full (node, "loading", self->child, "icon", G_BINDING_SYNC_CREATE,
                               boolean_to_icon, NULL, NULL, NULL);

  return G_LIST_MODEL (self);
}
