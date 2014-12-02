/* gb-navigation-item.c
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

#include <glib/gi18n.h>

#include "gb-navigation-item.h"

struct _GbNavigationItemPrivate
{
  gchar       *label;
  GbWorkspace *workspace;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbNavigationItem, gb_navigation_item,
                            G_TYPE_INITIALLY_UNOWNED)

enum {
  PROP_0,
  PROP_LABEL,
  PROP_WORKSPACE,
  LAST_PROP
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GbNavigationItem *
gb_navigation_item_new (const gchar *label)
{
  return g_object_new (GB_TYPE_NAVIGATION_ITEM,
                       "label", label,
                       NULL);
}

const gchar *
gb_navigation_item_get_label (GbNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_NAVIGATION_ITEM (item), NULL);

  return item->priv->label;
}

void
gb_navigation_item_set_label (GbNavigationItem *item,
                              const gchar      *label)
{
  g_return_if_fail (GB_IS_NAVIGATION_ITEM (item));

  g_free (item->priv->label);
  item->priv->label = g_strdup (label);
  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_LABEL]);
}

GbWorkspace *
gb_navigation_item_get_workspace (GbNavigationItem *item)
{
  g_return_val_if_fail (GB_IS_NAVIGATION_ITEM (item), NULL);

  return item->priv->workspace;
}

static void
gb_navigation_item_set_workspace (GbNavigationItem *item,
                                  GbWorkspace      *workspace)
{
  g_return_if_fail (GB_IS_NAVIGATION_ITEM (item));
  g_return_if_fail (!workspace || GB_IS_WORKSPACE (workspace));

  if (item->priv->workspace)
    {
      g_object_remove_weak_pointer (G_OBJECT (item),
                                    (gpointer *)&item->priv->workspace);
      item->priv->workspace = NULL;
    }

  if (workspace)
    {
      item->priv->workspace = workspace;
      g_object_add_weak_pointer (G_OBJECT (workspace),
                                 (gpointer *)&item->priv->workspace);
    }

  g_object_notify_by_pspec (G_OBJECT (item), gParamSpecs [PROP_WORKSPACE]);
}



void
gb_navigation_item_activate (GbNavigationItem *item)
{
  g_return_if_fail (GB_IS_NAVIGATION_ITEM (item));

  g_signal_emit (item, gSignals [ACTIVATE], 0);
}

static void
gb_navigation_item_finalize (GObject *object)
{
  GbNavigationItemPrivate *priv = GB_NAVIGATION_ITEM (object)->priv;

  g_clear_pointer (&priv->label, g_free);

  if (priv->workspace)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->workspace),
                                    (gpointer *)&priv->workspace);
      priv->workspace = NULL;
    }

  G_OBJECT_CLASS (gb_navigation_item_parent_class)->finalize (object);
}

static void
gb_navigation_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbNavigationItem *self = GB_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gb_navigation_item_get_label (self));
      break;

    case PROP_WORKSPACE:
      g_value_set_object (value, gb_navigation_item_get_workspace (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_navigation_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GbNavigationItem *self = GB_NAVIGATION_ITEM (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      gb_navigation_item_set_label (self, g_value_get_string (value));
      break;

    case PROP_WORKSPACE:
      gb_navigation_item_set_workspace (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_navigation_item_class_init (GbNavigationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_navigation_item_finalize;
  object_class->get_property = gb_navigation_item_get_property;
  object_class->set_property = gb_navigation_item_set_property;

  /**
   * GbNavigationItem:label:
   *
   * The "label" for the item within the navigation list.
   */
  gParamSpecs [PROP_LABEL] =
    g_param_spec_string ("label",
                         _("Label"),
                         _("The label for the navigation item."),
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_LABEL,
                                   gParamSpecs [PROP_LABEL]);

  gParamSpecs [PROP_WORKSPACE] =
    g_param_spec_object ("workspace",
                         _("Workspace"),
                         _("The workspace to ensure is focused."),
                         GB_TYPE_WORKSPACE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_WORKSPACE,
                                   gParamSpecs [PROP_WORKSPACE]);

  /**
   * GbNavigationItem::activate:
   *
   * This signal is emitted when the navigation item should be navigated
   * to. The subscriber should change to their respective workspace and focus
   * anything necessary to view the represented state.
   */
  gSignals [ACTIVATE] =
    g_signal_new ("activate",
                  GB_TYPE_NAVIGATION_ITEM,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbNavigationItemClass, activate),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
gb_navigation_item_init (GbNavigationItem *self)
{
  self->priv = gb_navigation_item_get_instance_private (self);
}
