/* ide-tweaks-item.c
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

#define G_LOG_DOMAIN "ide-tweaks-item"

#include "config.h"

#include <gtk/gtk.h>

#include "ide-tweaks-item.h"
#include "ide-tweaks-item-private.h"

typedef struct
{
  IdeTweaksItem *parent;
  GList link;
  GQueue children;
  char *id;
  char **keywords;
  char *sort_key;
} IdeTweaksItemPrivate;

enum {
  PROP_0,
  PROP_ID,
  PROP_KEYWORDS,
  PROP_SORT_KEY,
  N_PROPS
};

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (IdeTweaksItem, ide_tweaks_item, G_TYPE_OBJECT,
                                  G_ADD_PRIVATE (IdeTweaksItem)
                                  G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_item_dispose (GObject *object)
{
  IdeTweaksItem *self = (IdeTweaksItem *)object;
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  while (priv->children.head != NULL)
    {
      IdeTweaksItem *child = g_queue_peek_head (&priv->children);

      ide_tweaks_item_unparent (child);
    }

  g_assert (priv->children.length == 0);
  g_assert (priv->children.head == NULL);
  g_assert (priv->children.tail == NULL);

  ide_tweaks_item_unparent (self);

  g_clear_pointer (&priv->keywords, g_strfreev);
  g_clear_pointer (&priv->sort_key, g_free);
  g_clear_pointer (&priv->id, g_free);

  G_OBJECT_CLASS (ide_tweaks_item_parent_class)->dispose (object);
}

static void
ide_tweaks_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksItem *self = IDE_TWEAKS_ITEM (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, gtk_buildable_get_buildable_id (GTK_BUILDABLE (self)));
      break;

    case PROP_KEYWORDS:
      g_value_set_boxed (value, ide_tweaks_item_get_keywords (self));
      break;

    case PROP_SORT_KEY:
      g_value_set_string (value, ide_tweaks_item_get_sort_key (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksItem *self = IDE_TWEAKS_ITEM (object);

  switch (prop_id)
    {
    case PROP_KEYWORDS:
      ide_tweaks_item_set_keywords (self, g_value_get_boxed (value));
      break;

    case PROP_SORT_KEY:
      ide_tweaks_item_set_sort_key (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_item_class_init (IdeTweaksItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_item_dispose;
  object_class->get_property = ide_tweaks_item_get_property;
  object_class->set_property = ide_tweaks_item_set_property;

  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEYWORDS] =
    g_param_spec_boxed ("keywords", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SORT_KEY] =
    g_param_spec_string ("sort-key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_item_init (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  priv->link.data = self;
}

static gboolean
ide_tweaks_item_accepts (IdeTweaksItem *self,
                         IdeTweaksItem *child)
{
  if (IDE_TWEAKS_ITEM_GET_CLASS (self)->accepts)
    return IDE_TWEAKS_ITEM_GET_CLASS (self)->accepts (self, child);

  return FALSE;
}

const char *
ide_tweaks_item_get_id (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return priv->id;
}

const char * const *
ide_tweaks_item_get_keywords (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return (const char * const *)priv->keywords;
}

void
ide_tweaks_item_set_keywords (IdeTweaksItem      *self,
                              const char * const *keywords)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));

  if (keywords == (const char * const *)priv->keywords)
    return;

  g_strfreev (priv->keywords);
  priv->keywords = g_strdupv ((char **)keywords);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEYWORDS]);
}

const char *
ide_tweaks_item_get_sort_key (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return priv->sort_key;
}

void
ide_tweaks_item_set_sort_key (IdeTweaksItem *self,
                              const char    *sort_key)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));

  if (ide_set_string (&priv->sort_key, sort_key))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SORT_KEY]);
}

/**
 * ide_tweaks_item_get_first_child:
 * @self: a #IdeTweaksItem
 *
 * Gets the first child of @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_item_get_first_child (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return g_queue_peek_head (&priv->children);
}

/**
 * ide_tweaks_item_get_last_child:
 * @self: a #IdeTweaksItem
 *
 * Gets the last child of @self.
 *
 * Returns: (transfer none) (nullable): a #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_item_get_last_child (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return g_queue_peek_tail (&priv->children);
}

/**
 * ide_tweaks_item_get_previous_sibling:
 * @self: a #IdeTweaksItem
 *
 * Gets the previous sibling within the parent.
 *
 * Returns: (transfer none) (nullable): A #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_item_get_previous_sibling (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  if (priv->link.prev != NULL)
    return priv->link.prev->data;

  return NULL;
}

/**
 * ide_tweaks_item_get_next_sibling:
 * @self: a #IdeTweaksItem
 *
 * Gets the next sibling within the parent.
 *
 * Returns: (transfer none) (nullable): A #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_item_get_next_sibling (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  if (priv->link.next != NULL)
    return priv->link.next->data;

  return NULL;
}

/*
 * ide_tweaks_item_insert_after:
 * @self: (transfer full): a #IdeTweaksItem
 * @parent: the #IdeTweaksItem to add @self to
 * @previous_sibling: (nullable): a #IdeTweaksItem or %NULL to append @self
 *
 * Adds @self to the children of @parent, immediately after @previous_sibling.
 *
 * If @previous_sibling is %NULL, then @self is appended.
 */
