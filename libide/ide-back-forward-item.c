/* ide-back-forward-item.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "ide-back-forward-item.h"
#include "ide-source-location.h"

typedef struct
{
  IdeSourceLocation *location;
} IdeBackForwardItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBackForwardItem, ide_back_forward_item, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LOCATION,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

IdeBackForwardItem *
ide_back_forward_item_new (IdeContext        *context,
                           IdeSourceLocation *location)
{
  return g_object_new (IDE_TYPE_BACK_FORWARD_ITEM,
                       "context", context,
                       "location", location,
                       NULL);
}

IdeSourceLocation *
ide_back_forward_item_get_location (IdeBackForwardItem *self)
{
  IdeBackForwardItemPrivate *priv;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_ITEM (self), NULL);

  priv = ide_back_forward_item_get_instance_private (self);

  return priv->location;
}

static void
ide_back_forward_item_set_location (IdeBackForwardItem *self,
                                    IdeSourceLocation  *location)
{
  IdeBackForwardItemPrivate *priv;

  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (self));
  g_return_if_fail (location);

  priv = ide_back_forward_item_get_instance_private (self);

  if (location != priv->location)
    {
      g_clear_pointer (&priv->location, ide_source_location_unref);
      priv->location = ide_source_location_ref (location);
    }
}

static void
ide_back_forward_item_finalize (GObject *object)
{
  IdeBackForwardItem *self = (IdeBackForwardItem *)object;
  IdeBackForwardItemPrivate *priv = ide_back_forward_item_get_instance_private (self);

  g_clear_pointer (&priv->location, ide_source_location_unref);

  G_OBJECT_CLASS (ide_back_forward_item_parent_class)->finalize (object);
}

static void
ide_back_forward_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      g_value_set_boxed (value, ide_back_forward_item_get_location (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeBackForwardItem *self = IDE_BACK_FORWARD_ITEM (object);

  switch (prop_id)
    {
    case PROP_LOCATION:
      ide_back_forward_item_set_location (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_item_class_init (IdeBackForwardItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_back_forward_item_finalize;
  object_class->get_property = ide_back_forward_item_get_property;
  object_class->set_property = ide_back_forward_item_set_property;

  /**
   * IdeBackForwardItem:location:
   *
   * The #IdeBackForwardItem:location property contains the location within
   * a source file to navigate to.
   */
  gParamSpecs [PROP_LOCATION] =
    g_param_spec_boxed ("location",
                        _("Location"),
                        _("The location of the navigation item."),
                        IDE_TYPE_SOURCE_LOCATION,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LOCATION,
                                   gParamSpecs [PROP_LOCATION]);
}

static void
ide_back_forward_item_init (IdeBackForwardItem *self)
{
}
