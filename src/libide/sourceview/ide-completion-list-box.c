/* ide-completion-list-box.c
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

#define G_LOG_DOMAIN "ide-completion-list-box"

#include "config.h"

#include "ide-completion-context.h"
#include "ide-completion-list-box.h"
#include "ide-completion-list-box-row.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"

struct _IdeCompletionListBox
{
  DzlBin parent_instance;

  /* The box containing the rows. */
  GtkBox *box;

  /* The event box for button press events */
  GtkEventBox *events;

  /* Font stylign for rows */
  PangoAttrList *font_attrs;

  /*
   * The completion context that is being displayed.
   */
  IdeCompletionContext *context;

  /*
   * The handler for IdeCompletionContecxt::items-chaged which should
   * be disconnected when no longer needed.
   */
  gulong items_changed_handler;

  /*
   * The number of rows we expect to have visible to the user.
   */
  guint n_rows;

  /*
   * The currently selected index within the result set. Signed to
   * ensure our math in various places allows going negative to catch
   * lower edge.
   */
  gint selected;

  /*
   * This is set whenever we make a change that requires updating the
   * row content. We delay the update until the next frame callback so
   * that we only update once right before we draw the frame. This helps
   * reduce duplicate work when reacting to ::items-changed in the model.
   */
  guint queued_update;

  /*
   * These size groups are used to keep each portion of the proposal row
   * aligned with each other. Since we only have a fixed number of visible
   * rows, the overhead here is negligable by introducing the size cycle.
   */
  GtkSizeGroup *left_size_group;
  GtkSizeGroup *center_size_group;
  GtkSizeGroup *right_size_group;

  /*
   * The adjustments for scrolling the GtkScrollable.
   */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;

  /*
   * Gesture to handle button press/touch events.
   */
  GtkGesture *multipress_gesture;
};

typedef struct
{
  IdeCompletionListBox *self;
  IdeCompletionContext *context;
  guint n_items;
  guint position;
  guint selected;
} UpdateState;

enum {
  PROP_0,
  PROP_CONTEXT,
  PROP_PROPOSAL,
  PROP_N_ROWS,
  PROP_HADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VADJUSTMENT,
  PROP_VSCROLL_POLICY,
  N_PROPS
};

enum {
  REPOSITION,
  N_SIGNALS
};

static void ide_completion_list_box_queue_update (IdeCompletionListBox *self);

G_DEFINE_TYPE_WITH_CODE (IdeCompletionListBox, ide_completion_list_box, DZL_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static guint
ide_completion_list_box_get_offset (IdeCompletionListBox *self)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));

  return gtk_adjustment_get_value (self->vadjustment);
}

static void
ide_completion_list_box_set_offset (IdeCompletionListBox *self,
                                    guint                 offset)
{
  gdouble value = offset;
  gdouble page_size;
  gdouble upper;
  gdouble lower;

  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));

  lower = gtk_adjustment_get_lower (self->vadjustment);
  upper = gtk_adjustment_get_upper (self->vadjustment);
  page_size = gtk_adjustment_get_page_size (self->vadjustment);

  if (value > (upper - page_size))
    value = upper - page_size;

  if (value < lower)
    value = lower;

  gtk_adjustment_set_value (self->vadjustment, value);
}

static void
ide_completion_list_box_value_changed (IdeCompletionListBox *self,
                                       GtkAdjustment        *vadj)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (GTK_IS_ADJUSTMENT (vadj));

  ide_completion_list_box_queue_update (self);
}

static void
ide_completion_list_box_set_hadjustment (IdeCompletionListBox *self,
                                         GtkAdjustment        *hadjustment)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (!hadjustment || GTK_IS_ADJUSTMENT (hadjustment));

  if (g_set_object (&self->hadjustment, hadjustment))
    ide_completion_list_box_queue_update (self);
}