void
ide_tweaks_item_insert_after (IdeTweaksItem *self,
                              IdeTweaksItem *parent,
                              IdeTweaksItem *previous_sibling)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);
  IdeTweaksItemPrivate *parent_priv = ide_tweaks_item_get_instance_private (parent);
  IdeTweaksItemPrivate *previous_priv = ide_tweaks_item_get_instance_private (previous_sibling);

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));
  g_return_if_fail (IDE_IS_TWEAKS_ITEM (parent));
  g_return_if_fail (!previous_sibling || IDE_IS_TWEAKS_ITEM (previous_sibling));
  g_return_if_fail (!previous_sibling || ide_tweaks_item_get_parent (previous_sibling) == parent);
  g_return_if_fail (priv->link.data == self);
  g_return_if_fail (parent_priv->link.data == parent);
  g_return_if_fail (priv->parent == NULL);

  g_object_ref (self);

  priv->parent = parent;

  if (previous_sibling != NULL)
    g_queue_insert_after_link (&parent_priv->children, &previous_priv->link, &priv->link);
  else
    g_queue_push_tail_link (&parent_priv->children, &priv->link);
}

/*
 * ide_tweaks_item_insert_before:
 * @self: (transfer full): a #IdeTweaksItem
 * @parent: the #IdeTweaksItem to add @self to
 * @next_sibling: (nullable): a #IdeTweaksItem or %NULL to append @self
 *
 * Adds @self to the children of @parent, immediately before @next_sibling.
 *
 * If @previous_sibling is %NULL, then @self is prepended.
 */
void
ide_tweaks_item_insert_before (IdeTweaksItem *self,
                               IdeTweaksItem *parent,
                               IdeTweaksItem *next_sibling)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);
  IdeTweaksItemPrivate *parent_priv = ide_tweaks_item_get_instance_private (parent);
  IdeTweaksItemPrivate *next_priv = ide_tweaks_item_get_instance_private (next_sibling);

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));
  g_return_if_fail (IDE_IS_TWEAKS_ITEM (parent));
  g_return_if_fail (!next_sibling || IDE_IS_TWEAKS_ITEM (next_sibling));
  g_return_if_fail (!next_sibling || ide_tweaks_item_get_parent (next_sibling) == parent);
  g_return_if_fail (priv->link.data == self);
  g_return_if_fail (parent_priv->link.data == parent);
  g_return_if_fail (priv->parent == NULL);

  g_object_ref (self);

  priv->parent = parent;

  if (next_sibling != NULL)
    g_queue_insert_before_link (&parent_priv->children, &next_priv->link, &priv->link);
  else
    g_queue_push_head_link (&parent_priv->children, &priv->link);
}

/**
 * ide_tweaks_item_get_parent:
 * @self: a #IdeTweaksItem
 *
 * Gets the parent #IdeTweaksItem
 *
 * Returns: (transfer none) (nullable): the parent #IdeTweaksItem or %NULL
 */
IdeTweaksItem *
ide_tweaks_item_get_parent (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_TWEAKS_ITEM (self), NULL);

  return priv->parent;
}

void
ide_tweaks_item_unparent (IdeTweaksItem *self)
{
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);
  IdeTweaksItemPrivate *parent_priv;
  IdeTweaksItem *parent;

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));
  g_return_if_fail (priv->parent == NULL || IDE_IS_TWEAKS_ITEM (priv->parent));

  if (priv->parent == NULL)
    return;

  parent = priv->parent;
  parent_priv = ide_tweaks_item_get_instance_private (parent);

  g_queue_unlink (&parent_priv->children, &priv->link);
  priv->parent = NULL;

  g_object_unref (self);
}

static void
ide_tweaks_item_add_child (GtkBuildable *buildable,
                           GtkBuilder   *builder,
                           GObject      *child,
                           const char   *type)
{
  IdeTweaksItem *self = (IdeTweaksItem *)buildable;

  g_assert (IDE_IS_TWEAKS_ITEM (self));
  g_assert (G_IS_OBJECT (child));

  if (!IDE_IS_TWEAKS_ITEM (child))
    {
      g_warning ("Attempt to add %s as child of %s, which is not an IdeTweaksItem",
                 G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (self));
      return;
    }

  if (!ide_tweaks_item_accepts (self, IDE_TWEAKS_ITEM (child)))
    {
      g_warning ("Attempt to add %s as child of %s, but that is not allowed",
                 G_OBJECT_TYPE_NAME (child), G_OBJECT_TYPE_NAME (self));
      return;
    }

  ide_tweaks_item_insert_after (IDE_TWEAKS_ITEM (child), self, NULL);
}

