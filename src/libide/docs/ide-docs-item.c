/* ide-docs-item.c
 *
 * Copyright 2019 Christian Hergert <unknown@domain.org>
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

#define G_LOG_DOMAIN "ide-docs-item"

#include "config.h"

#include "ide-docs-enums.h"
#include "ide-docs-item.h"

typedef struct
{
  IdeDocsItem     *parent;
  GHashTable      *children_index;
  GQueue           children;
  GList            link;
  gchar           *id;
  gchar           *title;
  gchar           *display_name;
  gchar           *since;
  gchar           *url;
  IdeDocsItemKind  kind;
  gint             priority;
  guint            deprecated : 1;
} IdeDocsItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeDocsItem, ide_docs_item, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DEPRECATED,
  PROP_DISPLAY_NAME,
  PROP_ID,
  PROP_KIND,
  PROP_PRIORITY,
  PROP_SINCE,
  PROP_TITLE,
  PROP_URL,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_docs_item_new:
 *
 * Create a new #IdeDocsItem.
 *
 * Returns: (transfer full): a newly created #IdeDocsItem
 *
 * Since: 3.34
 */
IdeDocsItem *
ide_docs_item_new (void)
{
  return g_object_new (IDE_TYPE_DOCS_ITEM, NULL);
}

void
ide_docs_item_remove (IdeDocsItem *self,
                      IdeDocsItem *child)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);
  IdeDocsItemPrivate *child_priv = ide_docs_item_get_instance_private (child);
  const gchar *id;

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (child));
  g_return_if_fail (child_priv->parent == self);

  if ((id = ide_docs_item_get_id (child)))
    g_hash_table_remove (priv->children_index, id);

  g_queue_unlink (&priv->children, &child_priv->link);
  child_priv->parent = NULL;
  g_object_unref (child);
}

static void
ide_docs_item_dispose (GObject *object)
{
  IdeDocsItem *self = (IdeDocsItem *)object;
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  if (priv->parent != NULL)
    ide_docs_item_remove (priv->parent, self);

  G_OBJECT_CLASS (ide_docs_item_parent_class)->dispose (object);
}

static void
ide_docs_item_finalize (GObject *object)
{
  IdeDocsItem *self = (IdeDocsItem *)object;
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->since, g_free);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->title, g_free);
  g_clear_pointer (&priv->url, g_free);

  G_OBJECT_CLASS (ide_docs_item_parent_class)->finalize (object);
}

static void
ide_docs_item_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  IdeDocsItem *self = IDE_DOCS_ITEM (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_docs_item_get_id (self));
      break;

    case PROP_DISPLAY_NAME:
      g_value_set_string (value, ide_docs_item_get_display_name (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_docs_item_get_title (self));
      break;

    case PROP_SINCE:
      g_value_set_string (value, ide_docs_item_get_since (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, ide_docs_item_get_kind (self));
      break;

    case PROP_DEPRECATED:
      g_value_set_boolean (value, ide_docs_item_get_deprecated (self));
      break;

    case PROP_URL:
      g_value_set_string (value, ide_docs_item_get_url (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_item_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  IdeDocsItem *self = IDE_DOCS_ITEM (object);

  switch (prop_id)
    {
    case PROP_ID:
      ide_docs_item_set_id (self, g_value_get_string (value));
      break;

    case PROP_DISPLAY_NAME:
      ide_docs_item_set_display_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_docs_item_set_title (self, g_value_get_string (value));
      break;

    case PROP_SINCE:
      ide_docs_item_set_since (self, g_value_get_string (value));
      break;

    case PROP_KIND:
      ide_docs_item_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_DEPRECATED:
      ide_docs_item_set_deprecated (self, g_value_get_boolean (value));
      break;

    case PROP_URL:
      ide_docs_item_set_url (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_docs_item_class_init (IdeDocsItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_docs_item_dispose;
  object_class->finalize = ide_docs_item_finalize;
  object_class->get_property = ide_docs_item_get_property;
  object_class->set_property = ide_docs_item_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The identifier for the item, if any",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DISPLAY_NAME] =
    g_param_spec_string ("display-name",
                         "Display Name",
                         "The display-name of the item, possibily containing pango markup",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the item",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SINCE] =
    g_param_spec_string ("since",
                         "Since",
                         "When the item is added",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_DEPRECATED] =
    g_param_spec_string ("deprecated",
                         "Deprecated",
                         "When the item was deprecated",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_URL] =
    g_param_spec_string ("url",
                         "Url",
                         "The url for the documentation",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The kind of item",
                       IDE_TYPE_DOCS_ITEM_KIND,
                       IDE_DOCS_ITEM_KIND_NONE,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PRIORITY] =
    g_param_spec_int ("priority",
                      "Priority",
                      "The priority of the item",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_docs_item_init (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  priv->link.data = self;
}

const gchar *
ide_docs_item_get_id (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->id;
}

void
ide_docs_item_set_id (IdeDocsItem *self,
                      const gchar *id)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));
  g_return_if_fail (priv->parent == NULL);

  if (g_strcmp0 (id, priv->id) != 0)
    {
      g_free (priv->id);
      priv->id = g_strdup (id);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ID]);
    }
}

const gchar *
ide_docs_item_get_display_name (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->display_name;
}

void
ide_docs_item_set_display_name (IdeDocsItem *self,
                                const gchar *display_name)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (g_strcmp0 (display_name, priv->display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DISPLAY_NAME]);
    }
}

const gchar *
ide_docs_item_get_title (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->title;
}

void
ide_docs_item_set_title (IdeDocsItem *self,
                         const gchar *title)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (g_strcmp0 (title, priv->title) != 0)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
    }
}