static void
ide_completion_list_box_set_vadjustment (IdeCompletionListBox *self,
                                         GtkAdjustment        *vadjustment)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (!vadjustment || GTK_IS_ADJUSTMENT (vadjustment));

  if (self->vadjustment == vadjustment)
    return;

  if (self->vadjustment)
    {
      g_signal_handlers_disconnect_by_func (self->vadjustment,
                                            G_CALLBACK (ide_completion_list_box_value_changed),
                                            self);
      g_clear_object (&self->vadjustment);
    }

  if (vadjustment)
    {
      self->vadjustment = g_object_ref (vadjustment);

      gtk_adjustment_set_lower (self->vadjustment, 0);
      gtk_adjustment_set_upper (self->vadjustment, 0);
      gtk_adjustment_set_value (self->vadjustment, 0);
      gtk_adjustment_set_step_increment (self->vadjustment, 1);
      gtk_adjustment_set_page_size (self->vadjustment, self->n_rows);
      gtk_adjustment_set_page_increment (self->vadjustment, self->n_rows);

      g_signal_connect_object (self->vadjustment,
                               "value-changed",
                               G_CALLBACK (ide_completion_list_box_value_changed),
                               self,
                               G_CONNECT_SWAPPED);
    }

  ide_completion_list_box_queue_update (self);
}

static void
ide_completion_list_box_add (GtkContainer *container,
                             GtkWidget    *widget)
{
  IdeCompletionListBox *self = (IdeCompletionListBox *)container;

  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (GTK_IS_WIDGET (widget));

  if (IDE_IS_COMPLETION_LIST_BOX_ROW (widget))
    gtk_container_add (GTK_CONTAINER (self->box), widget);
  else
    GTK_CONTAINER_CLASS (ide_completion_list_box_parent_class)->add (container, widget);
}

static guint
get_row_at_y (IdeCompletionListBox *self,
              gdouble               y)
{
  GtkAllocation alloc;
  guint offset;
  guint n_items;

  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (G_IS_LIST_MODEL (self->context));

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  offset = ide_completion_list_box_get_offset (self);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));
  n_items = MAX (1, MIN (self->n_rows, n_items));

  return offset + (y / (alloc.height / n_items));
}

static void
multipress_gesture_pressed (GtkGestureMultiPress *gesture,
                            guint                 n_press,
                            gdouble               x,
                            gdouble               y,
                            IdeCompletionListBox *self)
{
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));

  if (self->context == NULL)
    return;

  self->selected = get_row_at_y (self, y);
  ide_completion_list_box_queue_update (self);
}

static void
multipress_gesture_released (GtkGestureMultiPress    *gesture,
                             guint                    n_press,
                             gdouble                  x,
                             gdouble                  y,
                             IdeCompletionListBoxRow *self)
{
  g_assert (GTK_IS_GESTURE_MULTI_PRESS (gesture));
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));

}

static void
ide_completion_list_box_constructed (GObject *object)
{
  IdeCompletionListBox *self = (IdeCompletionListBox *)object;

  G_OBJECT_CLASS (ide_completion_list_box_parent_class)->constructed (object);

  if (self->hadjustment == NULL)
    self->hadjustment = gtk_adjustment_new (0, 0, 0, 0, 0, 0);

  if (self->vadjustment == NULL)
    self->vadjustment = gtk_adjustment_new (0, 0, 0, 0, 0, 0);

  gtk_adjustment_set_lower (self->hadjustment, 0);
  gtk_adjustment_set_upper (self->hadjustment, 0);
  gtk_adjustment_set_value (self->hadjustment, 0);

  ide_completion_list_box_queue_update (self);
}

static void
ide_completion_list_box_finalize (GObject *object)
{
  IdeCompletionListBox *self = (IdeCompletionListBox *)object;

  g_clear_object (&self->multipress_gesture);
  g_clear_object (&self->left_size_group);
  g_clear_object (&self->center_size_group);
  g_clear_object (&self->right_size_group);
  g_clear_object (&self->hadjustment);
  g_clear_object (&self->vadjustment);
  g_clear_pointer (&self->font_attrs, pango_attr_list_unref);

  G_OBJECT_CLASS (ide_completion_list_box_parent_class)->finalize (object);
}

