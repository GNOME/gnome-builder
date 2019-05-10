/* ide-omni-bar.c
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

#define G_LOG_DOMAIN "ide-omni-bar"

#include "config.h"

#include <libpeas/peas.h>
#include <dazzle.h>

#include "ide-gui-global.h"
#include "ide-gui-private.h"
#include "ide-notification-list-box-row-private.h"
#include "ide-notification-stack-private.h"
#include "ide-omni-bar-addin.h"
#include "ide-omni-bar.h"

struct _IdeOmniBar
{
  GtkEventBox           parent_instance;

  PeasExtensionSet     *addins;
  GtkGesture           *gesture;
  GtkEventController   *motion;

  GtkStack             *top_stack;
  GtkPopover           *popover;
  DzlEntryBox          *entry_box;
  IdeNotificationStack *notification_stack;
  GtkListBox           *notifications_list_box;
  DzlPriorityBox       *inner_box;
  DzlPriorityBox       *outer_box;
  GtkProgressBar       *progress;
  GtkWidget            *placeholder;
  DzlPriorityBox       *sections_box;

  guint                 in_button : 1;
};

static void ide_omni_bar_move_next     (IdeOmniBar        *self,
                                        GVariant          *param);
static void ide_omni_bar_move_previous (IdeOmniBar        *self,
                                        GVariant          *param);
static void buildable_iface_init       (GtkBuildableIface *iface);

DZL_DEFINE_ACTION_GROUP (IdeOmniBar, ide_omni_bar, {
  { "move-next", ide_omni_bar_move_next },
  { "move-previous", ide_omni_bar_move_previous },
})

G_DEFINE_TYPE_WITH_CODE (IdeOmniBar, ide_omni_bar, GTK_TYPE_EVENT_BOX,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, ide_omni_bar_init_action_group)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
ide_omni_bar_popover_closed_cb (IdeOmniBar *self,
                                GtkPopover *popover)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_POPOVER (popover));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);

  state_flags &= ~GTK_STATE_FLAG_ACTIVE;
  state_flags &= ~GTK_STATE_FLAG_PRELIGHT;

  gtk_style_context_set_state (style_context, state_flags);
}

static void
multipress_pressed_cb (IdeOmniBar           *self,
                       guint                 n_press,
                       gdouble               x,
                       gdouble               y,
                       GtkGestureMultiPress *gesture)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  if (gtk_widget_get_focus_on_click (GTK_WIDGET (self)) &&
      !gtk_widget_has_focus (GTK_WIDGET (self)))
    gtk_widget_grab_focus (GTK_WIDGET (self));

  self->in_button = TRUE;

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);
  gtk_style_context_set_state (style_context, state_flags | GTK_STATE_FLAG_ACTIVE);

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
multipress_released_cb (IdeOmniBar           *self,
                        guint                 n_press,
                        gdouble               x,
                        gdouble               y,
                        GtkGestureMultiPress *gesture)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;
  gboolean show;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  show = self->in_button;
  self->in_button = FALSE;

  if (show)
    {
      gtk_popover_popup (self->popover);
      return;
    }

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);
  gtk_style_context_set_state (style_context, state_flags & ~GTK_STATE_FLAG_ACTIVE);
}

static void
multipress_cancel_cb (IdeOmniBar           *self,
                      GdkEventSequence     *sequence,
                      GtkGestureMultiPress *gesture)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));

  self->in_button = FALSE;

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);
  gtk_style_context_set_state (style_context, state_flags & ~GTK_STATE_FLAG_ACTIVE);
}

static void
ide_omni_bar_notification_stack_changed_cb (IdeOmniBar           *self,
                                            IdeNotificationStack *stack)
{
  IdeNotification *notif;
  gboolean enabled;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_NOTIFICATION_STACK (stack));

  enabled = ide_notification_stack_get_can_move (stack);

  ide_omni_bar_set_action_enabled (self, "move-previous", enabled);
  ide_omni_bar_set_action_enabled (self, "move-next", enabled);

  _ide_gtk_progress_bar_stop_pulsing (self->progress);
  gtk_widget_hide (GTK_WIDGET (self->progress));

  if ((notif = ide_notification_stack_get_visible (stack)))
    {
      if (ide_notification_get_has_progress (notif))
        {
          if (ide_notification_get_progress_is_imprecise (notif))
            _ide_gtk_progress_bar_start_pulsing (self->progress);
          gtk_widget_show (GTK_WIDGET (self->progress));
        }
    }

  if (ide_notification_stack_is_empty (stack))
    gtk_stack_set_visible_child_name (self->top_stack, "placeholder");
  else
    gtk_stack_set_visible_child_name (self->top_stack, "notifications");
}

static void
ide_omni_bar_extension_added_cb (PeasExtensionSet *set,
                                 PeasPluginInfo   *plugin_info,
                                 PeasExtension    *exten,
                                 gpointer          user_data)
{
  IdeOmniBarAddin *addin = (IdeOmniBarAddin *)exten;
  IdeOmniBar *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_OMNI_BAR_ADDIN (addin));
  g_assert (IDE_IS_OMNI_BAR (self));

  ide_omni_bar_addin_load (addin, self);
}

static void
ide_omni_bar_extension_removed_cb (PeasExtensionSet *set,
                                   PeasPluginInfo   *plugin_info,
                                   PeasExtension    *exten,
                                   gpointer          user_data)
{
  IdeOmniBarAddin *addin = (IdeOmniBarAddin *)exten;
  IdeOmniBar *self = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_OMNI_BAR_ADDIN (addin));
  g_assert (IDE_IS_OMNI_BAR (self));

  ide_omni_bar_addin_unload (addin, self);
}

static GtkWidget *
create_notification_row (gpointer item,
                         gpointer user_data)
{
  IdeNotification *notif = item;
  gboolean has_default;

  g_assert (IDE_IS_NOTIFICATION (notif));

  has_default = ide_notification_get_default_action (notif, NULL, NULL);

  return g_object_new (IDE_TYPE_NOTIFICATION_LIST_BOX_ROW,
                       "activatable", has_default,
                       "notification", notif,
                       "visible", TRUE,
                       NULL);
}

static gboolean
filter_for_popover (GObject  *object,
                    gpointer  user_data)
{
  IdeNotification *notif = (IdeNotification *)object;

  g_assert (IDE_IS_NOTIFICATION (notif));
  g_assert (user_data == NULL);

  return !ide_notification_get_has_progress (notif) &&
         ide_notification_get_urgent (notif);
}

static void
ide_omni_bar_context_set_cb (GtkWidget  *widget,
                             IdeContext *context)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;
  g_autoptr(IdeObject) notifications = NULL;
  g_autoptr(DzlListModelFilter) filter = NULL;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_CONTEXT (context));
  g_assert (self->addins == NULL);

  notifications = ide_object_get_child_typed (IDE_OBJECT (context), IDE_TYPE_NOTIFICATIONS);
  ide_notification_stack_bind_model (self->notification_stack, G_LIST_MODEL (notifications));

  filter = dzl_list_model_filter_new (G_LIST_MODEL (notifications));
  dzl_list_model_filter_set_filter_func (filter, filter_for_popover, NULL, NULL);
  gtk_list_box_bind_model (self->notifications_list_box,
                           G_LIST_MODEL (filter),
                           create_notification_row,
                           NULL, NULL);

  self->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_OMNI_BAR_ADDIN,
                                         NULL);

  g_signal_connect (self->addins,
                    "extension-added",
                    G_CALLBACK (ide_omni_bar_extension_added_cb),
                    self);
  g_signal_connect (self->addins,
                    "extension-removed",
                    G_CALLBACK (ide_omni_bar_extension_removed_cb),
                    self);

  peas_extension_set_foreach (self->addins,
                              ide_omni_bar_extension_added_cb,
                              self);
}

static void
ide_omni_bar_motion_enter_cb (IdeOmniBar               *self,
                              gdouble                   x,
                              gdouble                   y,
                              GtkEventControllerMotion *motion)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);

  if ((state_flags & GTK_STATE_FLAG_PRELIGHT) == 0)
    gtk_style_context_set_state (style_context, state_flags | GTK_STATE_FLAG_PRELIGHT);
}

static void
ide_omni_bar_motion_leave_cb (IdeOmniBar               *self,
                              GtkEventControllerMotion *motion)
{
  GtkStyleContext *style_context;
  GtkStateFlags state_flags;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  state_flags = gtk_style_context_get_state (style_context);

  if (state_flags & GTK_STATE_FLAG_PRELIGHT)
    gtk_style_context_set_state (style_context, state_flags & ~GTK_STATE_FLAG_PRELIGHT);
}

static void
ide_omni_bar_motion_cb (IdeOmniBar               *self,
                        gdouble                   x,
                        gdouble                   y,
                        GtkEventControllerMotion *motion)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  /*
   * Because of how crossing-events work with Gtk 3, we don't get reliable
   * crossing events for the motion controller. So every motion (which we do
   * seem to get semi-reliably), just re-run the enter-notify path to ensure
   * we get proper state set.
   */

  ide_omni_bar_motion_enter_cb (self, x, y, motion);
}

