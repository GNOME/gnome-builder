/* egg-suggestion-popover.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "egg-suggestion-popover"

#include <glib/gi18n.h>

#include "egg-animation.h"
#include "egg-elastic-bin.h"
#include "egg-suggestion.h"
#include "egg-suggestion-popover.h"
#include "egg-suggestion-row.h"

struct _EggSuggestionPopover
{
  GtkWindow           parent_instance;

  GtkWidget          *relative_to;
  GtkWindow          *transient_for;
  GtkRevealer        *revealer;
  GtkScrolledWindow  *scrolled_window;
  GtkListBox         *list_box;

  GListModel         *model;

  GType               row_type;

  gulong              delete_event_handler;
  gulong              configure_event_handler;
  gulong              size_allocate_handler;
  gulong              items_changed_handler;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_RELATIVE_TO,
  PROP_SELECTED,
  N_PROPS
};

enum {
  SUGGESTION_ACTIVATED,
  N_SIGNALS
};

G_DEFINE_TYPE (EggSuggestionPopover, egg_suggestion_popover, GTK_TYPE_WINDOW)

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
egg_suggestion_popover_reposition (EggSuggestionPopover *self)
{
  gint width;
  gint x;
  gint y;

  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->relative_to == NULL ||
      self->transient_for == NULL ||
      !gtk_widget_get_mapped (self->relative_to) ||
      !gtk_widget_get_mapped (GTK_WIDGET (self->transient_for)))
    return;

  gtk_window_get_size (self->transient_for, &width, NULL);
  gtk_widget_set_size_request (GTK_WIDGET (self), width, -1);
  gtk_window_get_position (self->transient_for, &x, &y);

  /*
   * XXX: This is just a hack for testing so we get the placement right.
   *
   *      What we should really do is allow hte EggSuggestionEntry to set our
   *      relative-to property by wrapping it. That would all the caller to
   *      place the popover relative to the main content area of the window
   *      as might be desired for a URL entry or global application search.
   */

  gtk_window_move (GTK_WINDOW (self), x, y + 47);
}

/**
 * egg_suggestion_popover_get_relative_to:
 * @self: a #EggSuggestionPopover
 *
 * Returns: (transfer none) (nullable): A #GtkWidget or %NULL.
 */
GtkWidget *
egg_suggestion_popover_get_relative_to (EggSuggestionPopover *self)
{

  g_return_val_if_fail (EGG_IS_SUGGESTION_POPOVER (self), NULL);

  return self->relative_to;
}

void
egg_suggestion_popover_set_relative_to (EggSuggestionPopover *self,
                                        GtkWidget            *relative_to)
{
  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));
  g_return_if_fail (!relative_to || GTK_IS_WIDGET (relative_to));

  if (self->relative_to != relative_to)
    {
      if (self->relative_to != NULL)
        {
          g_signal_handlers_disconnect_by_func (self->relative_to,
                                                G_CALLBACK (gtk_widget_destroyed),
                                                &self->relative_to);
          self->relative_to = NULL;
        }

      if (relative_to != NULL)
        {
          self->relative_to = relative_to;
          g_signal_connect (self->relative_to,
                            "destroy",
                            G_CALLBACK (gtk_widget_destroyed),
                            &self->relative_to);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RELATIVE_TO]);
    }
}

static void
egg_suggestion_popover_hide (GtkWidget *widget)
{
  EggSuggestionPopover *self = (EggSuggestionPopover *)widget;

  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->transient_for != NULL)
    gtk_window_group_remove_window (gtk_window_get_group (self->transient_for),
                                    GTK_WINDOW (self));

  g_signal_handler_disconnect (self->transient_for, self->delete_event_handler);
  g_signal_handler_disconnect (self->transient_for, self->size_allocate_handler);
  g_signal_handler_disconnect (self->transient_for, self->configure_event_handler);

  self->delete_event_handler = 0;
  self->size_allocate_handler = 0;
  self->configure_event_handler = 0;

  self->transient_for = NULL;

  GTK_WIDGET_CLASS (egg_suggestion_popover_parent_class)->hide (widget);
}

static void
egg_suggestion_popover_transient_for_size_allocate (EggSuggestionPopover *self,
                                                    GtkAllocation        *allocation,
                                                    GtkWindow            *toplevel)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (allocation != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  egg_suggestion_popover_reposition (self);
}

