/* gb-source-search-highlighter.c
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

#include "gb-cairo.h"
#include "gb-rgba.h"
#include "gb-source-search-highlighter.h"

struct _GbSourceSearchHighlighterPrivate
{
  GtkSourceView           *source_view;
  GtkSourceSearchSettings *search_settings;
  GtkSourceSearchContext  *search_context;
};

enum {
  PROP_0,
  PROP_SEARCH_CONTEXT,
  PROP_SEARCH_SETTINGS,
  PROP_SOURCE_VIEW,
  LAST_PROP
};

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceSearchHighlighter,
                            gb_source_search_highlighter,
                            G_TYPE_OBJECT)

static GParamSpec * gParamSpecs[LAST_PROP];
static guint gSignals[LAST_SIGNAL];

static void
add_match (GtkTextView       *text_view,
           cairo_region_t    *region,
           const GtkTextIter *begin,
           const GtkTextIter *end)
{
  GdkRectangle begin_rect;
  GdkRectangle end_rect;
  cairo_rectangle_int_t rect;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (region);
  g_assert (begin);
  g_assert (end);

  /*
   * NOTE: @end is not inclusive of the match.
   */

  if (gtk_text_iter_get_line (begin) == gtk_text_iter_get_line (end))
    {
      gtk_text_view_get_iter_location (text_view, begin, &begin_rect);
      gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                             begin_rect.x, begin_rect.y,
                                             &begin_rect.x, &begin_rect.y);
      gtk_text_view_get_iter_location (text_view, end, &end_rect);
      gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                             end_rect.x, end_rect.y,
                                             &end_rect.x, &end_rect.y);
      rect.x = begin_rect.x;
      rect.y = begin_rect.y;
      rect.width = end_rect.x - begin_rect.x;
      rect.height = MAX (begin_rect.height, end_rect.height);
      cairo_region_union_rectangle (region, &rect);
      return;
    }

  /*
   * TODO: Complex matches.
   */

  g_warning ("Need to support complex matches (multi-line)");
}

static void
add_matches (GtkTextView            *text_view,
             cairo_region_t         *region,
             GtkSourceSearchContext *search_context,
             const GtkTextIter      *begin,
             const GtkTextIter      *end)
{
  GtkTextIter first_begin;
  GtkTextIter new_begin;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (region);
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_assert (begin);
  g_assert (end);

  if (!gtk_source_search_context_forward (search_context,
                                          begin,
                                          &first_begin,
                                          &match_end))
    return;

  add_match (text_view, region, &first_begin, &match_end);

  for (;; )
    {
      gtk_text_iter_assign (&new_begin, &match_end);

      if (gtk_source_search_context_forward (search_context,
                                             &new_begin,
                                             &match_begin,
                                             &match_end) &&
          (gtk_text_iter_compare (&match_begin, end) < 0) &&
          (gtk_text_iter_compare (&first_begin, &match_begin) != 0))
        {
          add_match (text_view, region, &match_begin, &match_end);
          continue;
        }

      break;
    }
}

static void
draw_bezel (cairo_t                     *cr,
            const cairo_rectangle_int_t *rect,
            guint                        radius,
            const GdkRGBA               *rgba)
{
  GdkRectangle r;

  r.x = rect->x - radius;
  r.y = rect->y - radius;
  r.width = rect->width + (radius * 2);
  r.height = rect->height + (radius * 2);

  gdk_cairo_set_source_rgba (cr, rgba);
  gb_cairo_rounded_rectangle (cr, &r, radius, radius);
  cairo_fill (cr);
}

