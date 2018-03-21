/* ide-layout-stack.c
 *
 * Copyright © 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-layout-stack"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>

#include "ide-debug.h"

#include "layout/ide-layout-stack.h"
#include "layout/ide-layout-stack-addin.h"
#include "layout/ide-layout-stack-header.h"
#include "layout/ide-layout-private.h"
#include "layout/ide-shortcut-label.h"
#include "threading/ide-task.h"

#define TRANSITION_DURATION 300
#define DISTANCE_THRESHOLD(alloc) (MIN(250, (gint)((alloc)->width * .333)))

/**
 * SECTION:ide-layout-stack
 * @title: IdeLayoutStack
 * @short_description: A stack of #IdeLayoutView
 *
 * This widget is used to represent a stack of #IdeLayoutView widgets.  it
 * includes an #IdeLayoutStackHeader at the top, and then a stack of views
 * below.
 *
 * If there are no #IdeLayoutView visibile, then an empty state widget is
 * displayed with some common information for the user.
 *
 * To simplify integration with other systems, #IdeLayoutStack implements
 * the #GListModel interface for each of the #IdeLayoutView.
 */

typedef struct
{
  DzlBindingGroup      *bindings;
  DzlSignalGroup       *signals;
  GPtrArray            *views;
  GPtrArray            *in_transition;
  PeasExtensionSet     *addins;

  /*
   * Our gestures are used to do interactive moves when the user
   * does a three finger swipe. We create the dummy gesture to
   * ensure things work, because it for some reason does not without
   * the dummy gesture set.
   *
   * https://bugzilla.gnome.org/show_bug.cgi?id=788914
   */
  GtkGesture           *dummy;
  GtkGesture           *pan;
  DzlBoxTheatric       *pan_theatric;
  IdeLayoutView        *pan_view;

  /* Template references */
  DzlBox               *empty_state;
  DzlEmptyState        *failed_state;
  IdeLayoutStackHeader *header;
  GtkStack             *stack;
  GtkStack             *top_stack;
  GtkEventBox          *event_box;
} IdeLayoutStackPrivate;

typedef struct
{
  IdeLayoutStack *source;
  IdeLayoutStack *dest;
  IdeLayoutView  *view;
  DzlBoxTheatric *theatric;
} AnimationState;

enum {
  PROP_0,
  PROP_HAS_VIEW,
  PROP_VISIBLE_CHILD,
  N_PROPS
};

enum {
  CHNAGE_CURRENT_PAGE,
  N_SIGNALS
};

static void list_model_iface_init    (GListModelInterface *iface);
static void animation_state_complete (gpointer             data);

G_DEFINE_TYPE_WITH_CODE (IdeLayoutStack, ide_layout_stack, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (IdeLayoutStack)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static inline gboolean
is_uninitialized (GtkAllocation *alloc)
{
  return (alloc->x == -1 && alloc->y == -1 &&
          alloc->width == 1 && alloc->height == 1);
}

static void
ide_layout_stack_set_cursor (IdeLayoutStack *self,
                             const gchar    *name)
{
  GdkWindow *window;
  GdkDisplay *display;
  GdkCursor *cursor;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (name != NULL);

  window = gtk_widget_get_window (GTK_WIDGET (self));
  display = gtk_widget_get_display (GTK_WIDGET (self));
  cursor = gdk_cursor_new_from_name (display, name);

  gdk_window_set_cursor (window, cursor);

  g_clear_object (&cursor);
}

static void
ide_layout_stack_view_failed (IdeLayoutStack *self,
                              GParamSpec     *pspec,
                              IdeLayoutView  *view)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  if (ide_layout_view_get_failed (view))
    gtk_stack_set_visible_child (priv->top_stack, GTK_WIDGET (priv->failed_state));
  else
    gtk_stack_set_visible_child (priv->top_stack, GTK_WIDGET (priv->stack));
}

static void
ide_layout_stack_bindings_notify_source (IdeLayoutStack  *self,
                                         GParamSpec      *pspec,
                                         DzlBindingGroup *bindings)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  GObject *source;

  g_assert (DZL_IS_BINDING_GROUP (bindings));
  g_assert (pspec != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (self));

  source = dzl_binding_group_get_source (bindings);

  if (source == NULL)
    {
      _ide_layout_stack_header_set_title (priv->header, _("No Open Pages"));
      _ide_layout_stack_header_set_modified (priv->header, FALSE);
      _ide_layout_stack_header_set_background_rgba (priv->header, NULL);
      _ide_layout_stack_header_set_foreground_rgba (priv->header, NULL);
    }
}

static void
ide_layout_stack_notify_addin_of_view (PeasExtensionSet *set,
                                       PeasPluginInfo   *plugin_info,
                                       PeasExtension    *exten,
                                       gpointer          user_data)
{
  IdeLayoutStackAddin *addin = (IdeLayoutStackAddin *)exten;
  IdeLayoutView *view = user_data;

  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (addin));
  g_assert (!view || IDE_IS_LAYOUT_VIEW (view));

  ide_layout_stack_addin_set_view (addin, view);
}

