/* ide-layout-view-stack.c
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

#include "ide-application.h"
#include "ide-back-forward-item.h"
#include "ide-buffer.h"
#include "ide-buffer-manager.h"
#include "ide-enums.h"
#include "ide-file.h"
#include "ide-gtk.h"
#include "ide-layout-view.h"
#include "ide-layout-grid.h"
#include "ide-layout-stack.h"
#include "ide-layout-stack-actions.h"
#include "ide-layout-stack-private.h"
#include "ide-layout-stack-split.h"
#include "ide-layout-tab-bar.h"
#include "ide-workbench.h"

G_DEFINE_TYPE (IdeLayoutStack, ide_layout_stack, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_ACTIVE_VIEW,
  LAST_PROP
};

enum {
  EMPTY,
  SPLIT,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];

void
ide_layout_stack_add (GtkContainer *container,
                      GtkWidget    *child)
{
  IdeLayoutStack *self = (IdeLayoutStack *)container;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (IDE_IS_LAYOUT_VIEW (child))
    {
      GtkStyleContext *context;

      self->focus_history = g_list_prepend (self->focus_history, child);
      gtk_container_add (GTK_CONTAINER (self->stack), child);
      ide_layout_view_set_back_forward_list (IDE_LAYOUT_VIEW (child), self->back_forward_list);
      gtk_stack_set_visible_child (self->stack, child);

      context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_remove_class (context, "empty");
    }
  else
    {
      GTK_CONTAINER_CLASS (ide_layout_stack_parent_class)->add (container, child);
    }
}

void
ide_layout_stack_remove (IdeLayoutStack *self,
                         GtkWidget      *view)
{
  GtkWidget *focus_after_close = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));

  focus_after_close = g_list_nth_data (self->focus_history, 1);
  if (focus_after_close != NULL)
    g_object_ref (focus_after_close);

  self->focus_history = g_list_remove (self->focus_history, view);
  gtk_container_remove (GTK_CONTAINER (self->stack), view);

  if (focus_after_close != NULL)
    {
      gtk_stack_set_visible_child (self->stack, focus_after_close);
      gtk_widget_grab_focus (GTK_WIDGET (focus_after_close));
      g_clear_object (&focus_after_close);
    }
  else
    {
      GtkStyleContext *context;

      context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_class (context, "empty");

      g_signal_emit (self, signals [EMPTY], 0);
    }
}

static void
ide_layout_stack_real_remove (GtkContainer *container,
                              GtkWidget    *child)
{
  IdeLayoutStack *self = (IdeLayoutStack *)container;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (IDE_IS_LAYOUT_VIEW (child))
    ide_layout_stack_remove (self, child);
  else
    GTK_CONTAINER_CLASS (ide_layout_stack_parent_class)->remove (container, child);
}

static void
ide_layout_stack__notify_visible_child (IdeLayoutStack *self,
                                        GParamSpec     *pspec,
                                        GtkStack       *stack)
{
  GtkWidget *visible_child;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_STACK (stack));

  visible_child = gtk_stack_get_visible_child (stack);

  ide_layout_stack_set_active_view (self, visible_child);
}

static void
ide_layout_stack_grab_focus (GtkWidget *widget)
{
  IdeLayoutStack *self = (IdeLayoutStack *)widget;
  GtkWidget *visible_child;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  visible_child = gtk_stack_get_visible_child (self->stack);
  if (visible_child)
    gtk_widget_grab_focus (visible_child);
}

static void
navigate_to_cb (IdeLayoutStack     *self,
                IdeBackForwardItem *item,
                IdeBackForwardList *back_forward_list)
{
  IdeWorkbench *workbench;
  IdeUri *uri;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_BACK_FORWARD_ITEM (item));
  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  uri = ide_back_forward_item_get_uri (item);
  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  ide_workbench_open_uri_async (workbench, uri, NULL, NULL, NULL, NULL);
}

static void
ide_layout_stack_context_handler (GtkWidget  *widget,
                                  IdeContext *context)
{
  IdeBackForwardList *back_forward;
  IdeLayoutStack *self = (IdeLayoutStack *)widget;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (!context || IDE_IS_CONTEXT (context));

  if (context)
    {
      GAction *action;
      GList *children;
      GList *iter;

      ide_set_weak_pointer (&self->context, context);

      back_forward = ide_context_get_back_forward_list (context);

      g_clear_object (&self->back_forward_list);
      self->back_forward_list = ide_back_forward_list_branch (back_forward);

      g_signal_connect_object (self->back_forward_list,
                               "navigate-to",
                               G_CALLBACK (navigate_to_cb),
                               self,
                               G_CONNECT_SWAPPED);

      action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "go-backward");
      g_object_bind_property (self->back_forward_list, "can-go-backward",
                              action, "enabled", G_BINDING_SYNC_CREATE);

      action = g_action_map_lookup_action (G_ACTION_MAP (self->actions), "go-forward");
      g_object_bind_property (self->back_forward_list, "can-go-forward",
                              action, "enabled", G_BINDING_SYNC_CREATE);

      children = gtk_container_get_children (GTK_CONTAINER (self->stack));
      for (iter = children; iter; iter = iter->next)
        ide_layout_view_set_back_forward_list (iter->data, self->back_forward_list);
      g_list_free (children);
    }
}

static void
ide_layout_stack__workbench__unload (IdeWorkbench   *workbench,
                                     IdeContext     *context,
                                     IdeLayoutStack *self)
{
  IdeBackForwardList *back_forward_list;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (self->back_forward_list)
    {
      back_forward_list = ide_context_get_back_forward_list (context);
      ide_back_forward_list_merge (back_forward_list, self->back_forward_list);
    }
}

static void
ide_layout_stack_hierarchy_changed (GtkWidget *widget,
                                    GtkWidget *old_toplevel)
{
  IdeLayoutStack *self = (IdeLayoutStack *)widget;
  GtkWidget *toplevel;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (IDE_IS_WORKBENCH (old_toplevel))
    {
      g_signal_handlers_disconnect_by_func (old_toplevel,
                                            G_CALLBACK (ide_layout_stack__workbench__unload),
                                            self);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (IDE_IS_WORKBENCH (toplevel))
    {
      g_signal_connect (toplevel,
                        "unload",
                        G_CALLBACK (ide_layout_stack__workbench__unload),
                        self);
    }
}

static void
ide_layout_stack_swipe (IdeLayoutStack  *self,
                        gdouble          velocity_x,
                        gdouble          velocity_y,
                        GtkGestureSwipe *gesture)
{
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_GESTURE_SWIPE (gesture));

  if (ABS (velocity_x) > ABS (velocity_y))
    {
      if (velocity_x < 0)
        ide_widget_action (GTK_WIDGET (self), "view-stack", "previous-view", NULL);
      else if (velocity_x > 0)
        ide_widget_action (GTK_WIDGET (self), "view-stack", "next-view", NULL);
    }
}

static gboolean
ide_layout_stack__tab_bar__button_press (IdeLayoutStack  *self,
                                         GdkEventButton  *button,
                                         IdeLayoutTabBar *tab_bar)
{
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (button != NULL);
  g_assert (GTK_IS_EVENT_BOX (tab_bar));

  if (button->button == GDK_BUTTON_PRIMARY)
    {
      gtk_widget_grab_focus (GTK_WIDGET (self));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_layout_stack_destroy (GtkWidget *widget)
{
  IdeLayoutStack *self = (IdeLayoutStack *)widget;

  self->destroyed = TRUE;

  GTK_WIDGET_CLASS (ide_layout_stack_parent_class)->destroy (widget);
}

static void
ide_layout_stack_constructed (GObject *object)
{
  IdeLayoutStack *self = (IdeLayoutStack *)object;

  G_OBJECT_CLASS (ide_layout_stack_parent_class)->constructed (object);

  g_signal_connect_object (self->tab_bar,
                           "button-press-event",
                           G_CALLBACK (ide_layout_stack__tab_bar__button_press),
                           self,
                           G_CONNECT_SWAPPED);

  _ide_layout_stack_actions_init (self);
}

static void
ide_layout_stack_finalize (GObject *object)
{
  IdeLayoutStack *self = (IdeLayoutStack *)object;

  g_clear_pointer (&self->focus_history, g_list_free);
  ide_clear_weak_pointer (&self->context);
  ide_clear_weak_pointer (&self->active_view);
  g_clear_object (&self->back_forward_list);
  g_clear_object (&self->swipe_gesture);
  g_clear_object (&self->actions);

  G_OBJECT_CLASS (ide_layout_stack_parent_class)->finalize (object);
}

static void
ide_layout_stack_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeLayoutStack *self = IDE_LAYOUT_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      g_value_set_object (value, ide_layout_stack_get_active_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_stack_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeLayoutStack *self = IDE_LAYOUT_STACK (object);

  switch (prop_id)
    {
    case PROP_ACTIVE_VIEW:
      ide_layout_stack_set_active_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_layout_stack_class_init (IdeLayoutStackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->constructed = ide_layout_stack_constructed;
  object_class->finalize = ide_layout_stack_finalize;
  object_class->get_property = ide_layout_stack_get_property;
  object_class->set_property = ide_layout_stack_set_property;

  widget_class->destroy = ide_layout_stack_destroy;
  widget_class->grab_focus = ide_layout_stack_grab_focus;
  widget_class->hierarchy_changed = ide_layout_stack_hierarchy_changed;

  container_class->add = ide_layout_stack_add;
  container_class->remove = ide_layout_stack_real_remove;

  properties [PROP_ACTIVE_VIEW] =
    g_param_spec_object ("active-view",
                         "Active View",
                         "The active view.",
                         IDE_TYPE_LAYOUT_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [EMPTY] =
    g_signal_new_class_handler ("empty",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  /**
   * IdeLayoutStack::split:
   * @self: A #IdeLayoutStack.
   * @view: The #IdeLayoutView to split.
   * @split_type: (type gint): A #IdeLayoutGridSplit.
   *
   * Requests a split to be performed on the view.
   *
   * This should only be used by #IdeLayoutGrid.
   */
  signals [SPLIT] = g_signal_new ("split",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL, NULL,
                                   G_TYPE_NONE,
                                   2,
                                   IDE_TYPE_LAYOUT_VIEW,
                                   IDE_TYPE_LAYOUT_GRID_SPLIT);

  gtk_widget_class_set_css_name (widget_class, "layoutstack");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-stack.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, tab_bar);

  g_type_ensure (IDE_TYPE_LAYOUT_TAB_BAR);
}

