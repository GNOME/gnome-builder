/* ide-source-map.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-source-map"

#include <glib/gi18n.h>

#include "ide-buffer.h"
#include "ide-line-change-gutter-renderer.h"
#include "ide-macros.h"
#include "ide-pango.h"
#include "ide-source-map.h"
#include "ide-source-view.h"

#define DEFAULT_WIDTH        100
#define CONCEAL_TIMEOUT      2000

struct _IdeSourceMap
{
  GtkOverlay               parent_instance;

  PangoFontDescription    *font_desc;

  GtkCssProvider          *view_css_provider;
  GtkCssProvider          *box_css_provider;

  GtkSourceView           *child_view;
  GtkEventBox             *overlay_box;
  GtkSourceView           *view;
  GtkSourceGutterRenderer *line_renderer;

  guint                    delayed_conceal_timeout;

  guint                    in_press : 1;
  guint                    show_map : 1;
};

G_DEFINE_TYPE (IdeSourceMap, ide_source_map, GTK_TYPE_OVERLAY)

enum {
  PROP_0,
  PROP_FONT_DESC,
  PROP_VIEW,
  LAST_PROP
};

enum {
  SHOW_MAP,
  HIDE_MAP,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint gSignals [LAST_SIGNAL];

static gboolean
ide_source_map_do_conceal (gpointer data)
{
  IdeSourceMap *self = data;

  g_assert (IDE_IS_SOURCE_MAP (self));

  self->delayed_conceal_timeout = 0;

  if (self->show_map == TRUE)
    {
      self->show_map = FALSE;
      g_signal_emit (self, gSignals [HIDE_MAP], 0);
    }

  return G_SOURCE_REMOVE;
}

static gboolean
ide_source_map__enter_notify_event (IdeSourceMap     *self,
                                    GdkEventCrossing *event,
                                    GtkWidget        *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, gSignals [SHOW_MAP], 0);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_show_map_and_queue_fade (IdeSourceMap *self)
{
  g_assert (IDE_IS_SOURCE_MAP (self));

  if (self->delayed_conceal_timeout != 0)
    g_source_remove (self->delayed_conceal_timeout);

  self->delayed_conceal_timeout = g_timeout_add (CONCEAL_TIMEOUT,
                                                 ide_source_map_do_conceal,
                                                 self);

  if (self->show_map == FALSE)
    {
      self->show_map = TRUE;
      g_signal_emit (self, gSignals [SHOW_MAP], 0);
    }
}

static gboolean
ide_source_map__leave_notify_event (IdeSourceMap     *self,
                                    GdkEventCrossing *event,
                                    GtkWidget        *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__motion_notify_event (IdeSourceMap   *self,
                                     GdkEventMotion *motion,
                                     GtkWidget      *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (motion != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__scroll_event (IdeSourceMap   *self,
                              GdkEventScroll *scroll,
                              GtkWidget      *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (scroll != NULL);
  g_assert (GTK_IS_WIDGET (widget));

  ide_source_map_show_map_and_queue_fade (self);

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_rebuild_css (IdeSourceMap *self)
{
  g_assert (IDE_IS_SOURCE_MAP (self));

  if (self->font_desc != NULL)
    {
      gchar *css;
      gchar *tmp;

      tmp = ide_pango_font_description_to_css (self->font_desc);
      css = g_strdup_printf ("GtkSourceView { %s }\n", tmp ?: "");
      gtk_css_provider_load_from_data (self->view_css_provider, css, -1, NULL);
      g_free (css);
      g_free (tmp);
    }

  if (self->view != NULL)
    {
      GtkSourceStyleScheme *style_scheme;
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
      style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

      if (style_scheme != NULL)
        {
          g_autofree gchar *background = NULL;
          GtkSourceStyle *style;

          if (!(style = gtk_source_style_scheme_get_style (style_scheme, "map-overlay")) &&
              !(style = gtk_source_style_scheme_get_style (style_scheme, "selection")))
            return;

          g_object_get (style,
                        "background", &background,
                        NULL);

          if ((background != NULL) && *background == '#')
            {
              gchar *css;

              css = g_strdup_printf ("IdeSourceMap GtkEventBox {"
                                     "  background-color: %s;"
                                     "  opacity: 0.75;"
                                     "  border-top: 1px solid shade(%s,0.9);"
                                     "  border-bottom: 1px solid shade(%s,0.9);"
                                     " }\n",
                                     background, background, background);
              gtk_css_provider_load_from_data (self->box_css_provider, css, -1, NULL);
              g_free (css);
            }
        }
    }
}

/**
 * ide_source_map_get_view:
 *
 * Gets the #IdeSourceMap:view property, which is the view this widget is mapping.
 *
 * Returns: (transfer none) (nullable): A #GtkSourceView or %NULL.
 */