static void
ide_layout_stack_notify_visible_child (IdeLayoutStack *self,
                                       GParamSpec     *pspec,
                                       GtkStack       *stack)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  GtkWidget *visible_child;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_STACK (stack));

  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    return;

  visible_child = gtk_stack_get_visible_child (priv->stack);

  /*
   * Mux/Proxy actions to our level so that they also be activated
   * from the header bar without any weirdness by the View.
   */
  dzl_gtk_widget_mux_action_groups (GTK_WIDGET (self), visible_child,
                                    "IDE_LAYOUT_STACK_MUXED_ACTION");

  /* Update our bindings targets */
  dzl_binding_group_set_source (priv->bindings, visible_child);
  dzl_signal_group_set_target (priv->signals, visible_child);

  /* Show either the empty state, failed state, or actual view */
  if (visible_child != NULL &&
      ide_layout_view_get_failed (IDE_LAYOUT_VIEW (visible_child)))
    gtk_stack_set_visible_child (priv->top_stack, GTK_WIDGET (priv->failed_state));
  else if (visible_child != NULL)
    gtk_stack_set_visible_child (priv->top_stack, GTK_WIDGET (priv->stack));
  else
    gtk_stack_set_visible_child (priv->top_stack, GTK_WIDGET (priv->empty_state));

  /* Allow the header to update settings */
  _ide_layout_stack_header_update (priv->header, IDE_LAYOUT_VIEW (visible_child));

  /* Ensure action state is up to date */
  _ide_layout_stack_update_actions (self);

  peas_extension_set_foreach (priv->addins,
                              ide_layout_stack_notify_addin_of_view,
                              visible_child);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VISIBLE_CHILD]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_HAS_VIEW]);
}

static void
collect_widgets (GtkWidget *widget,
                 gpointer   user_data)
{
  g_ptr_array_add (user_data, widget);
}

static void
ide_layout_stack_change_current_page (IdeLayoutStack *self,
                                      gint            direction)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  g_autoptr(GPtrArray) ar = NULL;
  GtkWidget *visible_child;
  gint position = 0;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  visible_child = gtk_stack_get_visible_child (priv->stack);

  if (visible_child == NULL)
    return;

  gtk_container_child_get (GTK_CONTAINER (priv->stack), visible_child,
                           "position", &position,
                           NULL);

  ar = g_ptr_array_new ();
  gtk_container_foreach (GTK_CONTAINER (priv->stack), collect_widgets, ar);
  if (ar->len == 0)
    g_return_if_reached ();

  visible_child = g_ptr_array_index (ar, (position + direction) % ar->len);
  gtk_stack_set_visible_child (priv->stack, visible_child);
}

static void
ide_layout_stack_add (GtkContainer *container,
                      GtkWidget    *widget)
{
  IdeLayoutStack *self = (IdeLayoutStack *)container;
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (IDE_IS_LAYOUT_VIEW (widget))
    gtk_container_add (GTK_CONTAINER (priv->stack), widget);
  else
    GTK_CONTAINER_CLASS (ide_layout_stack_parent_class)->add (container, widget);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

static void
ide_layout_stack_view_added (IdeLayoutStack *self,
                             IdeLayoutView  *view)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  guint position;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  /*
   * Make sure that the header has dismissed all of the popovers immediately.
   * We don't want them lingering while we do other UI work which might want to
   * grab focus, etc.
   */
  _ide_layout_stack_header_popdown (priv->header);

  /* Notify GListModel consumers of the new view and it's position within
   * our stack of view widgets.
   */
  position = priv->views->len;
  g_ptr_array_add (priv->views, view);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  /*
   * Now ensure that the view is displayed and focus the widget so the
   * user can immediately start typing.
   */
  ide_layout_stack_set_visible_child (self, view);
  gtk_widget_grab_focus (GTK_WIDGET (view));
}

static void
ide_layout_stack_view_removed (IdeLayoutStack *self,
                               IdeLayoutView  *view)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_LAYOUT_VIEW (view));

  if (priv->views != NULL)
    {
      guint position = 0;

      /* If this is the last view, hide the popdown now.  We use our hide
       * variant instead of popdown so that we don't have jittery animations.
       */
      if (priv->views->len == 1)
        _ide_layout_stack_header_hide (priv->header);

      /*
       * Only remove the view if it is not in transition. We hold onto the
       * view during the transition so that we keep the list stable.
       */
      if (!g_ptr_array_find_with_equal_func (priv->in_transition, view, NULL, &position))
        {
          for (guint i = 0; i < priv->views->len; i++)
            {
              if ((gpointer)view == g_ptr_array_index (priv->views, i))
                {
                  g_ptr_array_remove_index (priv->views, i);
                  g_list_model_items_changed (G_LIST_MODEL (self), i, 1, 0);
                }
            }
        }
    }
}