static gboolean
ide_omni_bar_query_tooltip (GtkWidget  *widget,
                            gint        x,
                            gint        y,
                            gboolean    keyboard_mode,
                            GtkTooltip *tooltip)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;
  IdeNotification *notif;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_TOOLTIP (tooltip));

  if ((notif = ide_notification_stack_get_visible (self->notification_stack)))
    {
      g_autofree gchar *body = ide_notification_dup_body (notif);

      if (body != NULL)
        {
          gtk_tooltip_set_text (tooltip, body);
          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_omni_bar_notification_row_activated (IdeOmniBar                *self,
                                         IdeNotificationListBoxRow *row,
                                         GtkListBox                *list_box)
{
  g_autofree gchar *default_action = NULL;
  g_autoptr(GVariant) default_target = NULL;
  IdeNotification *notif;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (IDE_IS_NOTIFICATION_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  notif = ide_notification_list_box_row_get_notification (row);

  if (ide_notification_get_default_action (notif, &default_action, &default_target))
    {
      gchar *name = strchr (default_action, '.');
      gchar *group = default_action;

      if (name != NULL)
        {
          *name = '\0';
          name++;
        }
      else
        {
          group = NULL;
          name = default_action;
        }

      dzl_gtk_widget_action (GTK_WIDGET (list_box), group, name, default_target);
    }
}

static void
ide_omni_bar_destroy (GtkWidget *widget)
{
  IdeOmniBar *self = (IdeOmniBar *)widget;

  g_assert (IDE_IS_OMNI_BAR (self));

  if (self->progress != NULL)
    _ide_gtk_progress_bar_stop_pulsing (self->progress);

  g_clear_object (&self->addins);
  g_clear_object (&self->gesture);
  g_clear_object (&self->motion);

  GTK_WIDGET_CLASS (ide_omni_bar_parent_class)->destroy (widget);
}

static void
ide_omni_bar_class_init (IdeOmniBarClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = ide_omni_bar_destroy;
  widget_class->query_tooltip = ide_omni_bar_query_tooltip;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-gui/ui/ide-omni-bar.ui");
  gtk_widget_class_set_css_name (widget_class, "omnibar");
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, entry_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, inner_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, notification_stack);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, notifications_list_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, outer_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, popover);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, progress);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, sections_box);
  gtk_widget_class_bind_template_child (widget_class, IdeOmniBar, top_stack);
  gtk_widget_class_bind_template_callback (widget_class, ide_omni_bar_notification_row_activated);

  g_type_ensure (DZL_TYPE_ENTRY_BOX);
  g_type_ensure (IDE_TYPE_NOTIFICATION_STACK);
}