const gchar *
ide_docs_item_get_url (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->url;
}

void
ide_docs_item_set_url (IdeDocsItem *self,
                       const gchar *url)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (g_strcmp0 (url, priv->url) != 0)
    {
      g_free (priv->url);
      priv->url = g_strdup (url);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_URL]);
    }
}

const gchar *
ide_docs_item_get_since (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->since;
}

void
ide_docs_item_set_since (IdeDocsItem *self,
                         const gchar *since)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (g_strcmp0 (since, priv->since) != 0)
    {
      g_free (priv->since);
      priv->since = g_strdup (since);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SINCE]);
    }
}

gboolean
ide_docs_item_get_deprecated (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), FALSE);

  return priv->deprecated;
}

void
ide_docs_item_set_deprecated (IdeDocsItem *self,
                              gboolean     deprecated)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  deprecated = !!deprecated;

  if (deprecated != priv->deprecated)
    {
      priv->deprecated = deprecated;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEPRECATED]);
    }
}

IdeDocsItemKind
ide_docs_item_get_kind (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), FALSE);

  return priv->kind;
}

void
ide_docs_item_set_kind (IdeDocsItem     *self,
                        IdeDocsItemKind  kind)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (kind != priv->kind)
    {
      priv->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

gboolean
ide_docs_item_has_child (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), FALSE);

  return priv->children.length > 0;
}

gboolean
ide_docs_item_is_root (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), FALSE);

  return priv->parent == NULL;
}

/**
 * ide_docs_item_get_parent:
 *
 * Get the parent #IdeDocsItem if set.
 *
 * Returns: (transfer none): an #IdeDocsItem or %NULL
 *
 * Since: 3.34
 */
IdeDocsItem *
ide_docs_item_get_parent (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->parent;
}

/**
 * ide_docs_item_get_n_children:
 * @self: a #IdeDocsItem
 *
 * Gets the nubmer of children #IdeDocsItem contained by @self.
 *
 * Returns: the number of children
 *
 * Since: 3.34
 */
guint
ide_docs_item_get_n_children (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), 0);

  return priv->children.length;
}

/**
 * ide_docs_item_get_children:
 * @self: an #IdeDocsItem
 *
 * Gets a #GList of #IdeDocsItem that are direct children of @self.
 *
 * The result may not be modified or freed.
 *
 * Returns: (transfer none) (element-type Ide.DocsItem): a #GList of
 *   #IdeDocsItem.
 *
 * Since: 3.34
 */
const GList *
ide_docs_item_get_children (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return priv->children.head;
}