static void
ide_layout_stack_real_agree_to_close_async (IdeLayoutStack      *self,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_layout_stack_real_agree_to_close_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_return_boolean (task, TRUE);
}

static gboolean
ide_layout_stack_real_agree_to_close_finish (IdeLayoutStack *self,
                                             GAsyncResult   *result,
                                             GError        **error)
{
  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
ide_layout_stack_addin_added (PeasExtensionSet *set,
                              PeasPluginInfo   *plugin_info,
                              PeasExtension    *exten,
                              gpointer          user_data)
{
  IdeLayoutStackAddin *addin = (IdeLayoutStackAddin *)exten;
  IdeLayoutStack *self = user_data;
  IdeLayoutView *visible_child;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (addin));

  ide_layout_stack_addin_load (addin, self);

  visible_child = ide_layout_stack_get_visible_child (self);

  if (visible_child != NULL)
    ide_layout_stack_addin_set_view (addin, visible_child);
}

static void
ide_layout_stack_addin_removed (PeasExtensionSet *set,
                                PeasPluginInfo   *plugin_info,
                                PeasExtension    *exten,
                                gpointer          user_data)
{
  IdeLayoutStackAddin *addin = (IdeLayoutStackAddin *)exten;
  IdeLayoutStack *self = user_data;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (PEAS_IS_EXTENSION_SET (set));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_LAYOUT_STACK_ADDIN (addin));

  ide_layout_stack_addin_unload (addin, self);
}

static gboolean
ide_layout_stack_pan_begin (IdeLayoutStack   *self,
                            GdkEventSequence *sequence,
                            GtkGesturePan    *gesture)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  GtkAllocation alloc;
  cairo_surface_t *surface = NULL;
  IdeLayoutView *view;
  GdkWindow *window;
  GtkWidget *grid;
  cairo_t *cr;
  gdouble x, y;
  gboolean enable_animations;

  IDE_ENTRY;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_GESTURE_PAN (gesture));
  g_assert (priv->pan_theatric == NULL);

  view = ide_layout_stack_get_visible_child (self);
  if (view != NULL)
    gtk_widget_get_allocation (GTK_WIDGET (view), &alloc);

  g_object_get (gtk_settings_get_default (),
                "gtk-enable-animations", &enable_animations,
                NULL);

  if (sequence != NULL ||
      view == NULL ||
      !enable_animations ||
      is_uninitialized (&alloc) ||
      NULL == (window = gtk_widget_get_window (GTK_WIDGET (view))) ||
      NULL == (surface = gdk_window_create_similar_surface (window,
                                                            CAIRO_CONTENT_COLOR,
                                                            alloc.width,
                                                            alloc.height)))
    {
      if (sequence != NULL)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      IDE_RETURN (FALSE);
    }

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (gesture), &x, &y);

  cr = cairo_create (surface);
  gtk_widget_draw (GTK_WIDGET (view), cr);
  cairo_destroy (cr);

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);
  gtk_widget_translate_coordinates (GTK_WIDGET (priv->top_stack), grid, 0, 0,
                                    &alloc.x, &alloc.y);

  priv->pan_view = g_object_ref (view);
  priv->pan_theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                                     "surface", surface,
                                     "target", grid,
                                     "x", alloc.x + (gint)x,
                                     "y", alloc.y,
                                     "width", alloc.width,
                                     "height", alloc.height,
                                     NULL);

  g_clear_pointer (&surface, cairo_surface_destroy);

  /* Hide the view while we begin the possible transition to another
   * layout stack.
   */
  gtk_widget_hide (GTK_WIDGET (priv->pan_view));

  /*
   * Hide the mouse cursor until ide_layout_stack_pan_end() is called.
   * It can be distracting otherwise (and we want to warp it to the new
   * grid column too).
   */
  ide_layout_stack_set_cursor (self, "none");

  IDE_RETURN (TRUE);
}

static void
ide_layout_stack_pan_update (IdeLayoutStack   *self,
                             GdkEventSequence *sequence,
                             GtkGestureSwipe  *gesture)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  GtkAllocation alloc;
  GtkWidget *grid;
  gdouble x, y;

  IDE_ENTRY;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_GESTURE_PAN (gesture));
  g_assert (!priv->pan_theatric || DZL_IS_BOX_THEATRIC (priv->pan_theatric));

  if (priv->pan_theatric == NULL)
    {
      if (sequence != NULL)
        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      IDE_EXIT;
    }

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (gesture), &x, &y);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);
  gtk_widget_translate_coordinates (GTK_WIDGET (priv->top_stack), grid, 0, 0,
                                    &alloc.x, &alloc.y);

  g_object_set (priv->pan_theatric,
                "x", alloc.x + (gint)x,
                NULL);

  IDE_EXIT;
}

