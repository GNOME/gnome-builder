/* ide-notification-stack.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-notification-stack"

#include "config.h"

#include <dazzle.h>

#include "ide-notification-stack-private.h"
#include "ide-notification-view-private.h"

#define CAROUSEL_TIMEOUT_SECS 5
#define TRANSITION_DURATION   500

struct _IdeNotificationStack
{
  GtkStack         parent_instance;
  DzlSignalGroup  *signals;
  DzlBindingGroup *bindings;
  GListModel      *model;
  gdouble          progress;
  guint            carousel_source;
  guint            in_carousel : 1;
};

enum {
  PROP_0,
  PROP_PROGRESS,
  N_PROPS
};

enum {
  CHANGED,
  N_SIGNALS
};

G_DEFINE_TYPE (IdeNotificationStack, ide_notification_stack, GTK_TYPE_STACK)

static guint signals [N_SIGNALS];
static GParamSpec *properties [N_PROPS];

static void
ide_notification_stack_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeNotificationStack *self = IDE_NOTIFICATION_STACK (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      g_value_set_double (value, ide_notification_stack_get_progress (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_notification_stack_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeNotificationStack *self = IDE_NOTIFICATION_STACK (object);

  switch (prop_id)
    {
    case PROP_PROGRESS:
      ide_notification_stack_set_progress (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
ide_notification_stack_carousel_cb (gpointer data)
{
  IdeNotificationStack *self = data;

  g_assert (IDE_IS_NOTIFICATION_STACK (self));

  self->in_carousel = TRUE;
  ide_notification_stack_move_next (self);
  self->in_carousel = FALSE;

  return G_SOURCE_CONTINUE;
}

static void
ide_notification_stack_items_changed_cb (IdeNotificationStack *self,
                                         guint                 position,
                                         guint                 removed,
                                         guint                 added,
                                         GListModel           *model)
{
  GtkWidget *urgent = NULL;
  GList *children;
  GList *iter;

  g_assert (IDE_IS_NOTIFICATION_STACK (self));

  children = gtk_container_get_children (GTK_CONTAINER (self));
  iter = g_list_nth (children, position);

  for (guint i = 0; i < removed; i++, iter = iter->next)
    {
      GtkWidget *child = iter->data;
      gtk_widget_destroy (child);
    }

  g_list_free (children);

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(IdeNotification) notif = g_list_model_get_item (model, position + i);
      GtkWidget *view = g_object_new (IDE_TYPE_NOTIFICATION_VIEW,
                                      "notification", notif,
                                      "visible", TRUE,
                                      NULL);

      gtk_container_add_with_properties (GTK_CONTAINER (self), view,
                                         "position", position + i,
                                         NULL);

      if (!urgent && ide_notification_get_urgent (notif))
        urgent = view;
    }

  if (urgent != NULL)
    {
      gtk_stack_set_visible_child (GTK_STACK (self), urgent);
      g_clear_handle_id (&self->carousel_source, g_source_remove);
    }

  if (self->carousel_source == 0 && g_list_model_get_n_items (model))
    self->carousel_source = g_timeout_add_seconds (CAROUSEL_TIMEOUT_SECS,
                                                   ide_notification_stack_carousel_cb,
                                                   self);

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
ide_notification_stack_notify_visible_child (IdeNotificationStack *self)
{
  g_assert (IDE_IS_NOTIFICATION_STACK (self));

  self->progress = 0.0;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);

  dzl_binding_group_set_source (self->bindings,
                                ide_notification_stack_get_visible (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
ide_notification_stack_destroy (GtkWidget *widget)
{
  IdeNotificationStack *self = (IdeNotificationStack *)widget;

  if (self->signals != NULL)
    dzl_signal_group_set_target (self->signals, NULL);

  if (self->bindings != NULL)
    dzl_binding_group_set_source (self->bindings, NULL);

  g_clear_object (&self->bindings);
  g_clear_object (&self->signals);
  g_clear_handle_id (&self->carousel_source, g_source_remove);

  GTK_WIDGET_CLASS (ide_notification_stack_parent_class)->destroy (widget);
}

static void
ide_notification_stack_class_init (IdeNotificationStackClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = ide_notification_stack_get_property;
  object_class->set_property = ide_notification_stack_set_property;

  widget_class->destroy = ide_notification_stack_destroy;

  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress",
                         "Progress",
                         "The progress of the current item",
                         0.0, 1.0, 0.0,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, "notificationstack");
}

static void
ide_notification_stack_init (IdeNotificationStack *self)
{
  self->signals = dzl_signal_group_new (G_TYPE_LIST_MODEL);

  dzl_signal_group_connect_object (self->signals,
                                   "items-changed",
                                   G_CALLBACK (ide_notification_stack_items_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->bindings, "progress", self, "progress",
                          G_BINDING_SYNC_CREATE);

  gtk_stack_set_transition_duration (GTK_STACK (self), TRANSITION_DURATION);
  gtk_stack_set_transition_type (GTK_STACK (self), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);

  g_signal_connect (self,
                    "notify::visible-child",
                    G_CALLBACK (ide_notification_stack_notify_visible_child),
                    NULL);
}

void
ide_notification_stack_bind_model (IdeNotificationStack *self,
                                   GListModel           *model)
{
  g_return_if_fail (IDE_IS_NOTIFICATION_STACK (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));
  g_return_if_fail (!model ||
                    g_type_is_a (g_list_model_get_item_type (model), IDE_TYPE_NOTIFICATION));

  if (g_set_object (&self->model, model))
    {
      guint n_items = 0;

      if (model != NULL)
        n_items = g_list_model_get_n_items (model);

      gtk_container_foreach (GTK_CONTAINER (self), (GtkCallback)gtk_widget_destroy, NULL);
      dzl_signal_group_set_target (self->signals, model);

      if (n_items > 0)
        ide_notification_stack_items_changed_cb (self, 0, 0, n_items, model);
    }
}

gboolean
ide_notification_stack_get_can_move (IdeNotificationStack *self)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION_STACK (self), FALSE);

  if (self->model != NULL)
    return g_list_model_get_n_items (self->model) > 1;
  else
    return FALSE;
}

void
ide_notification_stack_move_next (IdeNotificationStack *self)
{
  GtkWidget *child;
  gint position;

  g_return_if_fail (IDE_IS_NOTIFICATION_STACK (self));

  if ((child = gtk_stack_get_visible_child (GTK_STACK (self))))
    {
      GList *children;

      gtk_container_child_get (GTK_CONTAINER (self), child,
                               "position", &position,
                               NULL);
      children = gtk_container_get_children (GTK_CONTAINER (self));
      if (!(child = g_list_nth_data (children, position + 1)))
        child = children->data;
      g_list_free (children);

      gtk_stack_set_transition_type (GTK_STACK (self), GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN);
      gtk_stack_set_visible_child (GTK_STACK (self), child);
      gtk_stack_set_transition_type (GTK_STACK (self), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);

      if (!self->in_carousel)
        g_clear_handle_id (&self->carousel_source, g_source_remove);
    }
}

void
ide_notification_stack_move_previous (IdeNotificationStack *self)
{
  GtkWidget *child;
  gint position;

  g_return_if_fail (IDE_IS_NOTIFICATION_STACK (self));

  if ((child = gtk_stack_get_visible_child (GTK_STACK (self))))
    {
      GList *children;

      gtk_container_child_get (GTK_CONTAINER (self), child,
                               "position", &position,
                               NULL);
      children = gtk_container_get_children (GTK_CONTAINER (self));
      if (position == 0)
        child = g_list_last (children)->data;
      else
        child = g_list_nth_data (children, position - 1);
      g_list_free (children);

      gtk_stack_set_transition_type (GTK_STACK (self), GTK_STACK_TRANSITION_TYPE_SLIDE_UP);
      gtk_stack_set_visible_child (GTK_STACK (self), child);
      gtk_stack_set_transition_type (GTK_STACK (self), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);

      if (!self->in_carousel)
        g_clear_handle_id (&self->carousel_source, g_source_remove);
    }
}

/**
 * ide_notification_stack_get_visible:
 * @self: a #IdeNotificationStack
 *
 * Gets the visible notification in the stack.
 *
 * Returns: (transfer none) (nullable): an #IdeNotification or %NULL
 *
 * Since: 3.32
 */
IdeNotification *
ide_notification_stack_get_visible (IdeNotificationStack *self)
{
  GtkWidget *child;

  g_return_val_if_fail (IDE_IS_NOTIFICATION_STACK (self), NULL);

  if ((child = gtk_stack_get_visible_child (GTK_STACK (self))))
    {
      if (IDE_IS_NOTIFICATION_VIEW (child))
        return ide_notification_view_get_notification (IDE_NOTIFICATION_VIEW (child));
    }

  return NULL;
}

gdouble
ide_notification_stack_get_progress (IdeNotificationStack *self)
{
  g_return_val_if_fail (IDE_IS_NOTIFICATION_STACK (self), 0.0);

  return self->progress;
}

void
ide_notification_stack_set_progress (IdeNotificationStack *self,
                                     gdouble               progress)
{
  g_return_if_fail (IDE_IS_NOTIFICATION_STACK (self));

  progress = CLAMP (progress, 0.0, 1.0);

  if (progress != self->progress)
    {
      self->progress = progress;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);
    }
}

gboolean
ide_notification_stack_is_empty (IdeNotificationStack *self)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_NOTIFICATION_STACK (self), FALSE);

  return self->model == NULL || g_list_model_get_n_items (self->model) == 0;
}