static GObject *
ide_tweaks_item_get_internal_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    const char   *child_name)
{
  IdeTweaksItem *self = (IdeTweaksItem *)buildable;

  g_assert (IDE_IS_TWEAKS_ITEM (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (child_name != NULL);

  for (IdeTweaksItem *child = ide_tweaks_item_get_first_child (self);
       child != NULL;
       child = ide_tweaks_item_get_next_sibling (child))
    {
      const char *buildable_id = gtk_buildable_get_buildable_id (GTK_BUILDABLE (child));

      if (ide_str_equal0 (buildable_id, child_name))
        return G_OBJECT (child);
    }

  return NULL;
}

static const char *
ide_tweaks_item_get_buildable_id (GtkBuildable *buildable)
{
  IdeTweaksItem *self = (IdeTweaksItem *)buildable;
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_assert (IDE_IS_TWEAKS_ITEM (self));

  return priv->id;
}

static void
ide_tweaks_item_set_buildable_id (GtkBuildable *buildable,
                                  const char   *buildable_id)
{
  IdeTweaksItem *self = (IdeTweaksItem *)buildable;
  IdeTweaksItemPrivate *priv = ide_tweaks_item_get_instance_private (self);

  g_assert (IDE_IS_TWEAKS_ITEM (self));
  g_assert (buildable_id != NULL);

  if (priv->id == NULL)
    priv->id = g_strdup (buildable_id);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  iface->add_child = ide_tweaks_item_add_child;
  iface->get_internal_child = ide_tweaks_item_get_internal_child;
  iface->get_id = ide_tweaks_item_get_buildable_id;
  iface->set_id = ide_tweaks_item_set_buildable_id;
}

static int
compare_pspec (gconstpointer a,
               gconstpointer b)
{
  const GParamSpec * const *pspec_a = a;
  const GParamSpec * const *pspec_b = b;

  return g_strcmp0 ((*pspec_a)->name, (*pspec_b)->name);
}

void
_ide_tweaks_item_printf (IdeTweaksItem *self,
                         GString       *string,
                         guint          level)
{
  g_autofree GParamSpec **pspecs = NULL;
  guint n_pspecs;

  g_return_if_fail (IDE_IS_TWEAKS_ITEM (self));
  g_return_if_fail (string != NULL);

  for (guint i = 0; i < level; i++)
    g_string_append (string, "  ");
  g_string_append_printf (string, "<%s id=\"%s\"",
                          G_OBJECT_TYPE_NAME (self),
                          gtk_buildable_get_buildable_id (GTK_BUILDABLE (self)));

  pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (self), &n_pspecs);

  qsort (pspecs, n_pspecs, sizeof (pspecs[0]), compare_pspec);

  for (guint i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];

      if (ide_str_equal0 (pspec->name, "id"))
        continue;

      if (pspec->flags & G_PARAM_READABLE)
        {
          if (g_value_type_transformable (pspec->value_type, G_TYPE_STRING))
            {
              g_auto(GValue) value = G_VALUE_INIT;
              g_autofree char *copy = NULL;

              g_value_init (&value, G_TYPE_STRING);
              g_object_get_property (G_OBJECT (self), pspec->name, &value);

              if (g_value_get_string (&value))
                copy = g_strescape (g_value_get_string (&value), NULL);

              g_string_append_printf (string, " %s=\"%s\"", pspec->name, copy ? copy : "");
            }
          else if (g_type_is_a (pspec->value_type, G_TYPE_OBJECT))
            {
              g_auto(GValue) value = G_VALUE_INIT;
              g_autofree char *generic = NULL;
              const char *type_name = "";
              GObject *obj;

              g_value_init (&value, G_TYPE_OBJECT);
              g_object_get_property (G_OBJECT (self), pspec->name, &value);

              if (!(obj = g_value_get_object (&value)) || !G_IS_LIST_MODEL (obj))
                continue;

              type_name = generic = g_strdup_printf ("%s<%s>",
                                                     G_OBJECT_TYPE_NAME (obj),
                                                     g_type_name (g_list_model_get_item_type (G_LIST_MODEL (obj))));

              g_string_append_printf (string, " %s=\"%s\"", pspec->name, type_name);
            }
        }
    }

  if (ide_tweaks_item_get_first_child (self) == NULL)
    {
      g_string_append (string, "/>\n");
      return;
    }

  g_string_append (string, ">\n");

  for (IdeTweaksItem *child = ide_tweaks_item_get_first_child (self);
       child != NULL;
       child = ide_tweaks_item_get_next_sibling (child))
    _ide_tweaks_item_printf (child, string, level+1);

  for (guint i = 0; i < level; i++)
    g_string_append (string, "  ");
  g_string_append_printf (string, "</%s>\n", G_OBJECT_TYPE_NAME (self));
}

IdeTweaksItem *
_ide_tweaks_item_deep_copy (IdeTweaksItem *self)
{
  /* TODO: actually implement deep copy */

  return g_object_new (G_OBJECT_TYPE (self), NULL);
}
