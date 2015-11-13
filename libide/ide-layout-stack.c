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

static void
ide_layout_stack_add_list_row (IdeLayoutStack *self,
                               IdeLayoutView  *child)
{
  GtkWidget *row;
  GtkWidget *label;
  GtkWidget *box;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (child));

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "visible", TRUE,
                      NULL);
  g_object_set_data (G_OBJECT (row), "IDE_LAYOUT_VIEW", child);

  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = g_object_new (GTK_TYPE_LABEL,
                        "margin-bottom", 3,
                        "margin-end", 6,
                        "margin-start", 6,
                        "margin-top", 3,
                        "visible", TRUE,
                        "xalign", 0.0f,
                        NULL);
  g_object_bind_property (child, "title", label, "label", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = g_object_new (GTK_TYPE_LABEL,
                        "visible", FALSE,
                        "label", "•",
                        "margin-start", 3,
                        "margin-end", 3,
                        NULL);
  g_object_bind_property (child, "modified", label, "visible", G_BINDING_SYNC_CREATE);
  gtk_container_add (GTK_CONTAINER (box), label);

  gtk_container_add (GTK_CONTAINER (self->views_listbox), row);
}

static void
ide_layout_stack_remove_list_row (IdeLayoutStack *self,
                                  IdeLayoutView  *child)
{
  GList *children;
  GList *iter;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (child));

  children = gtk_container_get_children (GTK_CONTAINER (self->views_listbox));

  for (iter = children; iter; iter = iter->next)
    {
      IdeLayoutView *view = g_object_get_data (iter->data, "IDE_LAYOUT_VIEW");

      if (view == child)
        {
          gtk_container_remove (GTK_CONTAINER (self->views_listbox), iter->data);
          break;
        }
    }

  g_list_free (children);
}

static void
ide_layout_stack_move_top_list_row (IdeLayoutStack *self,
                                    IdeLayoutView  *view)
{
  GList *children;
  GList *iter;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  children = gtk_container_get_children (GTK_CONTAINER (self->views_listbox));

  for (iter = children; iter; iter = iter->next)
    {
      GtkWidget *row = iter->data;
      IdeLayoutView *item = g_object_get_data (G_OBJECT (row), "IDE_LAYOUT_VIEW");

      if (item == view)
        {
          g_object_ref (row);
          gtk_container_remove (GTK_CONTAINER (self->views_listbox), row);
          gtk_list_box_prepend (self->views_listbox, row);
          gtk_list_box_select_row (self->views_listbox, GTK_LIST_BOX_ROW (row));
          g_object_unref (row);
          break;
        }
    }

  g_list_free (children);
}

void
ide_layout_stack_add (GtkContainer *container,
                      GtkWidget    *child)
{
  IdeLayoutStack *self = (IdeLayoutStack *)container;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (IDE_IS_LAYOUT_VIEW (child))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), TRUE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->views_button), TRUE);

      self->focus_history = g_list_prepend (self->focus_history, child);
      gtk_container_add (GTK_CONTAINER (self->stack), child);
      ide_layout_view_set_back_forward_list (IDE_LAYOUT_VIEW (child), self->back_forward_list);
      ide_layout_stack_add_list_row (self, IDE_LAYOUT_VIEW (child));
      gtk_stack_set_visible_child (self->stack, child);
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
  GtkWidget *controls;
  GtkWidget *focus_after_close = NULL;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));

  focus_after_close = g_list_nth_data (self->focus_history, 1);
  if (focus_after_close != NULL)
    g_object_ref (focus_after_close);

  ide_layout_stack_remove_list_row (self, IDE_LAYOUT_VIEW (view));

  self->focus_history = g_list_remove (self->focus_history, view);
  controls = ide_layout_view_get_controls (IDE_LAYOUT_VIEW (view));
  if (controls)
    gtk_container_remove (GTK_CONTAINER (self->controls), controls);
  gtk_container_remove (GTK_CONTAINER (self->stack), view);

  if (focus_after_close != NULL)
    {
      gtk_stack_set_visible_child (self->stack, focus_after_close);
      gtk_widget_grab_focus (GTK_WIDGET (focus_after_close));
      g_clear_object (&focus_after_close);
    }
  else
    g_signal_emit (self, signals [EMPTY], 0);
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

static gboolean
ide_layout_stack_is_empty (IdeLayoutStack *self)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), FALSE);

  return (self->focus_history == NULL);
}

static void
ide_layout_stack_real_empty (IdeLayoutStack *self)
{
  g_assert (IDE_IS_LAYOUT_STACK (self));

  /* its possible for a widget to be added during "empty" emission. */
  if (ide_layout_stack_is_empty (self) && !self->destroyed)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (self->modified_label), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (self->views_button), FALSE);
    }
}

#if 0
static void
navigate_to_cb (IdeLayoutStack     *self,
                IdeBackForwardItem *item,
                IdeBackForwardList *back_forward_list)
{
  IdeSourceLocation *srcloc;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_BACK_FORWARD_ITEM (item));
  g_assert (IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  srcloc = ide_back_forward_item_get_location (item);
  ide_layout_stack_focus_location (self, srcloc);
}
#endif

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
      GList *children;
      GList *iter;

      ide_set_weak_pointer (&self->context, context);

      back_forward = ide_context_get_back_forward_list (context);

      g_clear_object (&self->back_forward_list);
      self->back_forward_list = ide_back_forward_list_branch (back_forward);

