/* ide-scrubber-revealer.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-scrubber-revealer"

#include "config.h"

#include "ide-gtk-enums.h"
#include "ide-scrubber-revealer.h"

#define DISMISS_TIMEOUT_MSEC     1500
#define TRANSITION_DURATION_MSEC  250

struct _IdeScrubberRevealer
{
  GtkWidget                parent_instance;

  GtkWidget               *revealer;
  GtkWidget               *content;

  double                   last_x;
  double                   last_y;

  IdeScrubberRevealPolicy  policy;

  guint                    dismiss_source;

  guint                    hold : 1;
};

enum {
  PROP_0,
  PROP_CONTENT,
  PROP_SCRUBBER,
  PROP_POLICY,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeScrubberRevealer, ide_scrubber_revealer, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
ide_scrubber_revealer_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  IdeScrubberRevealer *self = (IdeScrubberRevealer *)widget;
  int content_min = 0;
  int content_nat = 0;
  int scrubber_min = 0;
  int scrubber_nat = 0;

  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  *minimum_baseline = -1;
  *natural_baseline = -1;

  if (self->content)
    gtk_widget_measure (self->content, orientation, for_size, &content_min, &content_nat, NULL, NULL);
  gtk_widget_measure (self->revealer, orientation, for_size, &scrubber_min, &scrubber_nat, NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      switch (self->policy)
        {
        case IDE_SCRUBBER_REVEAL_POLICY_NEVER:
          *minimum = content_min;
          *natural = content_nat;
          break;

        case IDE_SCRUBBER_REVEAL_POLICY_AUTO:
          *minimum = content_min;
          *natural = content_nat + scrubber_nat;
          break;

        case IDE_SCRUBBER_REVEAL_POLICY_ALWAYS:
          *minimum = content_min + scrubber_min;
          *natural = content_nat + scrubber_nat;
          break;

        default:
          g_assert_not_reached ();
        }
    }
  else
    {
      *minimum = MAX (content_min, scrubber_min);
      *natural = MAX (content_nat, scrubber_nat);
    }
}

static void
ide_scrubber_revealer_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  IdeScrubberRevealer *self = (IdeScrubberRevealer *)widget;
  GskTransform *transform = NULL;
  GtkRequisition content_min = {0};
  GtkRequisition content_nat = {0};
  GtkRequisition scrubber_min = {0};
  GtkRequisition scrubber_nat = {0};

  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->content)
    gtk_widget_get_preferred_size (self->content, &content_min, &content_nat);
  gtk_widget_get_preferred_size (self->revealer, &scrubber_min, &scrubber_nat);

  switch (self->policy)
    {
    case IDE_SCRUBBER_REVEAL_POLICY_NEVER:
      if (self->content)
        gtk_widget_allocate (self->content, width, height, -1, NULL);
      break;

    case IDE_SCRUBBER_REVEAL_POLICY_ALWAYS:
      if (self->content)
        gtk_widget_allocate (self->content, width - scrubber_min.width, height, -1, NULL);
      transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (width - scrubber_min.width, 0));
      break;

    case IDE_SCRUBBER_REVEAL_POLICY_AUTO:
      if (self->content)
        gtk_widget_allocate (self->content, width, height, -1, NULL);
      transform = gsk_transform_translate (NULL, &GRAPHENE_POINT_INIT (width - scrubber_min.width, 0));
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_widget_allocate (self->revealer, scrubber_min.width, height, -1, transform);
}

static void
ide_scrubber_revealer_snapshot (GtkWidget   *widget,
                                GtkSnapshot *snapshot)
{
  IdeScrubberRevealer *self = (IdeScrubberRevealer *)widget;

  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->content)
    gtk_widget_snapshot_child (widget, self->content, snapshot);

  if (self->policy != IDE_SCRUBBER_REVEAL_POLICY_NEVER)
    gtk_widget_snapshot_child (widget, self->revealer, snapshot);
}

static void
ide_scrubber_revealer_dismiss (IdeScrubberRevealer *self)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->policy == IDE_SCRUBBER_REVEAL_POLICY_AUTO)
    {
      g_clear_handle_id (&self->dismiss_source, g_source_remove);
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), FALSE);
    }
}

static gboolean
ide_scrubber_revealer_dismiss_source_func (gpointer data)
{
  IdeScrubberRevealer *self = data;

  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  self->dismiss_source = 0;

  ide_scrubber_revealer_dismiss (self);

  return G_SOURCE_REMOVE;
}

static void
ide_scrubber_revealer_present (IdeScrubberRevealer *self)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->policy == IDE_SCRUBBER_REVEAL_POLICY_AUTO)
    {
      g_clear_handle_id (&self->dismiss_source, g_source_remove);

      if (!self->hold)
        self->dismiss_source = g_timeout_add (DISMISS_TIMEOUT_MSEC,
                                              ide_scrubber_revealer_dismiss_source_func,
                                              self);

      gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer), TRUE);
    }
}

static void
ide_scrubber_revealer_hold (IdeScrubberRevealer *self)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->policy == IDE_SCRUBBER_REVEAL_POLICY_AUTO)
    {
      self->hold = TRUE;
      ide_scrubber_revealer_present (self);
    }
}

static void
ide_scrubber_revealer_release (IdeScrubberRevealer *self)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));

  if (self->policy == IDE_SCRUBBER_REVEAL_POLICY_AUTO)
    {
      self->hold = FALSE;
      ide_scrubber_revealer_present (self);
    }
}

static void
ide_scrubber_revealer_enter_notify_cb (IdeScrubberRevealer      *self,
                                       double                    x,
                                       double                    y,
                                       GtkEventControllerMotion *motion)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  ide_scrubber_revealer_present (self);
}

static void
ide_scrubber_revealer_motion_notify_cb (IdeScrubberRevealer      *self,
                                        double                    x,
                                        double                    y,
                                        GtkEventControllerMotion *motion)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  if (x != self->last_x || y != self->last_y)
    {
      self->last_x = x;
      self->last_y = y;

      ide_scrubber_revealer_present (self);
    }
}

static void
ide_scrubber_revealer_leave_notify_cb (IdeScrubberRevealer      *self,
                                       GtkEventControllerMotion *motion)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (motion));

  ide_scrubber_revealer_dismiss (self);
}

static gboolean
ide_scrubber_revealer_scroll_notify_cb (IdeScrubberRevealer      *self,
                                        double                    x,
                                        double                    y,
                                        GtkEventControllerScroll *scroll)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));

  ide_scrubber_revealer_present (self);

  return FALSE;
}

static void
ide_scrubber_revealer_drag_begin_cb (IdeScrubberRevealer *self,
                                     double               x,
                                     double               y,
                                     GtkGestureDrag      *drag)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_GESTURE_DRAG (drag));

  ide_scrubber_revealer_hold (self);
}

static void
ide_scrubber_revealer_drag_end_cb (IdeScrubberRevealer *self,
                                   double               x,
                                   double               y,
                                   GtkGestureDrag      *drag)
{
  g_assert (IDE_IS_SCRUBBER_REVEALER (self));
  g_assert (GTK_IS_GESTURE_DRAG (drag));

  ide_scrubber_revealer_release (self);
}

static void
ide_scrubber_revealer_dispose (GObject *object)
{
  IdeScrubberRevealer *self = (IdeScrubberRevealer *)object;

  g_clear_pointer (&self->revealer, gtk_widget_unparent);
  g_clear_pointer (&self->content, gtk_widget_unparent);
  g_clear_handle_id (&self->dismiss_source, g_source_remove);

  G_OBJECT_CLASS (ide_scrubber_revealer_parent_class)->dispose (object);
}

static void
ide_scrubber_revealer_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeScrubberRevealer *self = IDE_SCRUBBER_REVEALER (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, ide_scrubber_revealer_get_content (self));
      break;

    case PROP_SCRUBBER:
      g_value_set_object (value, ide_scrubber_revealer_get_scrubber (self));
      break;

    case PROP_POLICY:
      g_value_set_enum (value, ide_scrubber_revealer_get_policy (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_scrubber_revealer_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeScrubberRevealer *self = IDE_SCRUBBER_REVEALER (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      ide_scrubber_revealer_set_content (self, g_value_get_object (value));
      break;

    case PROP_SCRUBBER:
      ide_scrubber_revealer_set_scrubber (self, g_value_get_object (value));
      break;

    case PROP_POLICY:
      ide_scrubber_revealer_set_policy (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_scrubber_revealer_class_init (IdeScrubberRevealerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_scrubber_revealer_dispose;
  object_class->get_property = ide_scrubber_revealer_get_property;
  object_class->set_property = ide_scrubber_revealer_set_property;

  widget_class->measure = ide_scrubber_revealer_measure;
  widget_class->size_allocate = ide_scrubber_revealer_size_allocate;
  widget_class->snapshot = ide_scrubber_revealer_snapshot;

  properties [PROP_CONTENT] =
    g_param_spec_object ("content", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCRUBBER] =
    g_param_spec_object ("scrubber", NULL, NULL,
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_POLICY] =
    g_param_spec_enum ("policy", NULL, NULL,
                       IDE_TYPE_SCRUBBER_REVEAL_POLICY,
                       IDE_SCRUBBER_REVEAL_POLICY_NEVER,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_scrubber_revealer_init (IdeScrubberRevealer *self)
{
  GtkEventController *motion;
  GtkEventController *scroll;
  GtkGesture *drag;

  self->policy = IDE_SCRUBBER_REVEAL_POLICY_NEVER;
  self->revealer = g_object_new (GTK_TYPE_REVEALER,
                                 "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT,
                                 "transition-duration", TRANSITION_DURATION_MSEC,
                                 NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->revealer), GTK_WIDGET (self));

  motion = gtk_event_controller_motion_new ();
  g_signal_connect_object (motion,
                           "enter",
                           G_CALLBACK (ide_scrubber_revealer_enter_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (motion,
                           "leave",
                           G_CALLBACK (ide_scrubber_revealer_leave_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (motion,
                           "motion",
                           G_CALLBACK (ide_scrubber_revealer_motion_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), motion);

  scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL|GTK_EVENT_CONTROLLER_SCROLL_KINETIC);
  gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
  g_signal_connect_object (scroll,
                           "scroll",
                           G_CALLBACK (ide_scrubber_revealer_scroll_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), scroll);

  drag = gtk_gesture_drag_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drag), GTK_PHASE_CAPTURE);
  g_signal_connect_object (drag,
                           "drag-begin",
                           G_CALLBACK (ide_scrubber_revealer_drag_begin_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (drag,
                           "drag-end",
                           G_CALLBACK (ide_scrubber_revealer_drag_end_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag));
}

GtkWidget *
ide_scrubber_revealer_new (void)
{
  return g_object_new (IDE_TYPE_SCRUBBER_REVEALER, NULL);
}

/**
 * ide_scrubber_revealer_get_content:
 * @self: a #IdeScrubberRevealer
 *
 * Returns: (transfer none) (nullable): a #GtkWidget or %NULL
 */