static gboolean
egg_suggestion_popover_transient_for_delete_event (EggSuggestionPopover *self,
                                                   GdkEvent             *event,
                                                   GtkWindow            *toplevel)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  gtk_widget_hide (GTK_WIDGET (self));

  return FALSE;
}

static gboolean
egg_suggestion_popover_transient_for_configure_event (EggSuggestionPopover *self,
                                                      GdkEvent             *event,
                                                      GtkWindow            *toplevel)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WINDOW (toplevel));

  gtk_widget_hide (GTK_WIDGET (self));

  return FALSE;
}

static void
egg_suggestion_popover_show (GtkWidget *widget)
{
  EggSuggestionPopover *self = (EggSuggestionPopover *)widget;

  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->relative_to != NULL)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self->relative_to), GTK_TYPE_WINDOW);

      if (GTK_IS_WINDOW (toplevel))
        {
          self->transient_for = GTK_WINDOW (toplevel);
          gtk_window_group_add_window (gtk_window_get_group (self->transient_for),
                                       GTK_WINDOW (self));
          self->delete_event_handler =
            g_signal_connect_object (toplevel,
                                     "delete-event",
                                     G_CALLBACK (egg_suggestion_popover_transient_for_delete_event),
                                     self,
                                     G_CONNECT_SWAPPED);
          self->size_allocate_handler =
            g_signal_connect_object (toplevel,
                                     "size-allocate",
                                     G_CALLBACK (egg_suggestion_popover_transient_for_size_allocate),
                                     self,
                                     G_CONNECT_SWAPPED | G_CONNECT_AFTER);
          self->configure_event_handler =
            g_signal_connect_object (toplevel,
                                     "configure-event",
                                     G_CALLBACK (egg_suggestion_popover_transient_for_configure_event),
                                     self,
                                     G_CONNECT_SWAPPED);
          egg_suggestion_popover_reposition (self);
        }
    }

  GTK_WIDGET_CLASS (egg_suggestion_popover_parent_class)->show (widget);
}

static void
egg_suggestion_popover_screen_changed (GtkWidget *widget,
                                       GdkScreen *previous_screen)
{
  GdkScreen *screen;
  GdkVisual *visual;

  GTK_WIDGET_CLASS (egg_suggestion_popover_parent_class)->screen_changed (widget, previous_screen);

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual != NULL)
    gtk_widget_set_visual (widget, visual);
}

static void
egg_suggestion_popover_realize (GtkWidget *widget)
{
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual != NULL)
    gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (egg_suggestion_popover_parent_class)->realize (widget);
}

static void
egg_suggestion_popover_notify_child_revealed (EggSuggestionPopover *self,
                                              GParamSpec           *pspec,
                                              GtkRevealer          *revealer)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (GTK_IS_REVEALER (revealer));

  if (!gtk_revealer_get_reveal_child (self->revealer))
    gtk_widget_hide (GTK_WIDGET (self));
}

static void
egg_suggestion_popover_list_box_row_activated (EggSuggestionPopover *self,
                                               EggSuggestionRow     *row,
                                               GtkListBox           *list_box)
{
  EggSuggestion *suggestion;

  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (EGG_IS_SUGGESTION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  suggestion = egg_suggestion_row_get_suggestion (row);
  g_signal_emit (self, signals [SUGGESTION_ACTIVATED], 0, suggestion);
}

static void
egg_suggestion_popover_list_box_row_selected (EggSuggestionPopover *self,
                                              EggSuggestionRow     *row,
                                              GtkListBox           *list_box)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (!row || EGG_IS_SUGGESTION_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED]);
}

static void
egg_suggestion_popover_destroy (GtkWidget *widget)
{
  EggSuggestionPopover *self = (EggSuggestionPopover *)widget;

  if (self->transient_for != NULL)
    {
      g_signal_handler_disconnect (self->transient_for, self->size_allocate_handler);
      g_signal_handler_disconnect (self->transient_for, self->configure_event_handler);
      g_signal_handler_disconnect (self->transient_for, self->delete_event_handler);

      self->size_allocate_handler = 0;
      self->configure_event_handler = 0;
      self->delete_event_handler = 0;

      self->transient_for = NULL;
    }

  g_clear_object (&self->model);

  egg_suggestion_popover_set_relative_to (self, NULL);

  GTK_WIDGET_CLASS (egg_suggestion_popover_parent_class)->destroy (widget);
}