GtkSourceView *
ide_source_map_get_view (IdeSourceMap *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_MAP (self), NULL);

  return self->view;
}

static void
update_scrubber_height (IdeSourceMap *self)
{
  GtkAllocation alloc;
  gdouble ratio;
  gint child_height;
  gint view_height;

  g_assert (self != NULL);
  g_assert (self->view != NULL);
  g_assert (self->child_view != NULL);

  gtk_widget_get_allocation (GTK_WIDGET (self->view), &alloc);
  gtk_widget_get_preferred_height (GTK_WIDGET (self->view), NULL, &view_height);
  gtk_widget_get_preferred_height (GTK_WIDGET (self->child_view), NULL, &child_height);

  ratio = alloc.height / (gdouble)view_height;
  child_height *= ratio;

  if (child_height > 0)
    g_object_set (self->overlay_box,
                  "height-request", child_height,
                  NULL);
}

static void
update_child_vadjustment (IdeSourceMap *self)
{
  GtkAdjustment *vadj;
  GtkAdjustment *child_vadj;
  gdouble value;
  gdouble upper;
  gdouble page_size;
  gdouble child_value;
  gdouble child_upper;
  gdouble child_page_size;
  gdouble new_value = 0.0;

  g_assert (IDE_IS_SOURCE_MAP (self));

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));
  child_vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->child_view));

  g_object_get (vadj,
                "upper", &upper,
                "value", &value,
                "page-size", &page_size,
                NULL);

  g_object_get (child_vadj,
                "upper", &child_upper,
                "value", &child_value,
                "page-size", &child_page_size,
                NULL);

  /*
   * TODO: Technically we should take into account lower here, but in practice
   *       it is always 0.0.
   */

  if (child_page_size < child_upper)
    new_value = (value / (upper - page_size)) * (child_upper - child_page_size);

  gtk_adjustment_set_value (child_vadj, new_value);
}

static void
ide_source_map__view_vadj_value_changed (IdeSourceMap  *self,
                                         GtkAdjustment *vadj)
{
  gdouble page_size;
  gdouble upper;
  gdouble lower;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_IS_ADJUSTMENT (vadj));

  gtk_widget_queue_resize (GTK_WIDGET (self->overlay_box));

  g_object_get (vadj,
                "lower", &lower,
                "page-size", &page_size,
                "upper", &upper,
                NULL);

  update_child_vadjustment (self);
}

static void
ide_source_map__view_vadj_notify_upper (IdeSourceMap  *self,
                                        GParamSpec    *pspec,
                                        GtkAdjustment *vadj)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_IS_ADJUSTMENT (vadj));

  update_scrubber_height (self);
}

static gboolean
transform_font_desc (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  const PangoFontDescription *font_desc;
  PangoFontDescription *copy = NULL;

  font_desc = g_value_get_boxed (from_value);

  if (font_desc != NULL)
    {
      copy = pango_font_description_copy (font_desc);
      pango_font_description_set_size (copy, PANGO_SCALE);
      pango_font_description_set_weight (copy, PANGO_WEIGHT_HEAVY);
    }

  g_value_take_boxed (to_value, copy);

  return TRUE;
}

static void
ide_source_map__buffer_notify_style_scheme (IdeSourceMap  *self,
                                            GParamSpec    *pspec,
                                            GtkTextBuffer *buffer)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  ide_source_map_rebuild_css (self);
}

static void
ide_source_map__buffer_line_flags_changed (IdeSourceMap *self,
                                           IdeBuffer    *buffer)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_source_gutter_renderer_queue_draw (self->line_renderer);
}

