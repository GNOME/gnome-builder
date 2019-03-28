/* ide-completion-window.c
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

#define G_LOG_DOMAIN "ide-completion-window"

#include "config.h"

#include "ide-completion.h"
#include "ide-completion-context.h"
#include "ide-completion-display.h"
#include "ide-completion-window.h"
#include "ide-completion-private.h"
#include "ide-completion-proposal.h"
#include "ide-completion-provider.h"
#include "ide-completion-view.h"

struct _IdeCompletionWindow
{
  GtkWindow          parent_instance;
  IdeCompletionView *view;
};

enum {
  PROP_0,
  PROP_CONTEXT,
  N_PROPS
};

extern gpointer *gdk__private__                (void);
static void      completion_display_iface_init (IdeCompletionDisplayInterface *);

G_DEFINE_TYPE_WITH_CODE (IdeCompletionWindow, ide_completion_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_COMPLETION_DISPLAY,
                                                completion_display_iface_init))

static GParamSpec *properties [N_PROPS];

static gboolean
ide_completion_window_reposition (IdeCompletionWindow *self)
{
  IdeCompletionContext *context;
  GtkRequisition min, nat;
  IdeCompletion *completion;
  GdkRectangle rect;
  GdkRectangle begin_rect, end_rect;
  GtkSourceView *view;
  GtkTextIter begin, end;
  GtkWidget *toplevel;
  GdkWindow *window;
  gint x_offset = 0;

  g_assert (IDE_IS_COMPLETION_WINDOW (self));

  context = ide_completion_view_get_context (self->view);

  if (context == NULL)
    return FALSE;

  if (!(completion = ide_completion_context_get_completion (context)))
    return FALSE;

  if (!(view = ide_completion_get_view (completion)))
    return FALSE;

  if (!ide_completion_context_get_bounds (context, &begin, &end))
    return FALSE;

  if (!(toplevel = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW)))
    return FALSE;

  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &begin, &begin_rect);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &end, &end_rect);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         begin_rect.x, begin_rect.y,
                                         &begin_rect.x, &begin_rect.y);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         end_rect.x, end_rect.y,
                                         &end_rect.x, &end_rect.y);
  gdk_rectangle_union (&begin_rect, &end_rect, &rect);
  gtk_widget_translate_coordinates (GTK_WIDGET (view), toplevel,
                                    rect.x, rect.y,
                                    &rect.x, &rect.y);

  if (!gtk_widget_get_realized (GTK_WIDGET (self)))
    gtk_widget_realize (GTK_WIDGET (self));

  gtk_widget_get_preferred_size (GTK_WIDGET (self), &min, &nat);

  window = gtk_widget_get_window (GTK_WIDGET (self));

  x_offset = _ide_completion_view_get_x_offset (self->view);

#if 0
  g_print ("Target: %d,%d %dx%d (%d)\n",
           rect.x, rect.y, rect.width, rect.height, x_offset);
#endif

/* TODO: figure out where this comes from */
#define EXTRA_SPACE 9

  gdk_window_move_to_rect (window,
                           &rect,
                           GDK_GRAVITY_SOUTH_WEST,
                           GDK_GRAVITY_NORTH_WEST,
                           GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_RESIZE_X,
                           -x_offset + EXTRA_SPACE,
                           0);

  return TRUE;
}

static void
ide_completion_window_real_show (GtkWidget *widget)
{
  IdeCompletionWindow *self = (IdeCompletionWindow *)widget;

  g_assert (IDE_IS_COMPLETION_WINDOW (self));

  ide_completion_window_reposition (self);
  gtk_widget_set_opacity (widget, 1.0);

  GTK_WIDGET_CLASS (ide_completion_window_parent_class)->show (widget);
}

static void
ide_completion_window_real_realize (GtkWidget *widget)
{
  IdeCompletionWindow *self = (IdeCompletionWindow *)widget;
  GdkScreen *screen;
  GdkVisual *visual;

  g_assert (IDE_IS_COMPLETION_WINDOW (self));

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual != NULL)
    gtk_widget_set_visual (widget, visual);

  GTK_WIDGET_CLASS (ide_completion_window_parent_class)->realize (widget);

  ide_completion_window_reposition (self);
}