static void
ide_completion_list_box_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  IdeCompletionListBox *self = IDE_COMPLETION_LIST_BOX (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_completion_list_box_get_context (self));
      break;

    case PROP_PROPOSAL:
      g_value_take_object (value, ide_completion_list_box_get_proposal (self));
      break;

    case PROP_N_ROWS:
      g_value_set_uint (value, ide_completion_list_box_get_n_rows (self));
      break;

    case PROP_HADJUSTMENT:
      g_value_set_object (value, self->hadjustment);
      break;

    case PROP_VADJUSTMENT:
      g_value_set_object (value, self->vadjustment);
      break;

    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, GTK_SCROLL_NATURAL);
      break;

    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, GTK_SCROLL_NATURAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_list_box_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  IdeCompletionListBox *self = IDE_COMPLETION_LIST_BOX (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_completion_list_box_set_context (self, g_value_get_object (value));
      break;

    case PROP_N_ROWS:
      ide_completion_list_box_set_n_rows (self, g_value_get_uint (value));
      break;

    case PROP_HADJUSTMENT:
      ide_completion_list_box_set_hadjustment (self, g_value_get_object (value));
      break;

    case PROP_VADJUSTMENT:
      ide_completion_list_box_set_vadjustment (self, g_value_get_object (value));
      break;

    case PROP_HSCROLL_POLICY:
      /* Do nothing */
      break;

    case PROP_VSCROLL_POLICY:
      /* Do nothing */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_list_box_class_init (IdeCompletionListBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->constructed = ide_completion_list_box_constructed;
  object_class->finalize = ide_completion_list_box_finalize;
  object_class->get_property = ide_completion_list_box_get_property;
  object_class->set_property = ide_completion_list_box_set_property;

  container_class->add = ide_completion_list_box_add;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The context being displayed",
                         IDE_TYPE_COMPLETION_CONTEXT,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_HADJUSTMENT] =
    g_param_spec_object ("hadjustment", NULL, NULL,
                         GTK_TYPE_ADJUSTMENT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_HSCROLL_POLICY] =
    g_param_spec_enum ("hscroll-policy", NULL, NULL,
                       GTK_TYPE_SCROLLABLE_POLICY,
                       GTK_SCROLL_NATURAL,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VADJUSTMENT] =
    g_param_spec_object ("vadjustment", NULL, NULL,
                         GTK_TYPE_ADJUSTMENT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_VSCROLL_POLICY] =
    g_param_spec_enum ("vscroll-policy", NULL, NULL,
                       GTK_TYPE_SCROLLABLE_POLICY,
                       GTK_SCROLL_NATURAL,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PROPOSAL] =
    g_param_spec_object ("proposal",
                         "Proposal",
                         "The proposal that is currently selected",
                         IDE_TYPE_COMPLETION_PROPOSAL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_N_ROWS] =
    g_param_spec_uint ("n-rows",
                       "N Rows",
                       "The number of visible rows",
                       1, 32, 5,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [REPOSITION] =
    g_signal_new_class_handler ("reposition",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL, NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [REPOSITION],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);

  gtk_widget_class_set_css_name (widget_class, "list");
}

static void
ide_completion_list_box_init (IdeCompletionListBox *self)
{
  self->events = g_object_new (GTK_TYPE_EVENT_BOX,
                               "visible", TRUE,
                               NULL);
  gtk_widget_add_events (GTK_WIDGET (self->events), GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  g_signal_connect (self->events,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->events);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->events));

  self->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_VERTICAL,
                            "visible", TRUE,
                            NULL);
  g_signal_connect (self->box,
                    "destroy",
                    G_CALLBACK (gtk_widget_destroyed),
                    &self->box);
  gtk_container_add (GTK_CONTAINER (self->events), GTK_WIDGET (self->box));

  self->left_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->center_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->right_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  self->multipress_gesture = gtk_gesture_multi_press_new (GTK_WIDGET (self->events));
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->multipress_gesture), GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->multipress_gesture), FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->multipress_gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect_object (self->multipress_gesture, "pressed",
                           G_CALLBACK (multipress_gesture_pressed), self, 0);
  g_signal_connect_object (self->multipress_gesture, "released",
                           G_CALLBACK (multipress_gesture_released), self, 0);
}