static void
ide_source_map__view_notify_buffer (IdeSourceMap  *self,
                                    GParamSpec    *pspec,
                                    GtkSourceView *view)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_SOURCE_IS_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  g_signal_connect_object (buffer,
                           "notify::style-scheme",
                           G_CALLBACK (ide_source_map__buffer_notify_style_scheme),
                           self,
                           G_CONNECT_SWAPPED);

  if (IDE_IS_BUFFER (buffer))
    g_signal_connect_object (buffer,
                             "line-flags-changed",
                             G_CALLBACK (ide_source_map__buffer_line_flags_changed),
                             self,
                             G_CONNECT_SWAPPED);

  ide_source_map_rebuild_css (self);
}

void
ide_source_map_set_view (IdeSourceMap  *self,
                         GtkSourceView *view)
{
  g_return_if_fail (IDE_IS_SOURCE_MAP (self));
  g_return_if_fail (!view || GTK_SOURCE_IS_VIEW (view));

  if (ide_set_weak_pointer (&self->view, view))
    {
      if (view != NULL)
        {
          GtkAdjustment *vadj;
          GtkTextBuffer *buffer;

          g_object_bind_property (self->view, "buffer",
                                  self->child_view, "buffer",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property (self->view, "indent-width",
                                  self->child_view, "indent-width",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property (self->view, "tab-width",
                                  self->child_view, "tab-width",
                                  G_BINDING_SYNC_CREATE);

          g_signal_connect_object (view,
                                   "notify::buffer",
                                   G_CALLBACK (ide_source_map__view_notify_buffer),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (view,
                                   "enter-notify-event",
                                   G_CALLBACK (ide_source_map__enter_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (view,
                                   "leave-notify-event",
                                   G_CALLBACK (ide_source_map__leave_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (view,
                                   "motion-notify-event",
                                   G_CALLBACK (ide_source_map__motion_notify_event),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (view,
                                   "scroll-event",
                                   G_CALLBACK (ide_source_map__scroll_event),
                                   self,
                                   G_CONNECT_SWAPPED);

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
          ide_source_map__buffer_notify_style_scheme (self, NULL, buffer);

          /*
           * TODO: Not sure what we should do about this in terms of abstraction.
           */
          if (IDE_IS_SOURCE_VIEW (self->view))
            g_object_bind_property_full (self->view, "font-desc",
                                         self, "font-desc",
                                         G_BINDING_SYNC_CREATE,
                                         transform_font_desc,
                                         NULL, NULL, NULL);

          vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->view));

          g_signal_connect_object (vadj,
                                   "value-changed",
                                   G_CALLBACK (ide_source_map__view_vadj_value_changed),
                                   self,
                                   G_CONNECT_SWAPPED);

          g_signal_connect_object (vadj,
                                   "notify::upper",
                                   G_CALLBACK (ide_source_map__view_vadj_notify_upper),
                                   self,
                                   G_CONNECT_SWAPPED);

          if ((gtk_widget_get_events (GTK_WIDGET (self->view)) & GDK_ENTER_NOTIFY_MASK) == 0)
            gtk_widget_add_events (GTK_WIDGET (self->view), GDK_ENTER_NOTIFY_MASK);

          if ((gtk_widget_get_events (GTK_WIDGET (self->view)) & GDK_LEAVE_NOTIFY_MASK) == 0)
            gtk_widget_add_events (GTK_WIDGET (self->view), GDK_LEAVE_NOTIFY_MASK);

          ide_source_map_rebuild_css (self);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_VIEW]);
    }
}

static void
ide_source_map_set_font_desc (IdeSourceMap               *self,
                              const PangoFontDescription *font_desc)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (font_desc != NULL);

  if (font_desc != self->font_desc)
    {
      if (self->font_desc)
        g_clear_pointer (&self->font_desc, pango_font_description_free);

      if (font_desc)
        self->font_desc = pango_font_description_copy (font_desc);
    }

  ide_source_map_rebuild_css (self);
}

static void
ide_source_map_set_font_name (IdeSourceMap *self,
                              const gchar  *font_name)
{
  PangoFontDescription *font_desc;

  g_assert (IDE_IS_SOURCE_MAP (self));

  if (font_name == NULL)
    font_name = "Monospace 1";

  font_desc = pango_font_description_from_string (font_name);
  ide_source_map_set_font_desc (self, font_desc);
  pango_font_description_free (font_desc);
}

static void
ide_source_map_get_preferred_width (GtkWidget *widget,
                                    gint      *mininum_width,
                                    gint      *natural_width)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;
  PangoLayout *layout;
  guint right_margin_position;
  gint height;
  gint width;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (mininum_width != NULL);
  g_assert (natural_width != NULL);

  if (self->font_desc == NULL)
    {
      *mininum_width = *natural_width = DEFAULT_WIDTH;
      return;
    }

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->child_view), "X");
  pango_layout_get_pixel_size (layout, &width, &height);
  g_clear_object (&layout);

  right_margin_position = gtk_source_view_get_right_margin_position (self->view);
  width *= right_margin_position;

  *mininum_width = *natural_width = width;
}

static void
ide_source_map_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (minimum_height != NULL);
  g_assert (natural_height != NULL);

  if (self->view == NULL)
    {
      *minimum_height = *natural_height = 0;
      return;
    }

  gtk_widget_get_preferred_height (GTK_WIDGET (self->child_view),
                                   minimum_height, natural_height);

  *minimum_height = 0;
}

