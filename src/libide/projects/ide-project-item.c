/* ide-project-item.c
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

#include "ide-project-item.h"

typedef struct
{
  IdeProjectItem *parent;
  GSequence      *children;
} IdeProjectItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeProjectItem, ide_project_item, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_PARENT,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

IdeProjectItem *
ide_project_item_new (IdeProjectItem *parent)
{
  return g_object_new (IDE_TYPE_PROJECT_ITEM,
                       "parent", parent,
                       NULL);
}

void
ide_project_item_append (IdeProjectItem *item,
                         IdeProjectItem *child)
{
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (item);

  g_return_if_fail (IDE_IS_PROJECT_ITEM (item));
  g_return_if_fail (IDE_IS_PROJECT_ITEM (child));

  if (!priv->children)
    priv->children = g_sequence_new (g_object_unref);

  g_object_set (child, "parent", item, NULL);
  g_sequence_append (priv->children, g_object_ref (child));
}

void
ide_project_item_remove (IdeProjectItem *item,
                         IdeProjectItem *child)
{
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (item);
  GSequenceIter *iter;

  g_return_if_fail (IDE_IS_PROJECT_ITEM (item));
  g_return_if_fail (IDE_IS_PROJECT_ITEM (child));
  g_return_if_fail (item == ide_project_item_get_parent (child));

  if (priv->children == NULL)
    return;

  for (iter = g_sequence_get_begin_iter (priv->children);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      if (g_sequence_get (iter) == child)
        {
          g_sequence_remove (iter);
          g_object_set (child, "parent", NULL, NULL);
          g_object_unref (child);
          break;
        }
    }
}

/**
 * ide_project_item_get_children:
 *
 * A scalable list containing the children of the item.
 *
 * Returns: (transfer none): A #GSequence.
 */
GSequence *
ide_project_item_get_children (IdeProjectItem *item)
{
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (item);

  g_return_val_if_fail (IDE_IS_PROJECT_ITEM (item), NULL);

  return priv->children;
}

/**
 * ide_project_item_get_parent:
 *
 * Retrieves the parent #IdeProjectItem of @item, or %NULL if @item is the root
 * of the project tree.
 *
 * Returns: (transfer none) (nullable): An #IdeProjectItem or %NULL if the item
 *   is the root of the tree.
 */
IdeProjectItem *
ide_project_item_get_parent (IdeProjectItem *item)
{
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (item);

  g_return_val_if_fail (IDE_IS_PROJECT_ITEM (item), NULL);

  return priv->parent;
}

void
ide_project_item_set_parent (IdeProjectItem *item,
                             IdeProjectItem *parent)
{
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (item);

  g_return_if_fail (IDE_IS_PROJECT_ITEM (item));
  g_return_if_fail (!parent || IDE_IS_PROJECT_ITEM (parent));

  if (ide_set_weak_pointer (&priv->parent, parent))
    g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_PARENT]);
}

static void
ide_project_item_finalize (GObject *object)
{
  IdeProjectItem *self = (IdeProjectItem *)object;
  IdeProjectItemPrivate *priv = ide_project_item_get_instance_private (self);

  ide_clear_weak_pointer (&priv->parent);
  g_clear_pointer (&priv->children, g_sequence_free);

  G_OBJECT_CLASS (ide_project_item_parent_class)->finalize (object);
}

static void
ide_project_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeProjectItem *self = (IdeProjectItem *)object;

  switch (prop_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, ide_project_item_get_parent (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeProjectItem *self = (IdeProjectItem *)object;

  switch (prop_id)
    {
    case PROP_PARENT:
      ide_project_item_set_parent (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_project_item_class_init (IdeProjectItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_project_item_finalize;
  object_class->get_property = ide_project_item_get_property;
  object_class->set_property = ide_project_item_set_property;

  properties [PROP_PARENT] =
    g_param_spec_object ("parent",
                         "Parent",
                         "The parent project item, if not the root.",
                         IDE_TYPE_PROJECT_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_project_item_init (IdeProjectItem *self)
{
}