static void
ide_layout_stack_pan_end (IdeLayoutStack   *self,
                          GdkEventSequence *sequence,
                          GtkGesturePan    *gesture)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  IdeLayoutStackPrivate *dest_priv;
  IdeLayoutStack *dest;
  GtkAllocation alloc;
  GtkWidget *grid;
  GtkWidget *column;
  gdouble x, y;
  gint direction;
  gint index = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (GTK_IS_GESTURE_PAN (gesture));

  if (priv->pan_theatric == NULL || priv->pan_view == NULL)
    IDE_GOTO (cleanup);

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (gesture), &x, &y);

  if (x > DISTANCE_THRESHOLD (&alloc))
    direction = 1;
  else if (x < -DISTANCE_THRESHOLD (&alloc))
    direction = -1;
  else
    direction = 0;

  grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);
  g_assert (grid != NULL);
  g_assert (IDE_IS_LAYOUT_GRID (grid));

  column = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID_COLUMN);
  g_assert (column != NULL);
  g_assert (IDE_IS_LAYOUT_GRID_COLUMN (column));

  gtk_container_child_get (GTK_CONTAINER (grid), GTK_WIDGET (column),
                           "index", &index,
                           NULL);

  dest = _ide_layout_grid_get_nth_stack (IDE_LAYOUT_GRID (grid), index + direction);
  dest_priv = ide_layout_stack_get_instance_private (dest);
  g_assert (dest != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (dest));

  gtk_widget_get_allocation (GTK_WIDGET (dest), &alloc);

  if (!is_uninitialized (&alloc))
    {
      AnimationState *state;

      state = g_slice_new0 (AnimationState);
      state->source = g_object_ref (self);
      state->dest = g_object_ref (dest);
      state->view = g_object_ref (priv->pan_view);
      state->theatric = priv->pan_theatric;

      gtk_widget_translate_coordinates (GTK_WIDGET (dest_priv->top_stack), grid, 0, 0,
                                        &alloc.x, &alloc.y);

      /*
       * Use EASE_OUT_CUBIC, because user initiated the beginning of the
       * acceleration curve just by swiping. No need to duplicate.
       */
      dzl_object_animate_full (state->theatric,
                               DZL_ANIMATION_EASE_OUT_CUBIC,
                               TRANSITION_DURATION,
                               gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                               animation_state_complete,
                               state,
                               "x", alloc.x,
                               "width", alloc.width,
                               NULL);

      if (dest != self)
        {
          g_ptr_array_add (priv->in_transition, g_object_ref (priv->pan_view));
          gtk_container_remove (GTK_CONTAINER (priv->stack), GTK_WIDGET (priv->pan_view));
        }

      IDE_TRACE_MSG ("Animating transition to %s column",
                     dest != self ? "another" : "same");
    }
  else
    {
      g_autoptr(IdeLayoutView) view = g_object_ref (priv->pan_view);

      IDE_TRACE_MSG ("Moving view to a previously non-existant column");

      gtk_container_remove (GTK_CONTAINER (priv->stack), GTK_WIDGET (priv->pan_view));
      gtk_widget_show (GTK_WIDGET (priv->pan_view));
      gtk_container_add (GTK_CONTAINER (dest_priv->stack), GTK_WIDGET (priv->pan_view));
    }

cleanup:
  g_clear_object (&priv->pan_theatric);
  g_clear_object (&priv->pan_view);

  gtk_widget_queue_draw (gtk_widget_get_toplevel (GTK_WIDGET (self)));

  ide_layout_stack_set_cursor (self, "arrow");

  IDE_EXIT;
}

static void
ide_layout_stack_constructed (GObject *object)
{
  IdeLayoutStack *self = (IdeLayoutStack *)object;
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));

  G_OBJECT_CLASS (ide_layout_stack_parent_class)->constructed (object);

  priv->addins = peas_extension_set_new (peas_engine_get_default (),
                                         IDE_TYPE_LAYOUT_STACK_ADDIN,
                                         NULL);

  g_signal_connect (priv->addins,
                    "extension-added",
                    G_CALLBACK (ide_layout_stack_addin_added),
                    self);

  g_signal_connect (priv->addins,
                    "extension-removed",
                    G_CALLBACK (ide_layout_stack_addin_removed),
                    self);

  peas_extension_set_foreach (priv->addins,
                              ide_layout_stack_addin_added,
                              self);

  gtk_widget_add_events (GTK_WIDGET (priv->event_box), GDK_TOUCH_MASK);
  priv->pan = g_object_new (GTK_TYPE_GESTURE_PAN,
                            "widget", priv->event_box,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "n-points", 3,
                            NULL);
  g_signal_connect_swapped (priv->pan,
                            "begin",
                            G_CALLBACK (ide_layout_stack_pan_begin),
                            self);
  g_signal_connect_swapped (priv->pan,
                            "update",
                            G_CALLBACK (ide_layout_stack_pan_update),
                            self);
  g_signal_connect_swapped (priv->pan,
                            "end",
                            G_CALLBACK (ide_layout_stack_pan_end),
                            self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->pan),
                                              GTK_PHASE_BUBBLE);

  /*
   * FIXME: Our priv->pan gesture does not activate unless we add another
   *        dummy gesture. I currently have no idea why that is.
   *
   *        https://bugzilla.gnome.org/show_bug.cgi?id=788914
   */
  priv->dummy = gtk_gesture_rotate_new (GTK_WIDGET (priv->event_box));
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (priv->dummy),
                                              GTK_PHASE_BUBBLE);
}