static gboolean
ide_source_map_get_child_position (GtkOverlay   *overlay,
                                   GtkWidget    *child,
                                   GdkRectangle *alloc)
{
  IdeSourceMap *self = (IdeSourceMap *)overlay;
  GtkTextIter iter;
  GdkRectangle visible_area;
  GdkRectangle loc;
  GtkAllocation our_alloc;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_IS_OVERLAY (overlay));
  g_assert (GTK_IS_WIDGET (child));
  g_assert (alloc != NULL);

  if (self->view == NULL)
    return FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (overlay), &our_alloc);

  alloc->x = 0;
  alloc->width = our_alloc.width;

  gtk_widget_get_preferred_height (child, NULL, &alloc->height);

  gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self->view), &visible_area);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self->view), &iter, visible_area.x,
                                      visible_area.y);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self->child_view), &iter, &loc);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (self->child_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         loc.x, loc.y,
                                         NULL, &alloc->y);

  return TRUE;
}

static gboolean
ide_source_map__child_view_button_press_event (IdeSourceMap   *self,
                                               GdkEventButton *event,
                                               GtkSourceView  *child_view)
{
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_SOURCE_IS_VIEW (child_view));

  if (self->view != NULL)
    {
      gint x;
      gint y;

      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (child_view), GTK_TEXT_WINDOW_WIDGET,
                                             event->x, event->y, &x, &y);
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (child_view), &iter, x, y);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->view), &iter, 0.0, TRUE, 1.0, 0.5);
    }

  return GDK_EVENT_STOP;
}

static void
ide_source_map__child_view_state_flags_changed (GtkWidget     *widget,
                                                GtkStateFlags  flags,
                                                GtkWidget     *child_view)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;
  GdkWindow *window;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_SOURCE_IS_VIEW (child_view));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (child_view), GTK_TEXT_WINDOW_TEXT);
  if (window != NULL)
    gdk_window_set_cursor (window, NULL);
}

static void
ide_source_map__child_view_realize_after (GtkWidget *widget,
                                          GtkWidget *child_view)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;

  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_SOURCE_IS_VIEW (child_view));

  ide_source_map__child_view_state_flags_changed (widget, 0, child_view);
}

