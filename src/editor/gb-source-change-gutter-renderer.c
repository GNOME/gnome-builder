/* gb-source-change-gutter-renderer.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#include <glib/gi18n.h>

#include "gb-source-change-gutter-renderer.h"
#include "gb-source-change-monitor.h"

struct _GbSourceChangeGutterRendererPrivate
{
  GbSourceChangeMonitor *change_monitor;
};

enum
{
  PROP_0,
  PROP_CHANGE_MONITOR,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceChangeGutterRenderer,
                            gb_source_change_gutter_renderer,
                            GTK_SOURCE_TYPE_GUTTER_RENDERER)

static GParamSpec *gParamSpecs [LAST_PROP];

GbSourceChangeMonitor *
gb_source_change_gutter_renderer_get_change_monitor (GbSourceChangeGutterRenderer *renderer)
{
  g_return_val_if_fail (GB_IS_SOURCE_CHANGE_GUTTER_RENDERER (renderer), NULL);

  return renderer->priv->change_monitor;
}

static void
on_changed (GbSourceChangeMonitor        *monitor,
            GbSourceChangeGutterRenderer *renderer)
{
  GtkTextView *text_view;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_GUTTER_RENDERER (renderer));

  text_view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (renderer));
  gtk_widget_queue_draw (GTK_WIDGET (text_view));
}

static void
gb_source_change_gutter_renderer_set_change_monitor (GbSourceChangeGutterRenderer *renderer,
                                                     GbSourceChangeMonitor        *monitor)
{
  GbSourceChangeGutterRendererPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_GUTTER_RENDERER (renderer));
  g_return_if_fail (!monitor || GB_IS_SOURCE_CHANGE_MONITOR (monitor));

  priv = renderer->priv;

  if (priv->change_monitor)
    {
      g_signal_handlers_disconnect_by_func (priv->change_monitor,
                                            G_CALLBACK (on_changed),
                                            renderer);
      g_object_remove_weak_pointer (G_OBJECT (priv->change_monitor),
                                    (gpointer *)&priv->change_monitor);
      priv->change_monitor = NULL;
    }

  if (monitor)
    {
      priv->change_monitor = monitor;
      g_object_add_weak_pointer (G_OBJECT (priv->change_monitor),
                                 (gpointer *)&priv->change_monitor);
      g_signal_connect (priv->change_monitor,
                        "changed",
                        G_CALLBACK (on_changed),
                        renderer);
    }

  gtk_source_gutter_renderer_queue_draw (GTK_SOURCE_GUTTER_RENDERER (renderer));
}

static void
gb_source_change_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                                       cairo_t                      *cr,
                                       GdkRectangle                 *bg_area,
                                       GdkRectangle                 *cell_area,
                                       GtkTextIter                  *begin,
                                       GtkTextIter                  *end,
                                       GtkSourceGutterRendererState  state)
{
  GbSourceChangeGutterRendererPrivate *priv;
  GbSourceChangeFlags flags;
  GdkRGBA rgba;
  guint lineno;

  g_return_if_fail (GB_IS_SOURCE_CHANGE_GUTTER_RENDERER (renderer));
  g_return_if_fail (cr);
  g_return_if_fail (bg_area);
  g_return_if_fail (cell_area);
  g_return_if_fail (begin);
  g_return_if_fail (end);

  priv = GB_SOURCE_CHANGE_GUTTER_RENDERER (renderer)->priv;

  GTK_SOURCE_GUTTER_RENDERER_CLASS (gb_source_change_gutter_renderer_parent_class)->draw (renderer, cr, bg_area, cell_area, begin, end, state);

  lineno = gtk_text_iter_get_line (begin);
  flags = gb_source_change_monitor_get_line (priv->change_monitor, lineno);

  if (!flags)
    return;

  if ((flags & GB_SOURCE_CHANGE_ADDED) != 0)
    gdk_rgba_parse (&rgba, "#8ae234");

  if ((flags & GB_SOURCE_CHANGE_CHANGED) != 0)
    gdk_rgba_parse (&rgba, "#fcaf3e");

  gdk_cairo_rectangle (cr, cell_area);
  gdk_cairo_set_source_rgba (cr, &rgba);
  cairo_fill (cr);
}

static void
gb_source_change_gutter_renderer_dispose (GObject *object)
{
  GbSourceChangeGutterRenderer *renderer = (GbSourceChangeGutterRenderer *)object;

  gb_source_change_gutter_renderer_set_change_monitor (renderer, NULL);

  G_OBJECT_CLASS (gb_source_change_gutter_renderer_parent_class)->dispose (object);
}

static void
gb_source_change_gutter_renderer_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  GbSourceChangeGutterRenderer *renderer = GB_SOURCE_CHANGE_GUTTER_RENDERER (object);

  switch (prop_id) {
  case PROP_CHANGE_MONITOR:
    g_value_set_object (value, renderer->priv->change_monitor);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_change_gutter_renderer_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  GbSourceChangeGutterRenderer *renderer = GB_SOURCE_CHANGE_GUTTER_RENDERER (object);

  switch (prop_id) {
  case PROP_CHANGE_MONITOR:
    gb_source_change_gutter_renderer_set_change_monitor (
        renderer, g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gb_source_change_gutter_renderer_class_init (GbSourceChangeGutterRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

  object_class->dispose = gb_source_change_gutter_renderer_dispose;
  object_class->get_property = gb_source_change_gutter_renderer_get_property;
  object_class->set_property = gb_source_change_gutter_renderer_set_property;

  renderer_class->draw = gb_source_change_gutter_renderer_draw;

  gParamSpecs [PROP_CHANGE_MONITOR] =
    g_param_spec_object ("change-monitor",
                         _("Change Monitor"),
                         _("The change monitor for the gutter renderer."),
                         GB_TYPE_SOURCE_CHANGE_MONITOR,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_CHANGE_MONITOR,
                                   gParamSpecs [PROP_CHANGE_MONITOR]);
}

static void
gb_source_change_gutter_renderer_init (GbSourceChangeGutterRenderer *renderer)
{
  renderer->priv = gb_source_change_gutter_renderer_get_instance_private (renderer);
}