static void
ide_layout_stack_grab_focus (GtkWidget *widget)
{
  IdeLayoutStack *self = (IdeLayoutStack *)widget;
  IdeLayoutView *child;

  g_assert (IDE_IS_LAYOUT_STACK (self));

  child = ide_layout_stack_get_visible_child (self);

  if (child != NULL)
    gtk_widget_grab_focus (GTK_WIDGET (child));
  else
    GTK_WIDGET_CLASS (ide_layout_stack_parent_class)->grab_focus (widget);
}

static void
ide_layout_stack_destroy (GtkWidget *widget)
{
  IdeLayoutStack *self = (IdeLayoutStack *)widget;
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));

  g_clear_pointer (&priv->in_transition, g_ptr_array_unref);

  if (priv->views != NULL)
    {
      g_list_model_items_changed (G_LIST_MODEL (self), 0, priv->views->len, 0);
      g_clear_pointer (&priv->views, g_ptr_array_unref);
    }

  g_clear_object (&priv->addins);

  if (priv->bindings != NULL)
    {
      dzl_binding_group_set_source (priv->bindings, NULL);
      g_clear_object (&priv->bindings);
    }

  if (priv->signals != NULL)
    {
      dzl_signal_group_set_target (priv->signals, NULL);
      g_clear_object (&priv->signals);
    }

  g_clear_object (&priv->pan);

  GTK_WIDGET_CLASS (ide_layout_stack_parent_class)->destroy (widget);
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
    case PROP_HAS_VIEW:
      g_value_set_boolean (value, ide_layout_stack_get_has_view (self));
      break;

    case PROP_VISIBLE_CHILD:
      g_value_set_object (value, ide_layout_stack_get_visible_child (self));
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
    case PROP_VISIBLE_CHILD:
      ide_layout_stack_set_visible_child (self, g_value_get_object (value));
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
  object_class->get_property = ide_layout_stack_get_property;
  object_class->set_property = ide_layout_stack_set_property;

  widget_class->destroy = ide_layout_stack_destroy;
  widget_class->grab_focus = ide_layout_stack_grab_focus;

  container_class->add = ide_layout_stack_add;

  klass->agree_to_close_async = ide_layout_stack_real_agree_to_close_async;
  klass->agree_to_close_finish = ide_layout_stack_real_agree_to_close_finish;

  properties [PROP_HAS_VIEW] =
    g_param_spec_boolean ("has-view", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_VISIBLE_CHILD] =
    g_param_spec_object ("visible-child",
                         "Visible Child",
                         "The current view to be displayed",
                         IDE_TYPE_LAYOUT_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CHNAGE_CURRENT_PAGE] =
    g_signal_new_class_handler ("change-current-page",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_layout_stack_change_current_page),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__INT,
                                G_TYPE_NONE, 1, G_TYPE_INT);

  gtk_widget_class_set_css_name (widget_class, "idelayoutstack");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-layout-stack.ui");
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, empty_state);
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, failed_state);
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, header);
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, stack);
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, top_stack);
  gtk_widget_class_bind_template_child_private (widget_class, IdeLayoutStack, event_box);

  g_type_ensure (IDE_TYPE_LAYOUT_STACK_HEADER);
  g_type_ensure (IDE_TYPE_SHORTCUT_LABEL);
}