static gboolean
ide_source_map__overlay_box_button_press_event (IdeSourceMap   *self,
                                                GdkEventButton *event,
                                                GtkEventBox    *overlay_box)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (overlay_box));

  gtk_grab_add (GTK_WIDGET (overlay_box));

  self->in_press = TRUE;

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__overlay_box_button_release_event (IdeSourceMap   *self,
                                                  GdkEventButton *event,
                                                  GtkEventBox    *overlay_box)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (overlay_box));

  self->in_press = FALSE;

  gtk_grab_remove (GTK_WIDGET (overlay_box));

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_source_map__overlay_box_motion_notify_event (IdeSourceMap   *self,
                                                 GdkEventMotion *event,
                                                 GtkEventBox    *overlay_box)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (overlay_box));

  if (self->in_press && (self->view != NULL))
    {
      GtkAllocation alloc;
      GtkAllocation child_alloc;
      GtkTextBuffer *buffer;
      GdkRectangle rect;
      GtkTextIter iter;
      gdouble ratio;
      gint child_height;
      gint x;
      gint y;

      gtk_widget_get_allocation (GTK_WIDGET (overlay_box), &alloc);
      gtk_widget_get_allocation (GTK_WIDGET (self->child_view), &child_alloc);

      gtk_widget_translate_coordinates (GTK_WIDGET (overlay_box),
                                        GTK_WIDGET (self->child_view),
                                        event->x, event->y, &x, &y);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->child_view));
      gtk_text_buffer_get_end_iter (buffer, &iter);
      gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self->child_view), &iter, &rect);

      child_height = MIN (child_alloc.height, (rect.y + rect.height));

      y = CLAMP (y, child_alloc.y, child_alloc.y + child_height) - child_alloc.y;
      ratio = (gdouble)y / (gdouble)child_height;
      y = (rect.y + rect.height) * ratio;

      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self->child_view), &iter, x, y);

      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->view), &iter, 0.0, TRUE, 1.0, 0.5);
    }

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_size_allocate (GtkWidget     *widget,
                              GtkAllocation *alloc)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;

  GTK_WIDGET_CLASS (ide_source_map_parent_class)->size_allocate (widget, alloc);

  update_scrubber_height (self);
}

static gboolean
ide_source_map_do_scroll_event (IdeSourceMap   *self,
                                GdkEventScroll *event,
                                GtkWidget      *widget)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_WIDGET (widget));

#define SCROLL_ACCELERATION 4

  /*
   * TODO: This doesn't propagate kinetic scrolling or anything.
   *       We should probably make something that does that.
   */
  if (self->view != NULL)
    {
      gdouble x;
      gdouble y;
      gint count = 0;

      if (event->direction == GDK_SCROLL_UP)
        {
          count = -SCROLL_ACCELERATION;
        }
      else if (event->direction == GDK_SCROLL_DOWN)
        {
          count = SCROLL_ACCELERATION;
        }
      else
        {
          gdk_event_get_scroll_deltas ((GdkEvent *)event, &x, &y);

          if (y > 0)
            count = SCROLL_ACCELERATION;
          else if (y < 0)
            count = -SCROLL_ACCELERATION;
        }

      if (count != 0)
        g_signal_emit_by_name (self->view, "move-viewport", GTK_SCROLL_STEPS, count);
    }

#undef SCROLL_ACCELERATION

  return GDK_EVENT_PROPAGATE;
}