static void
ide_completion_list_box_update_row_cb (GtkWidget *widget,
                                       gpointer   user_data)
{
  g_autoptr(IdeCompletionProposal) proposal = NULL;
  g_autoptr(IdeCompletionProvider) provider = NULL;
  UpdateState *state = user_data;

  g_assert (IDE_IS_COMPLETION_LIST_BOX_ROW (widget));
  g_assert (state != NULL);

  if (state->position == state->selected)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_SELECTED);

  if (state->context != NULL && state->position < state->n_items)
    ide_completion_context_get_item_full (state->context, state->position, &provider, &proposal);

  ide_completion_list_box_row_set_proposal (IDE_COMPLETION_LIST_BOX_ROW (widget), proposal);

  if (provider && proposal)
    {
      g_autofree gchar *typed_text = NULL;
      GtkTextIter begin, end;

      if (ide_completion_context_get_bounds (state->context, &begin, &end))
        typed_text = gtk_text_iter_get_slice (&begin, &end);

      ide_completion_provider_display_proposal (provider,
                                                IDE_COMPLETION_LIST_BOX_ROW (widget),
                                                state->context,
                                                typed_text,
                                                proposal);
    }

  gtk_widget_set_visible (widget, proposal != NULL);

  state->position++;
}

static gboolean
ide_completion_list_box_update_cb (GtkWidget     *widget,
                                   GdkFrameClock *frame_clock,
                                   gpointer       user_data)
{
  IdeCompletionListBox *self = (IdeCompletionListBox *)widget;
  UpdateState state = {0};

  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));

  state.self = self;
  state.context = self->context;
  state.position = ide_completion_list_box_get_offset (self);
  state.selected = self->selected;

  if (self->context != NULL)
    state.n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));

  state.position = MIN (state.position, MAX (state.n_items, self->n_rows) - self->n_rows);
  state.selected = MIN (self->selected, state.n_items ? state.n_items - 1 : 0);

  if (gtk_adjustment_get_upper (self->vadjustment) != state.n_items)
    gtk_adjustment_set_upper (self->vadjustment, state.n_items);

  gtk_container_foreach (GTK_CONTAINER (self->box),
                         ide_completion_list_box_update_row_cb,
                         &state);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROPOSAL]);

  g_signal_emit (self, signals [REPOSITION], 0);

  /* Do this last so that we block any follow-up queue_updates */
  self->queued_update = 0;

  return G_SOURCE_REMOVE;
}

static void
ide_completion_list_box_queue_update (IdeCompletionListBox *self)
{
  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));

  if (self->queued_update == 0)
    {
      self->queued_update = gtk_widget_add_tick_callback (GTK_WIDGET (self),
                                                          ide_completion_list_box_update_cb,
                                                          NULL, NULL);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

GtkWidget *
ide_completion_list_box_new (void)
{
  return g_object_new (IDE_TYPE_COMPLETION_LIST_BOX, NULL);
}

guint
ide_completion_list_box_get_n_rows (IdeCompletionListBox *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), 0);

  return self->n_rows;
}