#if 0
      /*
       * TODO: We need to make BackForwardItem use IdeUri.
       */
      g_signal_connect_object (self->back_forward_list,
                               "navigate-to",
                               G_CALLBACK (navigate_to_cb),
                               self,
                               G_CONNECT_SWAPPED);
#endif

      g_object_bind_property (self->back_forward_list, "can-go-backward",
                              self->go_backward, "sensitive",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (self->back_forward_list, "can-go-forward",
                              self->go_forward, "sensitive",
                              G_BINDING_SYNC_CREATE);

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
ide_layout_stack__views_listbox__row_activated_cb (IdeLayoutStack *self,
                                                   GtkListBoxRow  *row,
                                                   GtkListBox     *list_box)
{
  IdeLayoutView *view;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  view = g_object_get_data (G_OBJECT (row), "IDE_LAYOUT_VIEW");

  if (IDE_IS_LAYOUT_VIEW (view))
    {
      gtk_widget_hide (GTK_WIDGET (self->views_popover));
      ide_layout_stack_set_active_view (self, GTK_WIDGET (view));
      gtk_widget_grab_focus (GTK_WIDGET (view));
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
ide_layout_stack__header__button_press (IdeLayoutStack *self,
                                        GdkEventButton *button,
                                        GtkEventBox    *event_box)
{
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (button != NULL);
  g_assert (GTK_IS_EVENT_BOX (event_box));

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

  g_signal_connect_object (self->views_listbox,
                           "row-activated",
                           G_CALLBACK (ide_layout_stack__views_listbox__row_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->header_event_box,
                           "button-press-event",
                           G_CALLBACK (ide_layout_stack__header__button_press),
                           self,
                           G_CONNECT_SWAPPED);

  _ide_layout_stack_actions_init (self);

  /*
   * FIXME:
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=747060
   *
   * Setting sensitive in the template is getting changed out from under us.
   * Likely due to the popover item being set (conflation of having a popover
   * vs wanting sensitivity). So we will just override it here.
   *
   * Last tested Gtk+ was 3.17.
   */
  gtk_widget_set_sensitive (GTK_WIDGET (self->close_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->views_button), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (self->document_button), FALSE);
}

static void
ide_layout_stack_finalize (GObject *object)
{
  IdeLayoutStack *self = (IdeLayoutStack *)object;

  g_clear_pointer (&self->focus_history, g_list_free);
  ide_clear_weak_pointer (&self->context);
  ide_clear_weak_pointer (&self->title_binding);
  ide_clear_weak_pointer (&self->active_view);
  g_clear_object (&self->back_forward_list);
  g_clear_object (&self->swipe_gesture);

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
                                G_CALLBACK (ide_layout_stack_real_empty),
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
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, close_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, controls);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, document_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, go_backward);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, go_forward);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, header_event_box);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, modified_label);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, stack);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, title_label);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, views_button);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, views_listbox);
  gtk_widget_class_bind_template_child (widget_class, IdeLayoutStack, views_popover);
}

static void
ide_layout_stack_init (IdeLayoutStack *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

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
      if (self->active_view)
        {
          if (self->title_binding)
            g_binding_unbind (self->title_binding);
          ide_clear_weak_pointer (&self->title_binding);
          if (self->modified_binding)
            g_binding_unbind (self->modified_binding);
          ide_clear_weak_pointer (&self->modified_binding);
          gtk_label_set_label (self->title_label, NULL);
          ide_clear_weak_pointer (&self->active_view);
          gtk_widget_hide (GTK_WIDGET (self->controls));
        }

      if (active_view)
        {
          GtkWidget *controls;
          GBinding *binding;
          GActionGroup *group;
          GMenu *menu;
          GtkPopover *popover;

          ide_set_weak_pointer (&self->active_view, active_view);
          if (active_view != gtk_stack_get_visible_child (self->stack))
            gtk_stack_set_visible_child (self->stack, active_view);

          menu = ide_layout_view_get_menu (IDE_LAYOUT_VIEW (active_view));
          popover = g_object_new (GTK_TYPE_POPOVER, NULL);
          gtk_popover_bind_model (popover, G_MENU_MODEL (menu), NULL);
          gtk_menu_button_set_popover (self->document_button, GTK_WIDGET (popover));

          self->focus_history = g_list_remove (self->focus_history, active_view);
          self->focus_history = g_list_prepend (self->focus_history, active_view);

          binding = g_object_bind_property (active_view, "special-title",
                                            self->title_label, "label",
                                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->title_binding, binding);

          binding = g_object_bind_property (active_view, "modified",
                                            self->modified_label, "visible",
                                            G_BINDING_SYNC_CREATE);
          ide_set_weak_pointer (&self->modified_binding, binding);

          controls = ide_layout_view_get_controls (IDE_LAYOUT_VIEW (active_view));

          if (controls != NULL)
            {
              GList *children;
              GList *iter;

              children = gtk_container_get_children (GTK_CONTAINER (self->controls));
              for (iter = children; iter; iter = iter->next)
                gtk_container_remove (GTK_CONTAINER (self->controls), iter->data);
              g_list_free (children);

              gtk_container_add (GTK_CONTAINER (self->controls), controls);
              gtk_widget_show (GTK_WIDGET (self->controls));
            }
          else
            {
              gtk_widget_hide (GTK_WIDGET (self->controls));
            }

          group = gtk_widget_get_action_group (active_view, "view");
          if (group)
            gtk_widget_insert_action_group (GTK_WIDGET (self), "view", group);

          ide_layout_stack_move_top_list_row (self, IDE_LAYOUT_VIEW (active_view));
        }

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
