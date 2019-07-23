/* ide-path-element.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-path-element"

#include "config.h"

#include "ide-path-element.h"

typedef struct
{
  gchar *id;
  gchar *title;
} IdePathElementPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdePathElement, ide_path_element, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

/**
 * ide_path_element_new:
 * @id: the id for the path element
 * @title: the display name for the path element
 *
 * Create a new #IdePathElement.
 *
 * Returns: (transfer full): a newly created #IdePathElement
 *
 * Since: 3.34
 */
IdePathElement *
ide_path_element_new (const gchar *id,
                      const gchar *title)
{
  return g_object_new (IDE_TYPE_PATH_ELEMENT,
                       "id", id,
                       "title", title,
                       NULL);
}

static void
ide_path_element_finalize (GObject *object)
{
  IdePathElement *self = (IdePathElement *)object;
  IdePathElementPrivate *priv = ide_path_element_get_instance_private (self);

  g_clear_pointer (&priv->id, g_free);
  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (ide_path_element_parent_class)->finalize (object);
}

static void
ide_path_element_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdePathElement *self = IDE_PATH_ELEMENT (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, ide_path_element_get_id (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_path_element_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_path_element_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdePathElement *self = IDE_PATH_ELEMENT (object);
  IdePathElementPrivate *priv = ide_path_element_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      priv->title = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_path_element_class_init (IdePathElementClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_path_element_finalize;
  object_class->get_property = ide_path_element_get_property;
  object_class->set_property = ide_path_element_set_property;

  properties [PROP_ID] =
    g_param_spec_string ("id",
                         "Id",
                         "The identifier for the element",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The display title for the element",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_path_element_init (IdePathElement *self)
{
}

const gchar *
ide_path_element_get_id (IdePathElement *self)
{
  IdePathElementPrivate *priv = ide_path_element_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PATH_ELEMENT (self), NULL);

  return priv->id;
}

const gchar *
ide_path_element_get_title (IdePathElement *self)
{
  IdePathElementPrivate *priv = ide_path_element_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_PATH_ELEMENT (self), NULL);

  return priv->title;
}

gboolean
ide_path_element_equal (IdePathElement *self,
                        IdePathElement *other)
{
  return 0 == g_strcmp0 (ide_path_element_get_id (self),
                         ide_path_element_get_id (other));
}