GtkWidget *
ide_scrubber_revealer_get_content (IdeScrubberRevealer *self)
{
  g_return_val_if_fail (IDE_IS_SCRUBBER_REVEALER (self), NULL);

  return self->content;
}

/**
 * ide_scrubber_revealer_get_scrubber:
 * @self: a #IdeScrubberRevealer
 *
 * Returns: (transfer none) (nullable): a #GtkWidget or %NULL
 */
GtkWidget *
ide_scrubber_revealer_get_scrubber (IdeScrubberRevealer *self)
{
  g_return_val_if_fail (IDE_IS_SCRUBBER_REVEALER (self), NULL);

  return gtk_revealer_get_child (GTK_REVEALER (self->revealer));
}

IdeScrubberRevealPolicy
ide_scrubber_revealer_get_policy (IdeScrubberRevealer *self)
{
  g_return_val_if_fail (IDE_IS_SCRUBBER_REVEALER (self), 0);

  return self->policy;
}

void
ide_scrubber_revealer_set_content (IdeScrubberRevealer *self,
                                   GtkWidget           *content)
{
  g_return_if_fail (IDE_IS_SCRUBBER_REVEALER (self));
  g_return_if_fail (!content || GTK_IS_WIDGET (content));
  g_return_if_fail (gtk_widget_get_parent (content) == NULL);

  if (content == self->content)
    return;

  if (content != NULL)
    gtk_widget_insert_after (content, GTK_WIDGET (self), NULL);

  if (self->content != NULL)
    gtk_widget_unparent (self->content);

  self->content = content;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTENT]);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
ide_scrubber_revealer_set_scrubber (IdeScrubberRevealer *self,
                                    GtkWidget           *scrubber)
{
  g_return_if_fail (IDE_IS_SCRUBBER_REVEALER (self));
  g_return_if_fail (!scrubber || GTK_IS_WIDGET (scrubber));
  g_return_if_fail (gtk_widget_get_parent (scrubber) == NULL);

  if (scrubber == ide_scrubber_revealer_get_scrubber (self))
    return;

  gtk_revealer_set_child (GTK_REVEALER (self->revealer), scrubber);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CONTENT]);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
ide_scrubber_revealer_set_policy (IdeScrubberRevealer     *self,
                                  IdeScrubberRevealPolicy  policy)
{
  g_return_if_fail (IDE_IS_SCRUBBER_REVEALER (self));
  g_return_if_fail (policy <= IDE_SCRUBBER_REVEAL_POLICY_ALWAYS);

  if (policy == self->policy)
    return;

  self->policy = policy;

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->revealer),
                                 policy == IDE_SCRUBBER_REVEAL_POLICY_ALWAYS);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POLICY]);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}
