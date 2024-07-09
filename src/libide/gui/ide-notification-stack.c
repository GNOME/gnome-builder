/* ide-notification-stack.c
 *
 * Copyright 2018-2022 Christian Hergert <chergert@redhat.com>
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

#include "ide-notification-stack-private.h"
#include "ide-notification-view-private.h"

#define CAROUSEL_TIMEOUT_SECS 5
#define TRANSITION_DURATION   500

struct _IdeNotificationStack
{
  GtkWidget        parent_instance;
  GtkStack        *stack;
  GPtrArray       *pages;
  GSignalGroup    *signals;
  GBindingGroup   *bindings;
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

G_DEFINE_FINAL_TYPE (IdeNotificationStack, ide_notification_stack, GTK_TYPE_WIDGET)

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
  g_autoptr(GtkSelectionModel) pages = NULL;
  GtkWidget *urgent = NULL;

  g_assert (IDE_IS_NOTIFICATION_STACK (self));

  if (self->pages == NULL)
    return;

  for (guint i = 0; i < removed; i++)
    {
      GtkStackPage *page = g_ptr_array_index (self->pages, position);
      g_ptr_array_remove_index (self->pages, position);
      gtk_stack_remove (self->stack, gtk_stack_page_get_child (page));
    }

  for (guint i = 0; i < added; i++)
    {
      g_autoptr(IdeNotification) notif = g_list_model_get_item (model, position + i);
      GtkWidget *view = g_object_new (IDE_TYPE_NOTIFICATION_VIEW,
                                      "notification", notif,
                                      "visible", TRUE,
                                      NULL);
      GtkStackPage *page = gtk_stack_add_child (self->stack, view);

      g_ptr_array_insert (self->pages, position + i, page);

      if (!urgent && ide_notification_get_urgent (notif))
        urgent = view;
    }

  if (urgent != NULL)
    {
      gtk_stack_set_visible_child (self->stack, urgent);
      g_clear_handle_id (&self->carousel_source, g_source_remove);
    }

  if (gtk_stack_get_visible_child (self->stack) == NULL &&
      (pages = gtk_stack_get_pages (self->stack)) &&
      g_list_model_get_n_items (G_LIST_MODEL (pages)) > 0)
    {
      g_autoptr(GtkStackPage) page = g_list_model_get_item (G_LIST_MODEL (pages), 0);

      gtk_stack_set_visible_child (self->stack,
                                   gtk_stack_page_get_child (page));

    }

  if (self->carousel_source == 0 && g_list_model_get_n_items (model))
    self->carousel_source = g_timeout_add_seconds (CAROUSEL_TIMEOUT_SECS,
                                                   ide_notification_stack_carousel_cb,
                                                   self);

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
ide_notification_stack_notify_visible_child (IdeNotificationStack *self,
                                             GParamSpec           *pspec,
                                             GtkStack             *stack)
{
  g_assert (IDE_IS_NOTIFICATION_STACK (self));
  g_assert (GTK_IS_STACK (stack));

  self->progress = 0.0;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROGRESS]);

  g_binding_group_set_source (self->bindings,
                                ide_notification_stack_get_visible (self));

  g_signal_emit (self, signals [CHANGED], 0);
}

static void
ide_notification_stack_dispose (GObject *object)
{
  IdeNotificationStack *self = (IdeNotificationStack *)object;

  g_clear_pointer (&self->pages, g_ptr_array_unref);

  if (self->signals != NULL)
    {
      g_signal_group_set_target (self->signals, NULL);
      g_clear_object (&self->signals);
    }

  if (self->bindings != NULL)
    {
      g_binding_group_set_source (self->bindings, NULL);
      g_clear_object (&self->bindings);
    }

  g_clear_handle_id (&self->carousel_source, g_source_remove);

  g_clear_pointer ((GtkWidget **)&self->stack, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_notification_stack_parent_class)->dispose (object);
}

static void
ide_notification_stack_class_init (IdeNotificationStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_notification_stack_dispose;
  object_class->get_property = ide_notification_stack_get_property;
  object_class->set_property = ide_notification_stack_set_property;

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
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
ide_notification_stack_init (IdeNotificationStack *self)
{
  self->pages = g_ptr_array_new ();

  self->signals = g_signal_group_new (G_TYPE_LIST_MODEL);
  g_signal_group_connect_object (self->signals,
                                   "items-changed",
                                   G_CALLBACK (ide_notification_stack_items_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->bindings = g_binding_group_new ();
  g_binding_group_bind (self->bindings, "progress",
                          self, "progress",
                          G_BINDING_SYNC_CREATE);

  self->stack = g_object_new (GTK_TYPE_STACK,
                              "transition-duration", TRANSITION_DURATION,
                              "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN,
                              NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->stack), GTK_WIDGET (self));

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_notification_stack_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);
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

      while (self->pages->len > 0)
        {
          GtkStackPage *page = g_ptr_array_index (self->pages, 0);
          g_ptr_array_remove_index (self->pages, 0);
          gtk_stack_remove (self->stack, gtk_stack_page_get_child (page));
        }

      g_signal_group_set_target (self->signals, model);

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

static void
ide_notification_stack_move (IdeNotificationStack *self,
                             int                   direction)
{
  GtkStackPage *page;
  GtkWidget *child;
  int position = -1;

  g_return_if_fail (IDE_IS_NOTIFICATION_STACK (self));
  g_return_if_fail (direction == -1 || direction == 1);

  if (!self->in_carousel)
    g_clear_handle_id (&self->carousel_source, g_source_remove);

  if (self->pages->len == 0)
    return;

  child = gtk_stack_get_visible_child (self->stack);

  for (guint i = 0; i < self->pages->len; i++)
    {
      page = g_ptr_array_index (self->pages, i);

      if (child == gtk_stack_page_get_child (page))
        {
          position = i;
          break;
        }
    }

  if (position == -1)
    return;

  g_assert (position >= 0);
  g_assert (self->pages->len > 0);

  if (direction == -1 && position == 0)
    position = self->pages->len - 1;
  else if (direction == 1 && position == self->pages->len - 1)
    position = 0;
  else
    position += direction;

  g_assert (position >= 0);
  g_assert (position < self->pages->len);

  page = g_ptr_array_index (self->pages, position);

  g_assert (page != NULL);
  g_assert (GTK_IS_STACK_PAGE (page));

  child = gtk_stack_page_get_child (page);

  g_assert (child != NULL);
  g_assert (GTK_IS_WIDGET (child));

  gtk_stack_set_visible_child (self->stack, child);
}

void
ide_notification_stack_move_next (IdeNotificationStack *self)
{
  ide_notification_stack_move (self, 1);
}

void
ide_notification_stack_move_previous (IdeNotificationStack *self)
{
  ide_notification_stack_move (self, -1);
}

/**
 * ide_notification_stack_get_visible:
 * @self: a #IdeNotificationStack
 *
 * Gets the visible notification in the stack.
 *
 * Returns: (transfer none) (nullable): an #IdeNotification or %NULL
 */
IdeNotification *
ide_notification_stack_get_visible (IdeNotificationStack *self)
{
  GtkWidget *child;

  g_return_val_if_fail (IDE_IS_NOTIFICATION_STACK (self), NULL);

  if ((child = gtk_stack_get_visible_child (self->stack)))
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