static void
ide_layout_stack_init (IdeLayoutStack *self)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  _ide_layout_stack_init_actions (self);
  _ide_layout_stack_init_shortcuts (self);

  priv->views = g_ptr_array_new ();
  priv->in_transition = g_ptr_array_new_with_free_func (g_object_unref);

  priv->signals = dzl_signal_group_new (IDE_TYPE_LAYOUT_VIEW);

  dzl_signal_group_connect_swapped (priv->signals,
                                    "notify::failed",
                                    G_CALLBACK (ide_layout_stack_view_failed),
                                    self);

  priv->bindings = dzl_binding_group_new ();

  g_signal_connect_object (priv->bindings,
                           "notify::source",
                           G_CALLBACK (ide_layout_stack_bindings_notify_source),
                           self,
                           G_CONNECT_SWAPPED);

  dzl_binding_group_bind (priv->bindings, "title",
                          priv->header, "title",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (priv->bindings, "modified",
                          priv->header, "modified",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (priv->bindings, "primary-color-bg",
                          priv->header, "background-rgba",
                          G_BINDING_SYNC_CREATE);

  dzl_binding_group_bind (priv->bindings, "primary-color-fg",
                          priv->header, "foreground-rgba",
                          G_BINDING_SYNC_CREATE);

  g_signal_connect_object (priv->stack,
                           "notify::visible-child",
                           G_CALLBACK (ide_layout_stack_notify_visible_child),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->stack,
                           "add",
                           G_CALLBACK (ide_layout_stack_view_added),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_signal_connect_object (priv->stack,
                           "remove",
                           G_CALLBACK (ide_layout_stack_view_removed),
                           self,
                           G_CONNECT_SWAPPED);

  _ide_layout_stack_header_set_views (priv->header, G_LIST_MODEL (self));
  _ide_layout_stack_header_update (priv->header, NULL);
}

GtkWidget *
ide_layout_stack_new (void)
{
  return g_object_new (IDE_TYPE_LAYOUT_STACK, NULL);
}

/**
 * ide_layout_stack_set_visible_child:
 * @self: a #IdeLayoutStack
 *
 * Sets the current view for the stack.
 *
 * Since: 3.26
 */
void
ide_layout_stack_set_visible_child (IdeLayoutStack *self,
                                    IdeLayoutView  *view)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (view)) == (GtkWidget *)priv->stack);

  gtk_stack_set_visible_child (priv->stack, GTK_WIDGET (view));
}

/**
 * ide_layout_stack_get_visible_child:
 * @self: a #IdeLayoutStack
 *
 * Gets the visible #IdeLayoutView if there is one; otherwise %NULL.
 *
 * Returns: (nullable) (transfer none): An #IdeLayoutView or %NULL
 *
 * Since: 3.26
 */
IdeLayoutView *
ide_layout_stack_get_visible_child (IdeLayoutStack *self)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), NULL);

  return IDE_LAYOUT_VIEW (gtk_stack_get_visible_child (priv->stack));
}

/**
 * ide_layout_stack_get_titlebar:
 * @self: a #IdeLayoutStack
 *
 * Gets the #IdeLayoutStackHeader header that is at the top of the stack.
 *
 * Returns: (transfer none) (type Ide.LayoutStackHeader): The layout stack header.
 *
 * Since: 3.26
 */
GtkWidget *
ide_layout_stack_get_titlebar (IdeLayoutStack *self)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), NULL);

  return GTK_WIDGET (priv->header);
}

/**
 * ide_layout_stack_get_has_view:
 * @self: an #IdeLayoutStack
 *
 * Gets the "has-view" property.
 *
 * This property is a convenience to allow widgets to easily bind
 * properties based on whether or not a view is visible in the stack.
 *
 * Returns: %TRUE if the stack has a view
 *
 * Since: 3.26
 */
gboolean
ide_layout_stack_get_has_view (IdeLayoutStack *self)
{
  IdeLayoutView *visible_child;

  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), FALSE);

  visible_child = ide_layout_stack_get_visible_child (self);

  return visible_child != NULL;
}

static void
ide_layout_stack_close_view_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeLayoutView *view = (IdeLayoutView *)object;
  g_autoptr(IdeLayoutStack) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkWidget *toplevel;
  GtkWidget *focus;
  gboolean had_focus = FALSE;

  g_assert (IDE_IS_LAYOUT_VIEW (view));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LAYOUT_STACK (self));

  if (!ide_layout_view_agree_to_close_finish (view, result, &error))
    {
      g_message ("%s", error->message);
      return;
    }

  /* Keep track of whether or not the widget had focus (which
   * would happen if we were activated from a keybinding.
   */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
  if (GTK_IS_WINDOW (toplevel) &&
      NULL != (focus = gtk_window_get_focus (GTK_WINDOW (toplevel))) &&
      (focus == GTK_WIDGET (view) ||
       gtk_widget_is_ancestor (focus, GTK_WIDGET (view))))
    had_focus = TRUE;

  /* Now we can destroy the child */
  gtk_widget_destroy (GTK_WIDGET (view));

  /* We don't want to leave the widget focus in an indeterminate
   * state so we immediately focus the next child in the stack.
   * But only do so if we had focus previously.
   */
  if (had_focus)
    {
      IdeLayoutView *visible_child = ide_layout_stack_get_visible_child (self);

      if (visible_child != NULL)
        gtk_widget_grab_focus (GTK_WIDGET (visible_child));
    }
}