static void
egg_suggestion_popover_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  EggSuggestionPopover *self = EGG_SUGGESTION_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, egg_suggestion_popover_get_model (self));
      break;

    case PROP_RELATIVE_TO:
      g_value_set_object (value, egg_suggestion_popover_get_relative_to (self));
      break;

    case PROP_SELECTED:
      g_value_set_object (value, egg_suggestion_popover_get_selected (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_suggestion_popover_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  EggSuggestionPopover *self = EGG_SUGGESTION_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      egg_suggestion_popover_set_model (self, g_value_get_object (value));
      break;

    case PROP_RELATIVE_TO:
      egg_suggestion_popover_set_relative_to (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      egg_suggestion_popover_set_selected (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
egg_suggestion_popover_class_init (EggSuggestionPopoverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = egg_suggestion_popover_get_property;
  object_class->set_property = egg_suggestion_popover_set_property;

  widget_class->destroy = egg_suggestion_popover_destroy;
  widget_class->hide = egg_suggestion_popover_hide;
  widget_class->screen_changed = egg_suggestion_popover_screen_changed;
  widget_class->realize = egg_suggestion_popover_realize;
  widget_class->show = egg_suggestion_popover_show;

  properties [PROP_MODEL] =
    g_param_spec_object ("model",
                         "Model",
                         "The model to be visualized",
                         EGG_TYPE_SUGGESTION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RELATIVE_TO] =
    g_param_spec_object ("relative-to",
                         "Relative To",
                         "The widget to be relative to",
                         GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SELECTED] =
    g_param_spec_object ("selected",
                         "Selected",
                         "The selected suggestion",
                         EGG_TYPE_SUGGESTION,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [SUGGESTION_ACTIVATED] =
    g_signal_new ("suggestion-activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, EGG_TYPE_SUGGESTION);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libegg-private/egg-suggestion-popover.ui");
  gtk_widget_class_bind_template_child (widget_class, EggSuggestionPopover, revealer);
  gtk_widget_class_bind_template_child (widget_class, EggSuggestionPopover, list_box);
  gtk_widget_class_bind_template_child (widget_class, EggSuggestionPopover, scrolled_window);

  gtk_widget_class_set_css_name (widget_class, "suggestionpopover");

  g_type_ensure (EGG_TYPE_ELASTIC_BIN);
}

static void
egg_suggestion_popover_init (EggSuggestionPopover *self)
{
  self->row_type = EGG_TYPE_SUGGESTION_ROW;

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_COMBO);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  g_signal_connect_object (self->revealer,
                           "notify::child-revealed",
                           G_CALLBACK (egg_suggestion_popover_notify_child_revealed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-activated",
                           G_CALLBACK (egg_suggestion_popover_list_box_row_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->list_box,
                           "row-selected",
                           G_CALLBACK (egg_suggestion_popover_list_box_row_selected),
                           self,
                           G_CONNECT_SWAPPED);
}

GtkWidget *
egg_suggestion_popover_new (void)
{
  return g_object_new (EGG_TYPE_SUGGESTION_POPOVER, NULL);
}

void
egg_suggestion_popover_popup (EggSuggestionPopover *self)
{
  guint duration = 250;
  guint n_items;

  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->model == NULL || 0 == (n_items = g_list_model_get_n_items (self->model)))
    return;

  if (self->relative_to != NULL)
    {
      GdkDisplay *display;
      GdkMonitor *monitor;
      GdkWindow *window;
      GtkAllocation alloc;
      gint min_height;
      gint nat_height;

      display = gtk_widget_get_display (GTK_WIDGET (self->relative_to));
      window = gtk_widget_get_window (GTK_WIDGET (self->relative_to));
      monitor = gdk_display_get_monitor_at_window (display, window);

      gtk_widget_get_preferred_height (GTK_WIDGET (self), &min_height, &nat_height);
      gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

      duration = egg_animation_calculate_duration (monitor, alloc.height, nat_height);
    }

  gtk_widget_show (GTK_WIDGET (self));

  gtk_revealer_set_transition_duration (self->revealer, duration);
  gtk_revealer_set_reveal_child (self->revealer, TRUE);
}

void
egg_suggestion_popover_popdown (EggSuggestionPopover *self)
{
  GtkAllocation alloc;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkWindow *window;
  guint duration;

  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  display = gtk_widget_get_display (GTK_WIDGET (self->relative_to));
  window = gtk_widget_get_window (GTK_WIDGET (self->relative_to));
  monitor = gdk_display_get_monitor_at_window (display, window);

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  duration = egg_animation_calculate_duration (monitor, alloc.height, 0);

  gtk_revealer_set_transition_duration (self->revealer, duration);
  gtk_revealer_set_reveal_child (self->revealer, FALSE);
}

static GtkWidget *
egg_suggestion_popover_create_row (gpointer item,
                                   gpointer user_data)
{
  EggSuggestionPopover *self = user_data;
  EggSuggestionRow *row;
  EggSuggestion *suggestion = item;

  g_assert (EGG_IS_SUGGESTION (suggestion));
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  row = g_object_new (self->row_type,
                      "suggestion", suggestion,
                      "visible", TRUE,
                      NULL);

  return GTK_WIDGET (row);
}

static void
egg_suggestion_popover_items_changed (EggSuggestionPopover *self,
                                      guint                 position,
                                      guint                 removed,
                                      guint                 added,
                                      GListModel           *model)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));
  g_assert (G_IS_LIST_MODEL (model));

  if (g_list_model_get_n_items (model) == 0)
    {
      egg_suggestion_popover_popdown (self);
      return;
    }

  /*
   * If we are currently animating in the initial view of the popover,
   * then we might need to cancel that animation and rely on the elastic
   * bin for smooth resizing.
   */
  if (gtk_revealer_get_reveal_child (self->revealer) &&
      !gtk_revealer_get_child_revealed (self->revealer) &&
      (removed || added))
    {
      gtk_revealer_set_transition_duration (self->revealer, 0);
      gtk_revealer_set_reveal_child (self->revealer, FALSE);
      gtk_revealer_set_reveal_child (self->revealer, TRUE);
    }
}

static void
egg_suggestion_popover_connect (EggSuggestionPopover *self)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->model == NULL)
    return;

  gtk_list_box_bind_model (self->list_box,
                           self->model,
                           egg_suggestion_popover_create_row,
                           self,
                           NULL);

  self->items_changed_handler =
    g_signal_connect_object (self->model,
                             "items-changed",
                             G_CALLBACK (egg_suggestion_popover_items_changed),
                             self,
                             G_CONNECT_SWAPPED);

  if (g_list_model_get_n_items (self->model) == 0)
    {
      egg_suggestion_popover_popdown (self);
      return;
    }

  /* select the first row */
  egg_suggestion_popover_move_by (self, 1);
}

