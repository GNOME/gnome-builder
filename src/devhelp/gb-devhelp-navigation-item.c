/* gb-devhelp-navigation-item.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "devhelp-navigation"

#include <glib/gi18n.h>

#include "gb-devhelp-navigation-item.h"
#include "gb-devhelp-workspace.h"
#include "gb-log.h"

struct _GbDevhelpNavigationItemPrivate
{
  gchar *uri;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbDevhelpNavigationItem,
                            gb_devhelp_navigation_item,
                            GB_TYPE_NAVIGATION_ITEM)

enum {
  PROP_0,
  PROP_URI,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

const gchar *
gb_devhelp_navigation_item_get_uri (GbDevhelpNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_DEVHELP_NAVIGATION_ITEM (item), NULL);

  return item->priv->uri;
}

static void
gb_devhelp_navigation_item_activate (GbNavigationItem *item)
{
  GbDevhelpNavigationItem *nav = (GbDevhelpNavigationItem *)item;
  GbWorkspace *workspace;

  ENTRY;

  g_return_if_fail (GB_IS_DEVHELP_NAVIGATION_ITEM (nav));

  workspace = gb_navigation_item_get_workspace (item);

  if (GB_IS_DEVHELP_WORKSPACE (workspace))
    gb_devhelp_workspace_open_uri (GB_DEVHELP_WORKSPACE (workspace),
                                   nav->priv->uri);

  EXIT;
}

void
gb_devhelp_navigation_item_set_uri (GbDevhelpNavigationItem *item,
                                    const gchar             *uri)
{
  g_return_if_fail (GB_IS_DEVHELP_NAVIGATION_ITEM (item));

  g_free (item->priv->uri);
  item->priv->uri = g_strdup (uri);
  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_URI]);
}

static void
gb_devhelp_navigation_item_finalize (GObject *object)
{
  GbDevhelpNavigationItemPrivate *priv = GB_DEVHELP_NAVIGATION_ITEM (object)->priv;

  g_clear_pointer (&priv->uri, g_free);

  G_OBJECT_CLASS (gb_devhelp_navigation_item_parent_class)->finalize (object);
}

static void
gb_devhelp_navigation_item_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GbDevhelpNavigationItem *self = GB_DEVHELP_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, gb_devhelp_navigation_item_get_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_navigation_item_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GbDevhelpNavigationItem *self = GB_DEVHELP_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_URI:
      gb_devhelp_navigation_item_set_uri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_devhelp_navigation_item_class_init (GbDevhelpNavigationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GbNavigationItemClass *item_class = GB_NAVIGATION_ITEM_CLASS (klass);

  object_class->finalize = gb_devhelp_navigation_item_finalize;
  object_class->get_property = gb_devhelp_navigation_item_get_property;
  object_class->set_property = gb_devhelp_navigation_item_set_property;

  item_class->activate = gb_devhelp_navigation_item_activate;

  gParamSpecs [PROP_URI] =
    g_param_spec_string ("uri",
                         _("URI"),
                         _("The uri of the devhelp link."),
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_URI,
                                   gParamSpecs [PROP_URI]);
}

static void
gb_devhelp_navigation_item_init (GbDevhelpNavigationItem *self)
{
  self->priv = gb_devhelp_navigation_item_get_instance_private (self);
}
