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

#include "ide-macros.h"
#include "ide-pango.h"
#include "ide-source-map.h"

#define DEFAULT_WIDTH 100

struct _IdeSourceMap
{
  GtkSourceView         parent_instance;

  PangoFontDescription *font_desc;
  GtkSourceView        *view;
  GtkCssProvider       *css_provider;
};

struct _IdeSourceMapClass
{
  GtkSourceViewClass parent_instance;
};

G_DEFINE_TYPE (IdeSourceMap, ide_source_map, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_VIEW,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

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
ide_source_map__view_vadj_value_changed (IdeSourceMap  *self,
                                         GtkAdjustment *vadj)
{
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (GTK_IS_ADJUSTMENT (vadj));

  gtk_widget_queue_draw (GTK_WIDGET (self));
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

          g_object_bind_property (view, "buffer",
                                  self, "buffer",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property (view, "indent-width",
                                  self, "indent-width",
                                  G_BINDING_SYNC_CREATE);
          g_object_bind_property (view, "tab-width",
                                  self, "tab-width",
                                  G_BINDING_SYNC_CREATE);

          vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));
          g_signal_connect_object (vadj,
                                   "value-changed",
                                   G_CALLBACK (ide_source_map__view_vadj_value_changed),
                                   self,
                                   G_CONNECT_SWAPPED);
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

  g_clear_pointer (&self->font_desc, pango_font_description_free);

  if (!self->css_provider)
    {
      GtkStyleContext *style_context;

      self->css_provider = gtk_css_provider_new ();
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_provider (style_context,
                                      GTK_STYLE_PROVIDER (self->css_provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (font_desc)
    {
      g_autofree gchar *str = NULL;
      g_autofree gchar *css = NULL;

      self->font_desc = pango_font_description_copy (font_desc);
      str = ide_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("IdeSourceMap { %s }", str ?: "");
      gtk_css_provider_load_from_data (self->css_provider, css, -1, NULL);
    }
}

static void
ide_source_map_set_font_name (IdeSourceMap *self,
                              const gchar  *font_name)
{
  PangoFontDescription *font_desc;

  g_assert (IDE_IS_SOURCE_MAP (self));

  if (font_name == NULL)
    font_name = "Monospace";

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

  layout = gtk_widget_create_pango_layout (widget, "X");
  pango_layout_get_pixel_size (layout, &width, &height);
  g_clear_object (&layout);

  right_margin_position = gtk_source_view_get_right_margin_position (self->view);

  width *= right_margin_position;

  *mininum_width = *natural_width = width;
}

static void
ide_source_map_draw_layer (GtkTextView      *text_view,
                           GtkTextViewLayer  layer,
                           cairo_t          *cr)
{
  IdeSourceMap *self = (IdeSourceMap *)text_view;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_MAP (self));
  g_assert (cr != NULL);

  if (self->view == NULL)
    return;

  GTK_TEXT_VIEW_CLASS (ide_source_map_parent_class)->draw_layer (text_view, layer, cr);

  if (layer == GTK_TEXT_VIEW_LAYER_ABOVE)
    {
      GdkRectangle visible_rect;
      GdkRectangle my_visible_rect;
      GdkRectangle area;
      GdkRectangle clip;
      GtkTextIter iter;

      cairo_save (cr);

      gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self->view), &visible_rect);

      gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self), &my_visible_rect);
      gdk_cairo_get_clip_rectangle (cr, &clip);

      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self->view), &iter, visible_rect.x, visible_rect.y);
      gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &area);
      cairo_move_to (cr, clip.x, area.y - my_visible_rect.y + 0.5);
      cairo_line_to (cr, clip.x + clip.width, area.y - my_visible_rect.y + 0.5);

      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self->view), &iter, visible_rect.x, visible_rect.y + visible_rect.height);
      gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &area);
      cairo_move_to (cr, clip.x, area.y + area.height - my_visible_rect.y + 0.5);
      cairo_line_to (cr, clip.x + clip.width, area.y + area.height - my_visible_rect.y + 0.5);

      cairo_set_source_rgb (cr, .63, .63, .63);
      cairo_set_line_width (cr, 1);
      cairo_stroke (cr);

      cairo_restore (cr);
    }
}

static void
ide_source_map_finalize (GObject *object)
{
  IdeSourceMap *self = (IdeSourceMap *)object;

  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  ide_clear_weak_pointer (&self->view);

  G_OBJECT_CLASS (ide_source_map_parent_class)->finalize (object);
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
  GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);

  object_class->finalize = ide_source_map_finalize;
  object_class->get_property = ide_source_map_get_property;
  object_class->set_property = ide_source_map_set_property;

  widget_class->get_preferred_width = ide_source_map_get_preferred_width;

  text_view_class->draw_layer = ide_source_map_draw_layer;

  gParamSpecs [PROP_VIEW] =
    g_param_spec_object ("view",
                         _("View"),
                         _("The view this widget is mapping."),
                         GTK_SOURCE_TYPE_VIEW,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_VIEW, gParamSpecs [PROP_VIEW]);
}

static void
ide_source_map_init (IdeSourceMap *self)
{
  GtkSourceCompletion *completion;

  gtk_widget_set_can_focus (GTK_WIDGET (self), FALSE);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (self), FALSE);
  gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), FALSE);
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self), FALSE);
  gtk_source_view_set_show_line_marks (GTK_SOURCE_VIEW (self), FALSE);
  gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (self), FALSE);

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_block_interactive (completion);

  ide_source_map_set_font_name (self, "Monospace 1");
}