static void
ide_source_map_destroy (GtkWidget *widget)
{
  IdeSourceMap *self = (IdeSourceMap *)widget;

  if (self->delayed_conceal_timeout)
    {
      g_source_remove (self->delayed_conceal_timeout);
      self->delayed_conceal_timeout = 0;
    }

  g_clear_object (&self->box_css_provider);
  g_clear_object (&self->view_css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  ide_clear_weak_pointer (&self->view);

  GTK_WIDGET_CLASS (ide_source_map_parent_class)->destroy (widget);
}

static void
ide_source_map_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  IdeSourceMap *self = IDE_SOURCE_MAP (object);

  switch (prop_id)
    {
    case PROP_VIEW:
      g_value_set_object (value, ide_source_map_get_view (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_map_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  IdeSourceMap *self = IDE_SOURCE_MAP (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      ide_source_map_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_VIEW:
      ide_source_map_set_view (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_map_class_init (IdeSourceMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkOverlayClass *overlay_class = GTK_OVERLAY_CLASS (klass);

  object_class->get_property = ide_source_map_get_property;
  object_class->set_property = ide_source_map_set_property;

  widget_class->destroy = ide_source_map_destroy;
  widget_class->get_preferred_height = ide_source_map_get_preferred_height;
  widget_class->get_preferred_width = ide_source_map_get_preferred_width;
  widget_class->size_allocate = ide_source_map_size_allocate;

  overlay_class->get_child_position = ide_source_map_get_child_position;

  gParamSpecs [PROP_VIEW] =
    g_param_spec_object ("view",
                         _("View"),
                         _("The view this widget is mapping."),
                         GTK_SOURCE_TYPE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gParamSpecs [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _("Font Description"),
                        _("The Pango font description to use."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, gParamSpecs);

  gSignals [HIDE_MAP] =
    g_signal_new ("hide-map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  gSignals [SHOW_MAP] =
    g_signal_new ("show-map",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static void
ide_source_map_init (IdeSourceMap *self)
{
  GtkSourceCompletion *completion;
  GtkSourceGutter *gutter;
  GtkStyleContext *context;

  self->child_view = g_object_new (GTK_SOURCE_TYPE_VIEW,
                                   "auto-indent", FALSE,
                                   "can-focus", FALSE,
                                   "draw-spaces", 0,
                                   "editable", FALSE,
                                   "expand", FALSE,
                                   "monospace", TRUE,
                                   "show-line-numbers", FALSE,
                                   "show-line-marks", FALSE,
                                   "show-right-margin", FALSE,
                                   "visible", TRUE,
                                   NULL);
  g_signal_connect_object (self->child_view,
                           "button-press-event",
                           G_CALLBACK (ide_source_map__child_view_button_press_event),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_events (GTK_WIDGET (self->child_view), GDK_SCROLL_MASK);
  g_signal_connect_object (self->child_view,
                           "scroll-event",
                           G_CALLBACK (ide_source_map_do_scroll_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->child_view,
                           "state-flags-changed",
                           G_CALLBACK (ide_source_map__child_view_state_flags_changed),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_signal_connect_object (self->child_view,
                           "realize",
                           G_CALLBACK (ide_source_map__child_view_realize_after),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  self->view_css_provider = gtk_css_provider_new ();
  context = gtk_widget_get_style_context (GTK_WIDGET (self->child_view));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->view_css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->child_view));

  /*
   * TODO:
   *
   * nacho/pbor/gsv maintainers
   *
   * We should make this packable via GtkBuilder or an internal-child or something.
   * That way, we can have GtkSourceMap in gb-editor-frame.ui and Builder can just
   * add this there.
   */
  gutter = gtk_source_view_get_gutter (self->child_view, GTK_TEXT_WINDOW_LEFT);
  self->line_renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                                      "size", 2,
                                      "visible", TRUE,
                                      NULL);
  gtk_source_gutter_insert (gutter, self->line_renderer, 0);

  self->overlay_box = g_object_new (GTK_TYPE_EVENT_BOX,
                                    "opacity", 0.5,
                                    "visible", TRUE,
                                    "height-request", 10,
                                    "width-request", 100,
                                    NULL);
  g_signal_connect_object (self->overlay_box,
                           "button-press-event",
                           G_CALLBACK (ide_source_map__overlay_box_button_press_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "scroll-event",
                           G_CALLBACK (ide_source_map_do_scroll_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "button-release-event",
                           G_CALLBACK (ide_source_map__overlay_box_button_release_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "motion-notify-event",
                           G_CALLBACK (ide_source_map__overlay_box_motion_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  context = gtk_widget_get_style_context (GTK_WIDGET (self->overlay_box));
  self->box_css_provider = gtk_css_provider_new ();
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (self->box_css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_overlay_add_overlay (GTK_OVERLAY (self), GTK_WIDGET (self->overlay_box));

  completion = gtk_source_view_get_completion (self->child_view);
  gtk_source_completion_block_interactive (completion);

  ide_source_map_set_font_name (self, "Monospace 1");

  gtk_widget_add_events (GTK_WIDGET (self->overlay_box),
                         (GDK_SCROLL_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK));
  gtk_widget_add_events (GTK_WIDGET (self->child_view),
                         (GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK));

  g_signal_connect_object (self->overlay_box,
                           "enter-notify-event",
                           G_CALLBACK (ide_source_map__enter_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "leave-notify-event",
                           G_CALLBACK (ide_source_map__leave_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "motion-notify-event",
                           G_CALLBACK (ide_source_map__motion_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->overlay_box,
                           "scroll-event",
                           G_CALLBACK (ide_source_map__scroll_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->child_view,
                           "enter-notify-event",
                           G_CALLBACK (ide_source_map__enter_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->child_view,
                           "leave-notify-event",
                           G_CALLBACK (ide_source_map__leave_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->child_view,
                           "motion-notify-event",
                           G_CALLBACK (ide_source_map__motion_notify_event),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (self->child_view,
                           "scroll-event",
                           G_CALLBACK (ide_source_map__scroll_event),
                           self,
                           G_CONNECT_SWAPPED);
}