void
ide_completion_list_box_set_n_rows (IdeCompletionListBox *self,
                                    guint                 n_rows)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX (self));
  g_return_if_fail (n_rows > 0);
  g_return_if_fail (n_rows <= 32);

  if (n_rows != self->n_rows)
    {
      gtk_container_foreach (GTK_CONTAINER (self->box),
                             (GtkCallback)gtk_widget_destroy,
                             NULL);

      self->n_rows = n_rows;

      if (self->vadjustment != NULL)
        gtk_adjustment_set_page_size (self->vadjustment, n_rows);

      for (guint i = 0; i < n_rows; i++)
        {
          GtkWidget *row = ide_completion_list_box_row_new ();

          _ide_completion_list_box_row_attach (IDE_COMPLETION_LIST_BOX_ROW (row),
                                               self->left_size_group,
                                               self->center_size_group,
                                               self->right_size_group);
          _ide_completion_list_box_row_set_attrs (IDE_COMPLETION_LIST_BOX_ROW (row),
                                                  self->font_attrs);
          gtk_container_add (GTK_CONTAINER (self), row);
        }

      ide_completion_list_box_queue_update (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ROWS]);
    }
}

/**
 * ide_completion_list_box_get_proposal:
 * @self: a #IdeCompletionListBox
 *
 * Gets the currently selected proposal, or %NULL if no proposal is selected
 *
 * Returns: (nullable) (transfer full): a #IdeCompletionProposal or %NULL
 *
 * Since: 3.32
 */
IdeCompletionProposal *
ide_completion_list_box_get_proposal (IdeCompletionListBox *self)
{
  IdeCompletionProposal *ret = NULL;

  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), NULL);

  if (self->context != NULL &&
      self->selected < g_list_model_get_n_items (G_LIST_MODEL (self->context)))
    ret = g_list_model_get_item (G_LIST_MODEL (self->context), self->selected);

  g_return_val_if_fail (!ret || IDE_IS_COMPLETION_PROPOSAL (ret), NULL);

  return ret;
}

/**
 * ide_completion_list_box_get_selected:
 * @self: an #IdeCompletionListBox
 * @provider: (out) (transfer full) (optional): a location for the provider
 * @proposal: (out) (transfer full) (optional): a location for the proposal
 *
 * Gets the selected item if there is any.
 *
 * If there is a selection, %TRUE is returned and @provider and @proposal
 * are set to the selected provider/proposal.
 *
 * Returns: %TRUE if there is a selection
 *
 * Since: 3.32
 */
gboolean
ide_completion_list_box_get_selected (IdeCompletionListBox   *self,
                                      IdeCompletionProvider **provider,
                                      IdeCompletionProposal **proposal)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), FALSE);

  if (self->context != NULL)
    {
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));

      if (n_items > 0)
        {
          guint selected = MIN (self->selected, n_items - 1);
          ide_completion_context_get_item_full (self->context, selected, provider, proposal);
          return TRUE;
        }
    }

  return FALSE;
}

/**
 * ide_completion_list_box_get_context:
 * @self: a #IdeCompletionListBox
 *
 * Gets the context that is being displayed in the list box.
 *
 * Returns: (transfer none) (nullable): an #IdeCompletionContext or %NULL
 *
 * Since: 3.32
 */
IdeCompletionContext *
ide_completion_list_box_get_context (IdeCompletionListBox *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), NULL);

  return self->context;
}

static void
ide_completion_list_box_items_changed_cb (IdeCompletionListBox *self,
                                          guint                 position,
                                          guint                 removed,
                                          guint                 added,
                                          GListModel           *model)
{
  guint offset;

  g_assert (IDE_IS_COMPLETION_LIST_BOX (self));
  g_assert (G_IS_LIST_MODEL (model));

  offset = ide_completion_list_box_get_offset (self);

  /* Skip widget resize if results are not visible */
  if (position >= offset + self->n_rows)
    return;

  ide_completion_list_box_queue_update (self);
}

/**
 * ide_completion_list_box_set_context:
 * @self: a #IdeCompletionListBox
 * @context: the #IdeCompletionContext
 *
 * Sets the context to be displayed.
 *
 * Since: 3.32
 */