static void
egg_suggestion_popover_disconnect (EggSuggestionPopover *self)
{
  g_assert (EGG_IS_SUGGESTION_POPOVER (self));

  if (self->model == NULL)
    return;

  g_signal_handler_disconnect (self->model, self->items_changed_handler);
  self->items_changed_handler = 0;

  gtk_list_box_bind_model (self->list_box, NULL, NULL, NULL, NULL);
}

void
egg_suggestion_popover_set_model (EggSuggestionPopover *self,
                                  GListModel           *model)
{
  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));
  g_return_if_fail (!model || G_IS_LIST_MODEL (model));
  g_return_if_fail (!model || g_type_is_a (g_list_model_get_item_type (model), EGG_TYPE_SUGGESTION));

  if (self->model != model)
    {
      if (self->model != NULL)
        {
          egg_suggestion_popover_disconnect (self);
          g_clear_object (&self->model);
        }

      if (model != NULL)
        {
          self->model = g_object_ref (model);
          egg_suggestion_popover_connect (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
    }
}

/**
 * egg_suggestion_popover_get_model:
 * @self: a #EggSuggestionPopover
 *
 * Gets the model being visualized.
 *
 * Returns: (nullable) (transfer none): A #GListModel or %NULL.
 */
GListModel *
egg_suggestion_popover_get_model (EggSuggestionPopover *self)
{
  g_return_val_if_fail (EGG_IS_SUGGESTION_POPOVER (self), NULL);

  return self->model;
}

static void
find_index_of_row (GtkWidget *widget,
                   gpointer   user_data)
{
  struct {
    GtkWidget *row;
    gint       index;
    gint       counter;
  } *row_lookup = user_data;

  if (widget == row_lookup->row)
    row_lookup->index = row_lookup->counter;

  row_lookup->counter++;
}

void
egg_suggestion_popover_move_by (EggSuggestionPopover *self,
                                gint                  amount)
{
  GtkListBoxRow *row;
  struct {
    GtkWidget *row;
    gint       index;
    gint       counter;
  } row_lookup;

  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));

  if (NULL == (row = gtk_list_box_get_row_at_index (self->list_box, 0)))
    return;

  if (NULL == gtk_list_box_get_selected_row (self->list_box))
    {
      gtk_list_box_select_row (self->list_box, row);
      return;
    }

  /*
   * It would be nice if we have a bit better API to have control over
   * this from GtkListBox. move-cursor isn't really sufficient for our
   * control over position without updating the focus.
   *
   * We could look at doing focus redirection to the popover first,
   * but that isn't exactly a clean solution either and suggests that
   * we subclass the listbox too.
   *
   * This is really inefficient, but in general we won't have that
   * many results, becuase showin the user a ton of results is not
   * exactly useful.
   *
   * If we do decide to reuse this class in something autocompletion
   * in a text editor, we'll want to restrategize (including avoiding
   * the creation of unnecessary rows and row reuse).
   */
  row = gtk_list_box_get_selected_row (self->list_box);

  row_lookup.row = GTK_WIDGET (row);
  row_lookup.counter = 0;
  row_lookup.index = -1;

  gtk_container_foreach (GTK_CONTAINER (self->list_box),
                         find_index_of_row,
                         &row_lookup);

  row_lookup.index += amount;
  row_lookup.index = CLAMP (row_lookup.index, -0, (gint)g_list_model_get_n_items (self->model) - 1);

  row = gtk_list_box_get_row_at_index (self->list_box, row_lookup.index);
  gtk_list_box_select_row (self->list_box, row);
}

