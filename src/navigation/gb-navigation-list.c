/* gb-navigation-list.c
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

#include "gb-navigation-list.h"

struct _GbNavigationListPrivate
{
  GPtrArray *items;
  gint       current;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbNavigationList, gb_navigation_list, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CAN_GO_BACKWARD,
  PROP_CAN_GO_FORWARD,
  PROP_CURRENT_ITEM,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

GbNavigationList *
gb_navigation_list_new (void)
{
  return g_object_new (GB_TYPE_NAVIGATION_LIST, NULL);
}

gboolean
gb_navigation_list_get_can_go_backward (GbNavigationList *list)
{
  g_return_val_if_fail (GB_IS_NAVIGATION_LIST (list), FALSE);

  return (list->priv->current > 0);
}

gboolean
gb_navigation_list_get_can_go_forward (GbNavigationList *list)
{
  g_return_val_if_fail (GB_IS_NAVIGATION_LIST (list), FALSE);

  return ((list->priv->current + 1) < list->priv->items->len);
}

void
gb_navigation_list_go_backward (GbNavigationList *list)
{
  g_return_if_fail (GB_IS_NAVIGATION_LIST (list));

  if (gb_navigation_list_get_can_go_backward (list))
    {
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CURRENT_ITEM]);
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CAN_GO_BACKWARD]);
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CAN_GO_FORWARD]);
    }
}

void
gb_navigation_list_go_forward (GbNavigationList *list)
{
  g_return_if_fail (GB_IS_NAVIGATION_LIST (list));

  if (gb_navigation_list_get_can_go_forward (list))
    {
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CURRENT_ITEM]);
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CAN_GO_BACKWARD]);
      g_object_notify_by_pspec (G_OBJECT (list),
                                gParamSpecs [PROP_CAN_GO_FORWARD]);
    }
}

GbNavigationItem *
gb_navigation_list_get_current_item (GbNavigationList *list)
{
  g_return_val_if_fail (GB_IS_NAVIGATION_LIST (list), NULL);

  if (list->priv->current < list->priv->items->len)
    return g_ptr_array_index (list->priv->items, list->priv->current);

  return NULL;
}

static void
gb_navigation_list_finalize (GObject *object)
{
  GbNavigationListPrivate *priv = GB_NAVIGATION_LIST (object)->priv;

  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (gb_navigation_list_parent_class)->finalize (object);
}

static void
gb_navigation_list_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GbNavigationList *self = GB_NAVIGATION_LIST (object);

  switch (prop_id)
    {
    case PROP_CAN_GO_BACKWARD:
      g_value_set_boolean (value, gb_navigation_list_get_can_go_backward (self));
      break;

    case PROP_CAN_GO_FORWARD:
      g_value_set_boolean (value, gb_navigation_list_get_can_go_forward (self));
      break;

    case PROP_CURRENT_ITEM:
      g_value_set_object (value, gb_navigation_list_get_current_item (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_navigation_list_class_init (GbNavigationListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_navigation_list_finalize;
  object_class->get_property = gb_navigation_list_get_property;

  gParamSpecs [PROP_CAN_GO_BACKWARD] =
    g_param_spec_boolean ("can-go-backward",
                          _("Can Go Backward"),
                          _("If we can go backwards in the navigation list."),
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_GO_BACKWARD,
                                   gParamSpecs [PROP_CAN_GO_BACKWARD]);

  gParamSpecs [PROP_CAN_GO_FORWARD] =
    g_param_spec_boolean ("can-go-forward",
                          _("Can Go Forward"),
                          _("If we can go forward in the navigation list."),
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_GO_FORWARD,
                                   gParamSpecs [PROP_CAN_GO_FORWARD]);

  gParamSpecs [PROP_CURRENT_ITEM] =
    g_param_spec_object ("current-item",
                         _("Current Item"),
                         _("The current item in the navigation list."),
                         GB_TYPE_NAVIGATION_ITEM,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CURRENT_ITEM,
                                   gParamSpecs [PROP_CURRENT_ITEM]);
}

static void
gb_navigation_list_init (GbNavigationList *self)
{
  self->priv = gb_navigation_list_get_instance_private (self);
  self->priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}
