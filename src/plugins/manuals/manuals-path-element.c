/*
 * manuals-path-element.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include "manuals-path-element.h"

enum {
  PROP_0,
  PROP_ICON,
  PROP_IS_LEAF,
  PROP_IS_ROOT,
  PROP_ITEM,
  PROP_SHOW_ICON,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (ManualsPathElement, manuals_path_element, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
manuals_path_element_finalize (GObject *object)
{
  ManualsPathElement *self = (ManualsPathElement *)object;

  g_clear_object (&self->item);
  g_clear_object (&self->icon);
  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (manuals_path_element_parent_class)->finalize (object);
}

static void
manuals_path_element_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ManualsPathElement *self = MANUALS_PATH_ELEMENT (object);

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, manuals_path_element_get_icon (self));
      break;

    case PROP_ITEM:
      g_value_set_object (value, manuals_path_element_get_item (self));
      break;

    case PROP_IS_LEAF:
      g_value_set_boolean (value, self->is_leaf);
      break;

    case PROP_IS_ROOT:
      g_value_set_boolean (value, self->is_root);
      break;

    case PROP_TITLE:
      g_value_set_string (value, manuals_path_element_get_title (self));
      break;

    case PROP_SHOW_ICON:
      g_value_set_boolean (value, manuals_path_element_get_icon (self) != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_element_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ManualsPathElement *self = MANUALS_PATH_ELEMENT (object);

  switch (prop_id)
    {
    case PROP_ICON:
      manuals_path_element_set_icon (self, g_value_get_object (value));
      break;

    case PROP_IS_LEAF:
      self->is_leaf = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_IS_ROOT:
      self->is_root = g_value_get_boolean (value);
      g_object_notify_by_pspec (object, pspec);
      break;

    case PROP_ITEM:
      manuals_path_element_set_item (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      manuals_path_element_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_path_element_class_init (ManualsPathElementClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = manuals_path_element_finalize;
  object_class->get_property = manuals_path_element_get_property;
  object_class->set_property = manuals_path_element_set_property;

  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_IS_LEAF] =
    g_param_spec_boolean ("is-leaf", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_IS_ROOT] =
    g_param_spec_boolean ("is-root", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         G_TYPE_OBJECT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SHOW_ICON] =
    g_param_spec_boolean ("show-icon", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
manuals_path_element_init (ManualsPathElement *self)
{
}

ManualsPathElement *
manuals_path_element_new (void)
{
  return g_object_new (MANUALS_TYPE_PATH_ELEMENT, NULL);
}

/**
 * manuals_path_element_get_item:
 * @self: a #ManualsPathElement
 *
 * Returns: (transfer none) (type GObject) (nullable): a #GObject or %NULL
 */
gpointer
manuals_path_element_get_item (ManualsPathElement *self)
{
  g_return_val_if_fail (MANUALS_IS_PATH_ELEMENT (self), NULL);

  return self->item;
}

GIcon *
manuals_path_element_get_icon (ManualsPathElement *self)
{
  g_return_val_if_fail (MANUALS_IS_PATH_ELEMENT (self), NULL);

  return self->icon;
}

gboolean
manuals_path_element_get_show_icon (ManualsPathElement *self)
{
  g_return_val_if_fail (MANUALS_IS_PATH_ELEMENT (self), FALSE);

  return self->icon != NULL;
}

const char *
manuals_path_element_get_title (ManualsPathElement *self)
{
  g_return_val_if_fail (MANUALS_IS_PATH_ELEMENT (self), NULL);

  return self->title;
}

void
manuals_path_element_set_icon (ManualsPathElement *self,
                               GIcon              *icon)
{
  g_return_if_fail (MANUALS_IS_PATH_ELEMENT (self));
  g_return_if_fail (!icon || G_IS_ICON (icon));

  if (g_set_object (&self->icon, icon))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ICON]);
    }
}

/**
 * manuals_path_element_set_item:
 * @self: a #ManualsPathElement
 * @item: (nullable) (type GObject): a #GObject or %NULL
 *
 */
void
manuals_path_element_set_item (ManualsPathElement *self,
                               gpointer            item)
{
  g_return_if_fail (MANUALS_IS_PATH_ELEMENT (self));
  g_return_if_fail (!item || G_IS_OBJECT (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

void
manuals_path_element_set_title (ManualsPathElement *self,
                                const char         *title)
{
  g_return_if_fail (MANUALS_IS_PATH_ELEMENT (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}