static void
find_suggestion_row (GtkWidget *widget,
                     gpointer   user_data)
{
  EggSuggestionRow *row = EGG_SUGGESTION_ROW (widget);
  EggSuggestion *suggestion = egg_suggestion_row_get_suggestion (row);
  struct {
    EggSuggestion  *suggestion;
    GtkListBoxRow **row;
  } *lookup = user_data;

  if (suggestion == lookup->suggestion)
    *lookup->row = GTK_LIST_BOX_ROW (row);
}

void
egg_suggestion_popover_set_selected (EggSuggestionPopover *self,
                                     EggSuggestion        *suggestion)
{
  GtkListBoxRow *row = NULL;
  struct {
    EggSuggestion  *suggestion;
    GtkListBoxRow **row;
  } lookup = { suggestion, &row };

  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));
  g_return_if_fail (!suggestion || EGG_IS_SUGGESTION (suggestion));

  if (suggestion == NULL)
    row = gtk_list_box_get_row_at_index (self->list_box, 0);
  else
    gtk_container_foreach (GTK_CONTAINER (self->list_box), find_suggestion_row, &lookup);

  if (row != NULL)
    gtk_list_box_select_row (self->list_box, row);
}

/**
 * egg_suggestion_popover_get_selected:
 * @self: a #EggSuggestionPopover
 *
 * Gets the currently selected suggestion.
 *
 * Returns: (transfer none) (nullable): An #EggSuggestion or %NULL.
 */
EggSuggestion *
egg_suggestion_popover_get_selected (EggSuggestionPopover *self)
{
  EggSuggestionRow *row;

  g_return_val_if_fail (EGG_IS_SUGGESTION_POPOVER (self), NULL);

  row = EGG_SUGGESTION_ROW (gtk_list_box_get_selected_row (self->list_box));
  if (row != NULL)
    return egg_suggestion_row_get_suggestion (row);

  return NULL;
}

void
egg_suggestion_popover_activate_selected (EggSuggestionPopover *self)
{
  EggSuggestion *suggestion;

  g_return_if_fail (EGG_IS_SUGGESTION_POPOVER (self));

  if (NULL != (suggestion = egg_suggestion_popover_get_selected (self)))
    g_signal_emit (self, signals [SUGGESTION_ACTIVATED], 0, suggestion);
}