void
ide_completion_list_box_set_context (IdeCompletionListBox *self,
                                     IdeCompletionContext *context)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX (self));
  g_return_if_fail (!context || IDE_IS_COMPLETION_CONTEXT (context));

  if (self->context == context)
    return;

  if (self->context != NULL && self->items_changed_handler != 0)
    {
      g_signal_handler_disconnect (self->context, self->items_changed_handler);
      self->items_changed_handler = 0;
    }

  g_set_object (&self->context, context);

  if (self->context != NULL)
    self->items_changed_handler =
      g_signal_connect_object (self->context,
                               "items-changed",
                               G_CALLBACK (ide_completion_list_box_items_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  self->selected = 0;
  gtk_adjustment_set_value (self->vadjustment, 0);

  ide_completion_list_box_queue_update (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
}

static void
get_first_cb (GtkWidget *widget,
              gpointer   user_data)
{
  GtkWidget **row = user_data;

  if (*row == NULL)
    *row = widget;
}

IdeCompletionListBoxRow *
_ide_completion_list_box_get_first (IdeCompletionListBox *self)
{
  IdeCompletionListBoxRow *row = NULL;

  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), NULL);

  gtk_container_foreach (GTK_CONTAINER (self->box), get_first_cb, &row);

  return row;
}

void
ide_completion_list_box_move_cursor (IdeCompletionListBox *self,
                                     GtkMovementStep       step,
                                     gint                  direction)
{
  gint n_items;
  gint offset;

  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX (self));

  if (self->context == NULL || direction == 0)
    return;

  if (!(n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context))))
    return;

  /* n_items is signed so that we can do negative comparison */
  if (n_items < 0)
    return;

  if (step == GTK_MOVEMENT_BUFFER_ENDS)
    {
      if (direction > 0)
        {
          ide_completion_list_box_set_offset (self, n_items);
          self->selected = n_items - 1;
        }
      else
        {
          ide_completion_list_box_set_offset (self, 0);
          self->selected = 0;
        }

      ide_completion_list_box_queue_update (self);

      return;
    }

  if (direction < 0 && self->selected == 0)
    return;

  if (direction > 0 && self->selected == n_items - 1)
    return;

  if (step == GTK_MOVEMENT_PAGES)
    direction *= self->n_rows;

  if ((self->selected + direction) > n_items)
    self->selected = n_items - 1;
  else if ((self->selected + direction) < 0)
    self->selected = 0;
  else
    self->selected += direction;

  offset = ide_completion_list_box_get_offset (self);

  if (self->selected < offset)
    ide_completion_list_box_set_offset (self, self->selected);
  else if (self->selected >= (offset + self->n_rows))
    ide_completion_list_box_set_offset (self, self->selected - self->n_rows + 1);

  ide_completion_list_box_queue_update (self);
}

gboolean
_ide_completion_list_box_key_activates (IdeCompletionListBox *self,
                                        const GdkEventKey    *key)
{
  g_autoptr(IdeCompletionProvider) provider = NULL;
  g_autoptr(IdeCompletionProposal) proposal = NULL;

  g_return_val_if_fail (IDE_IS_COMPLETION_LIST_BOX (self), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  if (ide_completion_list_box_get_selected (self, &provider, &proposal))
    {
      if (ide_completion_provider_key_activates (provider, proposal, key))
        return TRUE;
    }

  return FALSE;
}

static void
update_font_desc (GtkWidget *widget,
                  gpointer   user_data)
{
  PangoAttrList *attrs = user_data;

  if (IDE_IS_COMPLETION_LIST_BOX_ROW (widget))
    _ide_completion_list_box_row_set_attrs (IDE_COMPLETION_LIST_BOX_ROW (widget), attrs);
}

void
_ide_completion_list_box_set_font_desc (IdeCompletionListBox       *self,
                                        const PangoFontDescription *font_desc)
{
  g_return_if_fail (IDE_IS_COMPLETION_LIST_BOX (self));

  g_clear_pointer (&self->font_attrs, pango_attr_list_unref);

  if (font_desc)
    {
      self->font_attrs = pango_attr_list_new ();
      if (font_desc)
        pango_attr_list_insert (self->font_attrs, pango_attr_font_desc_new (font_desc));
    }

  gtk_container_foreach (GTK_CONTAINER (self->box), update_font_desc, self->font_attrs);
}