static void
ide_layout_stack_init (IdeLayoutStack *self)
{
  GtkStyleContext *context;
  GList *focus_chain = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (context, "empty");

  g_signal_connect_object (self->stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_layout_stack__notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  self->swipe_gesture = gtk_gesture_swipe_new (GTK_WIDGET (self));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->swipe_gesture), TRUE);
  g_signal_connect_object (self->swipe_gesture,
                           "swipe",
                           G_CALLBACK (ide_layout_stack_swipe),
                           self,
                           G_CONNECT_SWAPPED);

  ide_widget_set_context_handler (self, ide_layout_stack_context_handler);

  focus_chain = g_list_prepend (focus_chain, self->tab_bar);
  focus_chain = g_list_prepend (focus_chain, self->stack);
  gtk_container_set_focus_chain (GTK_CONTAINER (self), focus_chain);
  g_list_free (focus_chain);
}

GtkWidget *
ide_layout_stack_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT_STACK, NULL);
}

/**
 * ide_layout_stack_get_active_view:
 *
 * Returns: (transfer none) (nullable): A #GtkWidget or %NULL.
 */
GtkWidget *
ide_layout_stack_get_active_view (IdeLayoutStack *self)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), NULL);

  return self->active_view;
}

void
ide_layout_stack_set_active_view (IdeLayoutStack *self,
                                  GtkWidget      *active_view)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (!active_view || IDE_IS_LAYOUT_VIEW (active_view));

  if (self->destroyed)
    return;

  if (self->active_view != active_view)
    {
      gtk_widget_insert_action_group (GTK_WIDGET (self), "view", NULL);

      ide_set_weak_pointer (&self->active_view, active_view);

      if (active_view != NULL)
        {
          GActionGroup *group;

          if (active_view != gtk_stack_get_visible_child (self->stack))
            gtk_stack_set_visible_child (self->stack, active_view);

          self->focus_history = g_list_remove (self->focus_history, active_view);
          self->focus_history = g_list_prepend (self->focus_history, active_view);

          group = gtk_widget_get_action_group (active_view, "view");
          if (group)
            gtk_widget_insert_action_group (GTK_WIDGET (self), "view", group);
        }

      ide_layout_tab_bar_set_view (self->tab_bar, active_view);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTIVE_VIEW]);
    }
}

/**
 * ide_layout_stack_foreach_view:
 * @callback: (scope call): A callback to invoke for each view.
 */
void
ide_layout_stack_foreach_view (IdeLayoutStack *self,
                               GtkCallback     callback,
                               gpointer        user_data)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (self->stack), callback, user_data);
}