void
_ide_layout_stack_request_close (IdeLayoutStack *self,
                                 IdeLayoutView  *view)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));

  ide_layout_view_agree_to_close_async (view,
                                        NULL,
                                        ide_layout_stack_close_view_cb,
                                        g_object_ref (self));
}

static GType
ide_layout_stack_get_item_type (GListModel *model)
{
  return IDE_TYPE_LAYOUT_VIEW;
}

static guint
ide_layout_stack_get_n_items (GListModel *model)
{
  IdeLayoutStack *self = (IdeLayoutStack *)model;
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));

  return priv->views ? priv->views->len : 0;
}

static gpointer
ide_layout_stack_get_item (GListModel *model,
                           guint       position)
{
  IdeLayoutStack *self = (IdeLayoutStack *)model;
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_assert (IDE_IS_LAYOUT_STACK (self));
  g_assert (position < priv->views->len);

  return g_object_ref (g_ptr_array_index (priv->views, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
  iface->get_n_items = ide_layout_stack_get_n_items;
  iface->get_item = ide_layout_stack_get_item;
  iface->get_item_type = ide_layout_stack_get_item_type;
}

void
ide_layout_stack_agree_to_close_async (IdeLayoutStack      *self,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_LAYOUT_STACK_GET_CLASS (self)->agree_to_close_async (self, cancellable, callback, user_data);
}

gboolean
ide_layout_stack_agree_to_close_finish (IdeLayoutStack *self,
                                        GAsyncResult   *result,
                                        GError        **error)
{
  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_LAYOUT_STACK_GET_CLASS (self)->agree_to_close_finish (self, result, error);
}

static void
animation_state_complete (gpointer data)
{
  IdeLayoutStackPrivate *priv;
  AnimationState *state = data;

  g_assert (state != NULL);
  g_assert (IDE_IS_LAYOUT_STACK (state->source));
  g_assert (IDE_IS_LAYOUT_STACK (state->dest));
  g_assert (IDE_IS_LAYOUT_VIEW (state->view));

  /* Add the widget to the new stack */
  if (state->dest != state->source)
    {
      gtk_container_add (GTK_CONTAINER (state->dest), GTK_WIDGET (state->view));

      /* Now remove it from our temporary transition. Be careful in case we were
       * destroyed in the mean time.
       */
      priv = ide_layout_stack_get_instance_private (state->source);

      if (priv->in_transition != NULL)
        {
          guint position = 0;

          if (g_ptr_array_find_with_equal_func (priv->views, state->view, NULL, &position))
            {
              g_ptr_array_remove (priv->in_transition, state->view);
              g_ptr_array_remove_index (priv->views, position);
              g_list_model_items_changed (G_LIST_MODEL (state->source), position, 1, 0);
            }
        }
    }

  /*
   * We might need to reshow the widget in cases where we are in a
   * three-finger-swipe of the view. There is also a chance that we
   * aren't the proper visible child and that needs to be restored now.
   */
  gtk_widget_show (GTK_WIDGET (state->view));
  ide_layout_stack_set_visible_child (state->dest, state->view);

  g_clear_object (&state->source);
  g_clear_object (&state->dest);
  g_clear_object (&state->view);
  g_clear_object (&state->theatric);
  g_slice_free (AnimationState, state);
}

void
_ide_layout_stack_transfer (IdeLayoutStack *self,
                            IdeLayoutStack *dest,
                            IdeLayoutView  *view)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);
  IdeLayoutStackPrivate *dest_priv = ide_layout_stack_get_instance_private (dest);
  const GdkRGBA *fg;
  const GdkRGBA *bg;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (IDE_IS_LAYOUT_STACK (dest));
  g_return_if_fail (IDE_IS_LAYOUT_VIEW (view));
  g_return_if_fail (GTK_WIDGET (priv->stack) == gtk_widget_get_parent (GTK_WIDGET (view)));

  /*
   * Inform the destination stack about our new primary colors so that it can
   * begin a transition to the new colors. We also want to do this upfront so
   * that we can reduce the amount of style invalidation caused during the
   * transitions.
   */

  fg = ide_layout_view_get_primary_color_fg (view);
  bg = ide_layout_view_get_primary_color_bg (view);
  _ide_layout_stack_header_set_foreground_rgba (dest_priv->header, fg);
  _ide_layout_stack_header_set_background_rgba (dest_priv->header, bg);

  /*
   * If both the old and the new stacks are mapped, we can animate
   * between them using a snapshot of the view. Well, we also need
   * to be sure they have a valid allocation, but that check is done
   * slightly after this because it makes things easier.
   */
  if (gtk_widget_get_mapped (GTK_WIDGET (self)) &&
      gtk_widget_get_mapped (GTK_WIDGET (dest)) &&
      gtk_widget_get_mapped (GTK_WIDGET (view)))
    {
      GtkAllocation alloc, dest_alloc;
      cairo_surface_t *surface = NULL;
      GdkWindow *window;
      GtkWidget *grid;
      gboolean enable_animations;

      grid = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_LAYOUT_GRID);

      gtk_widget_get_allocation (GTK_WIDGET (view), &alloc);
      gtk_widget_get_allocation (GTK_WIDGET (dest), &dest_alloc);

      g_object_get (gtk_settings_get_default (),
                    "gtk-enable-animations", &enable_animations,
                    NULL);

      if (enable_animations &&
          grid != NULL &&
          !is_uninitialized (&alloc) &&
          !is_uninitialized (&dest_alloc) &&
          dest_alloc.width > 0 && dest_alloc.height > 0 &&
          NULL != (window = gtk_widget_get_window (GTK_WIDGET (view))) &&
          NULL != (surface = gdk_window_create_similar_surface (window,
                                                                CAIRO_CONTENT_COLOR,
                                                                alloc.width,
                                                                alloc.height)))
        {
          DzlBoxTheatric *theatric = NULL;
          AnimationState *state;
          cairo_t *cr;

          cr = cairo_create (surface);
          gtk_widget_draw (GTK_WIDGET (view), cr);
          cairo_destroy (cr);

          gtk_widget_translate_coordinates (GTK_WIDGET (priv->stack), grid, 0, 0,
                                            &alloc.x, &alloc.y);
          gtk_widget_translate_coordinates (GTK_WIDGET (dest_priv->stack), grid, 0, 0,
                                            &dest_alloc.x, &dest_alloc.y);

          theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                                   "surface", surface,
                                   "height", alloc.height,
                                   "target", grid,
                                   "width", alloc.width,
                                   "x", alloc.x,
                                   "y", alloc.y,
                                   NULL);

          state = g_slice_new0 (AnimationState);
          state->source = g_object_ref (self);
          state->dest = g_object_ref (dest);
          state->view = g_object_ref (view);
          state->theatric = theatric;

          dzl_object_animate_full (theatric,
                                   DZL_ANIMATION_EASE_IN_OUT_CUBIC,
                                   TRANSITION_DURATION,
                                   gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                                   animation_state_complete,
                                   state,
                                   "x", dest_alloc.x,
                                   "width", dest_alloc.width,
                                   "y", dest_alloc.y,
                                   "height", dest_alloc.height,
                                   NULL);

          /*
           * Mark the view as in-transition so that when we remove it
           * we can ignore the items-changed until the animation completes.
           */
          g_ptr_array_add (priv->in_transition, g_object_ref (view));
          gtk_container_remove (GTK_CONTAINER (priv->stack), GTK_WIDGET (view));

          cairo_surface_destroy (surface);

          return;
        }
    }

  g_object_ref (view);
  gtk_container_remove (GTK_CONTAINER (priv->stack), GTK_WIDGET (view));
  gtk_container_add (GTK_CONTAINER (dest_priv->stack), GTK_WIDGET (view));
  g_object_unref (view);
}