static void
ide_omni_bar_init (IdeOmniBar *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_has_tooltip (GTK_WIDGET (self), TRUE);

  gtk_widget_add_events (GTK_WIDGET (self),
                         (GDK_POINTER_MOTION_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK));

  self->motion = gtk_event_controller_motion_new (GTK_WIDGET (self));
  gtk_event_controller_set_propagation_phase (self->motion, GTK_PHASE_CAPTURE);

  g_signal_connect_swapped (self->motion,
                            "enter",
                            G_CALLBACK (ide_omni_bar_motion_enter_cb),
                            self);

  g_signal_connect_swapped (self->motion,
                            "motion",
                            G_CALLBACK (ide_omni_bar_motion_cb),
                            self);

  g_signal_connect_swapped (self->motion,
                            "leave",
                            G_CALLBACK (ide_omni_bar_motion_leave_cb),
                            self);

  self->gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self));
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (self->gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect_swapped (self->gesture,
                            "pressed",
                            G_CALLBACK (multipress_pressed_cb),
                            self);
  g_signal_connect_swapped (self->gesture,
                            "released",
                            G_CALLBACK (multipress_released_cb),
                            self);
  g_signal_connect_swapped (self->gesture,
                            "cancel",
                            G_CALLBACK (multipress_cancel_cb),
                            self);

  g_signal_connect_object (self->notification_stack,
                           "changed",
                           G_CALLBACK (ide_omni_bar_notification_stack_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->popover,
                           "closed",
                           G_CALLBACK (ide_omni_bar_popover_closed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_widget_insert_action_group (GTK_WIDGET (self), "omnibar", G_ACTION_GROUP (self));

  ide_omni_bar_set_action_enabled (self, "move-previous", FALSE);
  ide_omni_bar_set_action_enabled (self, "move-next", FALSE);

  ide_widget_set_context_handler (GTK_WIDGET (self), ide_omni_bar_context_set_cb);
}

GtkWidget *
ide_omni_bar_new (void)
{
  return g_object_new (IDE_TYPE_OMNI_BAR, NULL);
}

static void
ide_omni_bar_move_next (IdeOmniBar *self,
                        GVariant   *param)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (param == NULL);

  ide_notification_stack_move_next (self->notification_stack);
}

static void
ide_omni_bar_move_previous (IdeOmniBar *self,
                            GVariant   *param)
{
  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (param == NULL);

  ide_notification_stack_move_previous (self->notification_stack);
}

/**
 * ide_omni_bar_add_status_icon:
 * @self: a #IdeOmniBar
 * @widget: the #GtkWidget to add
 * @priority: the sort priority for @widget
 *
 * Adds a status-icon style widget to the end of the omnibar. Generally,
 * you'll want this to be either a GtkButton, GtkLabel, or something simple.
 *
 * Since: 3.32
 */
void
ide_omni_bar_add_status_icon (IdeOmniBar *self,
                              GtkWidget  *widget,
                              gint        priority)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add_with_properties (GTK_CONTAINER (self->inner_box), widget,
                                     "pack-type", GTK_PACK_END,
                                     "priority", priority,
                                     NULL);
}

void
ide_omni_bar_add_button (IdeOmniBar  *self,
                         GtkWidget   *widget,
                         GtkPackType  pack_type,
                         gint         priority)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (pack_type == GTK_PACK_START ||
                    pack_type == GTK_PACK_END);

  gtk_container_add_with_properties (GTK_CONTAINER (self->outer_box), widget,
                                     "pack-type", pack_type,
                                     "priority", priority,
                                     NULL);
}