static void
ide_completion_window_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeCompletionWindow *self = IDE_COMPLETION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, ide_completion_window_get_context (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_window_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeCompletionWindow *self = IDE_COMPLETION_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      ide_completion_window_set_context (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_completion_window_class_init (IdeCompletionWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = ide_completion_window_get_property;
  object_class->set_property = ide_completion_window_set_property;

  widget_class->show = ide_completion_window_real_show;
  widget_class->realize = ide_completion_window_real_realize;

  properties [PROP_CONTEXT] =
    g_param_spec_object ("context",
                         "Context",
                         "The completion context to display results for",
                         IDE_TYPE_COMPLETION_CONTEXT,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "completionwindow");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-sourceview/ui/ide-completion-window.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeCompletionWindow, view);

  g_type_ensure (IDE_TYPE_COMPLETION_VIEW);
}

static void
ide_completion_window_init (IdeCompletionWindow *self)
{
  gtk_window_set_type_hint (GTK_WINDOW (self), GDK_WINDOW_TYPE_HINT_COMBO);
  gtk_window_set_skip_pager_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (self), TRUE);
  gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (self), FALSE);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (self->view,
                            "reposition",
                            G_CALLBACK (ide_completion_window_reposition),
                            self);
}

IdeCompletionWindow *
_ide_completion_window_new (GtkWidget *view)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_ancestor (view, GTK_TYPE_WINDOW);

  return g_object_new (IDE_TYPE_COMPLETION_WINDOW,
                       "destroy-with-parent", TRUE,
                       "modal", FALSE,
                       "transient-for", toplevel,
                       "type", GTK_WINDOW_POPUP,
                       NULL);
}

/**
 * ide_completion_window_get_context:
 * @self: a #IdeCompletionWindow
 *
 * Gets the context that is being displayed in the window, or %NULL.
 *
 * Returns: (transfer none) (nullable): an #IdeCompletionContext or %NULL
 *
 * Since: 3.32
 */
IdeCompletionContext *
ide_completion_window_get_context (IdeCompletionWindow *self)
{
  g_return_val_if_fail (IDE_IS_COMPLETION_WINDOW (self), NULL);

  return ide_completion_view_get_context (self->view);
}

/**
 * ide_completion_window_set_context:
 * @self: a #IdeCompletionWindow
 *
 * Sets the context to be displayed in the window.
 *
 * Since: 3.32
 */
void
ide_completion_window_set_context (IdeCompletionWindow  *self,
                                   IdeCompletionContext *context)
{
  g_return_if_fail (IDE_IS_COMPLETION_WINDOW (self));
  g_return_if_fail (!context || IDE_IS_COMPLETION_CONTEXT (context));

  ide_completion_view_set_context (self->view, context);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
}

static gboolean
ide_completion_window_key_press_event (IdeCompletionDisplay *display,
                                       const GdkEventKey    *event)
{
  g_assert (IDE_IS_COMPLETION_WINDOW (display));
  g_assert (event != NULL);

  return _ide_completion_view_handle_key_press (IDE_COMPLETION_WINDOW (display)->view, event);
}

static void
ide_completion_window_set_n_rows (IdeCompletionDisplay *display,
                                  guint                 n_rows)
{
  g_assert (IDE_IS_COMPLETION_WINDOW (display));
  g_assert (n_rows > 0);
  g_assert (n_rows <= 32);

  _ide_completion_view_set_n_rows (IDE_COMPLETION_WINDOW (display)->view, n_rows);
}

static void
ide_completion_window_attach (IdeCompletionDisplay *display,
                              GtkSourceView        *view)
{
  GtkWidget *toplevel;

  g_assert (IDE_IS_COMPLETION_WINDOW (display));
  g_assert (IDE_IS_SOURCE_VIEW (view));

  if ((toplevel = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW)))
    gtk_window_set_transient_for (GTK_WINDOW (display), GTK_WINDOW (toplevel));
}


static void
ide_completion_window_move_cursor (IdeCompletionDisplay *display,
                                   GtkMovementStep       step,
                                   gint                  count)
{
  g_assert (IDE_IS_COMPLETION_WINDOW (display));

  _ide_completion_view_move_cursor (IDE_COMPLETION_WINDOW (display)->view, step, count);
}

static void
ide_completion_window_set_font_desc (IdeCompletionDisplay       *display,
                                     const PangoFontDescription *font_desc)
{
  g_assert (IDE_IS_COMPLETION_WINDOW (display));

  _ide_completion_view_set_font_desc (IDE_COMPLETION_WINDOW (display)->view, font_desc);
}

static void
completion_display_iface_init (IdeCompletionDisplayInterface *iface)
{
  iface->set_context = (gpointer)ide_completion_window_set_context;
  iface->set_n_rows = ide_completion_window_set_n_rows;
  iface->attach = ide_completion_window_attach;
  iface->key_press_event = ide_completion_window_key_press_event;
  iface->move_cursor = ide_completion_window_move_cursor;
  iface->set_font_desc = ide_completion_window_set_font_desc;
}