/**
 * ide_layout_stack_foreach_view:
 * @self: a #IdeLayoutStack
 * @callback: (scope call) (closure user_data): A callback for each view
 * @user_data: user data for @callback
 *
 * This function will call @callback for every view found in @self.
 *
 * Since: 3.26
 */
void
ide_layout_stack_foreach_view (IdeLayoutStack *self,
                               GtkCallback     callback,
                               gpointer        user_data)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (self);

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));
  g_return_if_fail (callback != NULL);

  gtk_container_foreach (GTK_CONTAINER (priv->stack), callback, user_data);
}

/**
 * ide_layout_stack_addin_find_by_module_name:
 * @stack: An #IdeLayoutStack
 * @module_name: the module name which provides the addin
 *
 * This function will locate the #IdeLayoutStackAddin that was registered by
 * the plugin named @module_name (which should match the "Module" field
 * provided in the .plugin file).
 *
 * If no module was found or that module does not implement the
 * #IdeLayoutStackAddinInterface, then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): An #IdeLayoutStackAddin or %NULL
 *
 * Since: 3.26
 */
IdeLayoutStackAddin *
ide_layout_stack_addin_find_by_module_name (IdeLayoutStack *stack,
                                            const gchar    *module_name)
{
  IdeLayoutStackPrivate *priv = ide_layout_stack_get_instance_private (stack);
  PeasExtension *ret = NULL;
  PeasPluginInfo *plugin_info;

  g_return_val_if_fail (IDE_IS_LAYOUT_STACK (stack), NULL);
  g_return_val_if_fail (priv->addins != NULL, NULL);
  g_return_val_if_fail (module_name != NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (peas_engine_get_default (), module_name);

  if (plugin_info != NULL)
    ret = peas_extension_set_get_extension (priv->addins, plugin_info);
  else
    g_warning ("No addin could be found matching module \"%s\"", module_name);

  return ret ? IDE_LAYOUT_STACK_ADDIN (ret) : NULL;
}