void
ide_omni_bar_set_placeholder (IdeOmniBar *self,
                              GtkWidget  *widget)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (!widget || GTK_IS_WIDGET (widget));

  if (self->placeholder == widget)
    return;

  if (self->placeholder)
    gtk_widget_destroy (self->placeholder);

  self->placeholder = widget;

  if (self->placeholder)
    {
      g_signal_connect (self->placeholder,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        self->placeholder);
      gtk_container_add_with_properties (GTK_CONTAINER (self->top_stack), self->placeholder,
                                         "name", "placeholder",
                                         NULL);
      if (self->notification_stack == NULL ||
          ide_notification_stack_is_empty (self->notification_stack))
        gtk_stack_set_visible_child_name (self->top_stack, "placeholder");
    }
}

static void
ide_omni_bar_add_child (GtkBuildable *buildable,
                          GtkBuilder   *builder,
                        GObject      *child,
                        const gchar  *type)
{
  IdeOmniBar *self = (IdeOmniBar *)buildable;

  g_assert (IDE_IS_OMNI_BAR (self));
  g_assert (GTK_IS_BUILDER (builder));
  g_assert (G_IS_OBJECT (child));

  if (ide_str_equal0 (type, "start") && GTK_IS_WIDGET (child))
    ide_omni_bar_add_button (IDE_OMNI_BAR (self),
                             GTK_WIDGET (child),
                             GTK_PACK_START,
                             0);
  else if (ide_str_equal0 (type, "end") && GTK_IS_WIDGET (child))
    ide_omni_bar_add_button (IDE_OMNI_BAR (self),
                             GTK_WIDGET (child),
                             GTK_PACK_END,
                             0);
  else if (ide_str_equal0 (type, "placeholder") && GTK_IS_WIDGET (child))
    ide_omni_bar_set_placeholder (IDE_OMNI_BAR (self), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = ide_omni_bar_add_child;
}

/**
 * ide_omni_bar_add_popover_section:
 * @self: an #IdeOmniBar
 * @widget: a #GtkWidget
 * @priority: sort priority for the section
 *
 * Adds @widget to the omnibar popover, sorted by @priority
 *
 * Since: 3.32
 */
void
ide_omni_bar_add_popover_section (IdeOmniBar *self,
                                  GtkWidget  *widget,
                                  gint        priority)
{
  g_return_if_fail (IDE_IS_OMNI_BAR (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_add_with_properties (GTK_CONTAINER (self->sections_box), widget,
                                     "priority", priority,
                                     NULL);
}