static void
maybe_index (IdeDocsItem *self,
             IdeDocsItem *child)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);
  const gchar *id = ide_docs_item_get_id (child);

  if (id != NULL)
    {
      if (priv->children_index == NULL)
        priv->children_index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      g_hash_table_insert (priv->children_index, g_strdup (id), child);
    }
}

void
ide_docs_item_append (IdeDocsItem *self,
                      IdeDocsItem *child)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);
  IdeDocsItemPrivate *child_priv = ide_docs_item_get_instance_private (child);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (child));
  g_return_if_fail (child_priv->parent == NULL);
  g_return_if_fail (child_priv->link.prev == NULL);
  g_return_if_fail (child_priv->link.next == NULL);

  g_object_ref (child);
  child_priv->parent = self;
  g_queue_push_tail_link (&priv->children, &child_priv->link);
  maybe_index (self, child);
}

void
ide_docs_item_prepend (IdeDocsItem *self,
                       IdeDocsItem *child)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);
  IdeDocsItemPrivate *child_priv = ide_docs_item_get_instance_private (child);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));
  g_return_if_fail (IDE_IS_DOCS_ITEM (child));
  g_return_if_fail (child_priv->parent == NULL);
  g_return_if_fail (child_priv->link.prev == NULL);
  g_return_if_fail (child_priv->link.next == NULL);

  g_object_ref (child);
  child_priv->parent = self;
  g_queue_push_head_link (&priv->children, &child_priv->link);
  maybe_index (self, child);
}

gint
ide_docs_item_get_priority (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), 0);

  return priv->priority;
}

void
ide_docs_item_set_priority (IdeDocsItem *self,
                            gint         priority)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (priority != priv->priority)
    {
      priv->priority = priority;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PRIORITY]);
    }
}

/**
 * ide_docs_item_find_child_by_id:
 * @self: a #IdeDocsItem
 * @id: the id of the child to locate
 *
 * Finds a child item based on the id of the child.
 *
 * Returns: (transfer none) (nullable): an #IdeDocsItem or %NULL
 *
 * Since: 3.34
 */
IdeDocsItem *
ide_docs_item_find_child_by_id (IdeDocsItem *self,
                                const gchar *id)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);
  IdeDocsItem *child;

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  if (id == NULL)
    return NULL;

  if (priv->children_index != NULL &&
      (child = g_hash_table_lookup (priv->children_index, id)))
    return child;

  for (const GList *iter = priv->children.head;
       iter != NULL;
       iter = iter->next)
    {
      IdeDocsItemPrivate *child_priv;

      child = iter->data;
      child_priv = ide_docs_item_get_instance_private (child);

      g_assert (IDE_IS_DOCS_ITEM (child));

      if (g_strcmp0 (child_priv->id, id) == 0)
        return child;
    }

  return NULL;
}

static gint
sort_by_priority (IdeDocsItem *a,
                  IdeDocsItem *b)
{
  gint prio_a = ide_docs_item_get_priority (a);
  gint prio_b = ide_docs_item_get_priority (b);

  if (prio_a < prio_b)
    return -1;
  else if (prio_a > prio_b)
    return 1;
  else
    return 0;
}

void
ide_docs_item_sort_by_priority (IdeDocsItem *self)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (priv->children.length != 0)
    g_queue_sort (&priv->children,
                  (GCompareDataFunc) sort_by_priority,
                  NULL);
}

void
ide_docs_item_truncate (IdeDocsItem *self,
                        guint        max_items)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_if_fail (IDE_IS_DOCS_ITEM (self));

  if (max_items == 0)
    return;

  if (max_items >= priv->children.length)
    return;

  while (priv->children.length > max_items)
    ide_docs_item_remove (self, priv->children.tail->data);
}

/**
 * ide_docs_item_get_nth_child:
 * @self: a #IdeDocsItem
 * @nth: the index (starting from zero) of the child
 *
 * Gets the @nth item from the children.
 *
 * Returns: (transfer none) (nullable): an #IdeDocsItem or %NULL
 *
 * Since: 3.34
 */
IdeDocsItem *
ide_docs_item_get_nth_child (IdeDocsItem *self,
                             guint        nth)
{
  IdeDocsItemPrivate *priv = ide_docs_item_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_DOCS_ITEM (self), NULL);

  return g_list_nth_data (priv->children.head, nth);
}