void
gb_source_search_highlighter_draw (GbSourceSearchHighlighter *highlighter,
                                   GtkTextView               *text_view,
                                   cairo_t                   *cr)
{
  GbSourceSearchHighlighterPrivate *priv;
  GtkSourceStyleScheme *scheme;
  cairo_region_t *clip_region;
  cairo_region_t *match_region;
  GtkSourceStyle *style;
  GtkTextBuffer *buffer;
  GdkRectangle area;
  GtkTextIter begin;
  GtkTextIter end;
  GdkRGBA color;
  GdkRGBA color1;
  GdkRGBA color2;

  g_return_if_fail (GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (cr);

  priv = highlighter->priv;

  if (!priv->search_context ||
      !gtk_source_search_context_get_highlight (priv->search_context))
    return;

  buffer = gtk_text_view_get_buffer (text_view);
  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  style = gtk_source_style_scheme_get_style (scheme, "search-match");

  if (style)
    {
      gchar *background;

      /*
       * TODO: We should probably cache this.
       */

      g_object_get (style, "background", &background, NULL);
      gdk_rgba_parse (&color, background);
      gb_rgba_shade (&color, &color1, 0.8);
      gb_rgba_shade (&color, &color2, 1.1);
      g_free (background);
    }
  else
    {
      gdk_rgba_parse (&color1, "#edd400");
      gdk_rgba_parse (&color2, "#fce94f");
    }

  gdk_cairo_get_clip_rectangle (cr, &area);
  gtk_text_view_window_to_buffer_coords (text_view,
                                         GTK_TEXT_WINDOW_TEXT,
                                         area.x,
                                         area.y,
                                         &area.x,
                                         &area.y);
  gtk_text_view_get_iter_at_location (text_view, &begin, area.x, area.y);
  gtk_text_view_get_iter_at_location (text_view, &end,
                                      area.x + area.width,
                                      area.y + area.height);

  if (!gdk_cairo_get_clip_rectangle (cr, &area))
    g_assert_not_reached ();

  clip_region = cairo_region_create_rectangle (&area);
  match_region = cairo_region_create ();
  add_matches (text_view, match_region, priv->search_context, &begin, &end);

  cairo_region_subtract (clip_region, match_region);

#if 0
  /* uncomment to shadow the background */
  gdk_cairo_region (cr, clip_region);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.2);
  cairo_fill (cr);
#endif

  {
    cairo_rectangle_int_t r;
    gint n;
    gint i;

    gdk_cairo_region (cr, clip_region);
    cairo_clip (cr);

    n = cairo_region_num_rectangles (match_region);
    for (i = 0; i < n; i++)
      {
        cairo_region_get_rectangle (match_region, i, &r);

        draw_bezel (cr, &r, 3, &color1);
        draw_bezel (cr, &r, 2, &color2);
      }
  }

  cairo_region_destroy (clip_region);
  cairo_region_destroy (match_region);
}

void
gb_source_search_highlighter_set_search_context (GbSourceSearchHighlighter *highlighter,
                                                 GtkSourceSearchContext    *search_context)
{
  GbSourceSearchHighlighterPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));
  g_return_if_fail (!search_context || GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  priv = highlighter->priv;

  g_clear_object (&priv->search_context);
  priv->search_context = search_context ? g_object_ref (search_context) : NULL;
}

void
gb_source_search_highlighter_set_search_settings (GbSourceSearchHighlighter *highlighter,
                                                  GtkSourceSearchSettings   *search_settings)
{
  GbSourceSearchHighlighterPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));
  g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  priv = highlighter->priv;

  g_clear_object (&priv->search_settings);
  priv->search_settings = search_settings ? g_object_ref (search_settings) : NULL;
}

static void
gb_source_search_highlighter_finalize (GObject *object)
{
  GbSourceSearchHighlighterPrivate *priv;

  priv = GB_SOURCE_SEARCH_HIGHLIGHTER (object)->priv;

  g_clear_object (&priv->search_context);
  g_clear_object (&priv->search_settings);

  G_OBJECT_CLASS (gb_source_search_highlighter_parent_class)->finalize (object);
}

static void
gb_source_search_highlighter_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GbSourceSearchHighlighter *highlighter = GB_SOURCE_SEARCH_HIGHLIGHTER (object);

  switch (prop_id)
    {
    case PROP_SEARCH_CONTEXT:
      gb_source_search_highlighter_set_search_context (highlighter, g_value_get_object (value));
      break;

    case PROP_SEARCH_SETTINGS:
      gb_source_search_highlighter_set_search_settings (highlighter, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_search_highlighter_class_init (GbSourceSearchHighlighterClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_search_highlighter_finalize;
  object_class->set_property = gb_source_search_highlighter_set_property;

  gParamSpecs[PROP_SEARCH_CONTEXT] =
    g_param_spec_object ("search-context",
                         _("Search Context"),
                         _("Search Context"),
                         GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_CONTEXT,
                                   gParamSpecs[PROP_SEARCH_CONTEXT]);

  gParamSpecs[PROP_SEARCH_SETTINGS] =
    g_param_spec_object ("search-settings",
                         _("Search Settings"),
                         _("Search Settings"),
                         GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_SETTINGS,
                                   gParamSpecs[PROP_SEARCH_SETTINGS]);

  gSignals[CHANGED] = g_signal_new ("changed",
                                    GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER,
                                    G_SIGNAL_RUN_FIRST,
                                    0,
                                    NULL,
                                    NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE,
                                    0);
}

static void
gb_source_search_highlighter_init (GbSourceSearchHighlighter *highlighter)
{
  highlighter->priv = gb_source_search_highlighter_get_instance_private (highlighter);
}
