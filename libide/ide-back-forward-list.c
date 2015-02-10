/* ide-back-forward-list.c
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
#include "ide-back-forward-list.h"

typedef struct
{
  GQueue *backward;
  GQueue *forward;
} IdeBackForwardListPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBackForwardList, ide_back_forward_list, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CAN_GO_BACKWARD,
  PROP_CAN_GO_FORWARD,
  LAST_PROP
};

enum {
  NAVIGATE_TO,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
ide_back_forward_list_navigate_to (IdeBackForwardList *self,
                                   IdeBackForwardItem *item)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (item));

  g_signal_emit (self, gSignals [NAVIGATE_TO], 0, item);
}

void
ide_back_forward_list_go_backward (IdeBackForwardList *self)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));

}

void
ide_back_forward_list_go_forward (IdeBackForwardList *self)
{
  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));

}

gboolean
ide_back_forward_list_get_can_go_backward (IdeBackForwardList *self)
{
  IdeBackForwardListPrivate *priv;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);

  priv = ide_back_forward_list_get_instance_private (self);

  return (priv->backward->length > 0);
}

gboolean
ide_back_forward_list_get_can_go_forward (IdeBackForwardList *self)
{
  IdeBackForwardListPrivate *priv;

  g_return_val_if_fail (IDE_IS_BACK_FORWARD_LIST (self), FALSE);

  priv = ide_back_forward_list_get_instance_private (self);

  return (priv->forward->length > 0);
}

void
ide_back_forward_list_push (IdeBackForwardList *self,
                            IdeBackForwardItem *item)
{
  IdeBackForwardListPrivate *priv;
  IdeBackForwardItem *tmp;

  g_return_if_fail (IDE_IS_BACK_FORWARD_LIST (self));
  g_return_if_fail (IDE_IS_BACK_FORWARD_ITEM (item));

  priv = ide_back_forward_list_get_instance_private (self);

  if (priv->forward->length > 0)
    {
      while ((tmp = g_queue_pop_head (priv->forward)))
        g_queue_push_head (priv->backward, tmp);
    }

  if (!(tmp = g_queue_peek_head (priv->backward)) ||
      !ide_back_forward_item_chain (tmp, item))
    g_queue_push_head (priv->backward, g_object_ref (item));

  g_object_notify_by_pspec (G_OBJECT (self),
                            gParamSpecs [PROP_CAN_GO_BACKWARD]);
  g_object_notify_by_pspec (G_OBJECT (self),
                            gParamSpecs [PROP_CAN_GO_FORWARD]);
}

IdeBackForwardList *
ide_back_forward_list_branch (IdeBackForwardList *list)
{
  return NULL;
}

void
ide_back_forward_list_merge (IdeBackForwardList *list,
                             IdeBackForwardList *branch)
{
}

static void
ide_back_forward_list_dispose (GObject *object)
{
  IdeBackForwardList *self = (IdeBackForwardList *)object;
  IdeBackForwardListPrivate *priv = ide_back_forward_list_get_instance_private (self);

  g_clear_pointer (&priv->backward, g_queue_free);
  g_clear_pointer (&priv->forward, g_queue_free);

  G_OBJECT_CLASS (ide_back_forward_list_parent_class)->dispose (object);
}

static void
ide_back_forward_list_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeBackForwardList *self = IDE_BACK_FORWARD_LIST (object);

  switch (prop_id)
    {
    case PROP_CAN_GO_BACKWARD:
      g_value_set_boolean (value,
                           ide_back_forward_list_get_can_go_backward (self));
      break;

    case PROP_CAN_GO_FORWARD:
      g_value_set_boolean (value,
                           ide_back_forward_list_get_can_go_forward (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_back_forward_list_class_init (IdeBackForwardListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_back_forward_list_dispose;
  object_class->get_property = ide_back_forward_list_get_property;

  gParamSpecs [PROP_CAN_GO_BACKWARD] =
    g_param_spec_boolean ("can-go-backward",
                          _("Can Go Backward"),
                          _("If there are more backward navigation items."),
                          IDE_TYPE_BACK_FORWARD_ITEM,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_GO_BACKWARD,
                                   gParamSpecs [PROP_CAN_GO_BACKWARD]);

  gParamSpecs [PROP_CAN_GO_FORWARD] =
    g_param_spec_boolean ("can-go-forward",
                          _("Can Go Forward"),
                          _("If there are more forward navigation items."),
                          IDE_TYPE_BACK_FORWARD_ITEM,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CAN_GO_FORWARD,
                                   gParamSpecs [PROP_CAN_GO_FORWARD]);

  gSignals [NAVIGATE_TO] =
    g_signal_new ("navigate-to",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_BACK_FORWARD_ITEM);
}

static void
ide_back_forward_list_init (IdeBackForwardList *self)
{
  IdeBackForwardListPrivate *priv;

  priv = ide_back_forward_list_get_instance_private (self);

  priv->backward = g_queue_new ();
  priv->forward = g_queue_new ();
}
