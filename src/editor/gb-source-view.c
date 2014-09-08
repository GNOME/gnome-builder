/* gb-source-view.c
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "sourceview"

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>

#include "gb-animation.h"
#include "gb-box-theatric.h"
#include "gb-cairo.h"
#include "gb-editor-document.h"
#include "gb-log.h"
#include "gb-source-search-highlighter.h"
#include "gb-source-snippet-context.h"
#include "gb-source-snippet-private.h"
#include "gb-source-view.h"
#include "gb-widget.h"

struct _GbSourceViewPrivate
{
  GQueue                    *snippets;
  GbSourceSearchHighlighter *search_highlighter;
  GtkTextBuffer             *buffer;
  guint                      buffer_insert_text_handler;
  guint                      buffer_insert_text_after_handler;
  guint                      buffer_delete_range_handler;
  guint                      buffer_delete_range_after_handler;
  guint                      buffer_mark_set_handler;
  guint                      show_shadow : 1;
};

typedef void (*GbSourceViewMatchFunc) (GbSourceView      *view,
                                       const GtkTextIter *match_begin,
                                       const GtkTextIter *match_end,
                                       gpointer           user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceView, gb_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_SEARCH_HIGHLIGHTER,
  PROP_SHOW_SHADOW,
  LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
on_search_highlighter_changed (GbSourceSearchHighlighter *highlighter,
                               GbSourceView              *view)
{
  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));

  g_print ("%s()\n", G_STRFUNC);

  EXIT;
}

GbSourceSearchHighlighter *
gb_source_view_get_search_highlighter (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), NULL);

  return view->priv->search_highlighter;
}

void
gb_source_view_set_search_highlighter (GbSourceView              *view,
                                       GbSourceSearchHighlighter *highlighter)
{
  GbSourceViewPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (!highlighter ||
                    GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));

  priv = view->priv;

  if (priv->search_highlighter)
    {
      g_signal_handlers_disconnect_by_func (
        priv->search_highlighter,
        G_CALLBACK (on_search_highlighter_changed),
        view);
    }

  g_clear_object (&priv->search_highlighter);

  if (highlighter)
    {
      priv->search_highlighter = g_object_ref (highlighter);
      g_signal_connect (priv->search_highlighter,
                        "changed",
                        G_CALLBACK (on_search_highlighter_changed),
                        view);
    }

  priv->search_highlighter = highlighter ? g_object_ref (highlighter) : NULL;

  g_object_notify_by_pspec (G_OBJECT (view),
                            gParamSpecs[PROP_SEARCH_HIGHLIGHTER]);
}

static void
invalidate_window (GbSourceView *view)
{
  GdkWindow *window;

  g_assert (GB_IS_SOURCE_VIEW (view));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
                                     GTK_TEXT_WINDOW_WIDGET);

  if (window)
    {
      gdk_window_invalidate_rect (window, NULL, TRUE);
      gtk_widget_queue_draw (GTK_WIDGET (view));
    }
}

gboolean
gb_source_view_get_show_shadow (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);

  return view->priv->show_shadow;
}

void
gb_source_view_set_show_shadow (GbSourceView *view,
                                gboolean      show_shadow)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  view->priv->show_shadow = show_shadow;
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs[PROP_SHOW_SHADOW]);
  invalidate_window (view);
}

static gchar *
get_filename (GbSourceView *view)
{
  GtkTextBuffer *buffer;
  GFile *file;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  g_assert (GB_IS_EDITOR_DOCUMENT (buffer));

  file = gb_editor_document_get_file (GB_EDITOR_DOCUMENT (buffer));

  if (!file)
    return g_strdup ("");

  return g_file_get_basename (file);
}

static void
get_rect_for_iters (GtkTextView       *text_view,
                    const GtkTextIter *iter1,
                    const GtkTextIter *iter2,
                    GdkRectangle      *rect,
                    GtkTextWindowType  window_type)
{
  GdkRectangle area;
  GdkRectangle tmp;
  GtkTextIter iter;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter1);
  g_return_if_fail (iter2);
  g_return_if_fail (rect);

  gtk_text_view_get_iter_location (text_view, iter1, &area);

  gtk_text_iter_assign (&iter, iter1);

  do
    {
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);

      area.x = MIN (area.x, tmp.x);
      area.y = MIN (area.y, tmp.y);
      area.width = MAX (area.width, (tmp.x + tmp.width) - area.x);
      area.height = MAX (area.height, (tmp.y + tmp.height) - area.y);

      gtk_text_iter_forward_to_line_end (&iter);
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);

      area.x = MIN (area.x, tmp.x);
      area.y = MIN (area.y, tmp.y);
      area.width = MAX (area.width, (tmp.x + tmp.width) - area.x);
      area.height = MAX (area.height, (tmp.y + tmp.height) - area.y);

      if (!gtk_text_iter_forward_char (&iter))
        break;
    }
  while (gtk_text_iter_compare (&iter, iter2) <= 0);

  gtk_text_view_buffer_to_window_coords (text_view, window_type,
                                         area.x, area.y,
                                         &area.x, &area.y);

  *rect = area;
}

static void
gb_source_view_block_handlers (GbSourceView *view)
{
  GbSourceViewPrivate *priv = view->priv;

  if (priv->buffer)
    {
      g_signal_handler_block (priv->buffer,
                              priv->buffer_insert_text_handler);
      g_signal_handler_block (priv->buffer,
                              priv->buffer_insert_text_after_handler);
      g_signal_handler_block (priv->buffer,
                              priv->buffer_delete_range_handler);
      g_signal_handler_block (priv->buffer,
                              priv->buffer_delete_range_after_handler);
      g_signal_handler_block (priv->buffer,
                              priv->buffer_mark_set_handler);
    }
}

static void
gb_source_view_unblock_handlers (GbSourceView *view)
{
  GbSourceViewPrivate *priv = view->priv;

  if (priv->buffer)
    {
      g_signal_handler_unblock (priv->buffer,
                                priv->buffer_insert_text_handler);
      g_signal_handler_unblock (priv->buffer,
                                priv->buffer_insert_text_after_handler);
      g_signal_handler_unblock (priv->buffer,
                                priv->buffer_delete_range_handler);
      g_signal_handler_unblock (priv->buffer,
                                priv->buffer_delete_range_after_handler);
      g_signal_handler_unblock (priv->buffer,
                                priv->buffer_mark_set_handler);
    }
}

static void
gb_source_view_scroll_to_insert (GbSourceView *view)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view), mark, 0.0, FALSE, 0, 0);
}

void
gb_source_view_pop_snippet (GbSourceView *view)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  if ((snippet = g_queue_pop_head (priv->snippets)))
    {
      gb_source_snippet_finish (snippet);
      g_object_unref (snippet);
    }

  if ((snippet = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_unpause (snippet);

  invalidate_window (view);
}

void
gb_source_view_clear_snippets (GbSourceView *view)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  while (view->priv->snippets->length)
    gb_source_view_pop_snippet (view);
}

static void
animate_in (GbSourceView      *view,
            const GtkTextIter *begin,
            const GtkTextIter *end)
{
  GtkAllocation alloc;
  GdkRectangle rect;
  GbBoxTheatric *theatric;

  get_rect_for_iters (GTK_TEXT_VIEW (view), begin, end, &rect,
                      GTK_TEXT_WINDOW_WIDGET);

  gtk_widget_get_allocation (GTK_WIDGET (view), &alloc);
  rect.height = MIN (rect.height, alloc.height - rect.y);

  theatric = g_object_new (GB_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", rect.height,
                           "target", view,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

#if 0
  g_print ("Starting at %d,%d %dx%d\n",
           rect.x, rect.y, rect.width, rect.height);
#endif

#define X_GROW 50
#define Y_GROW 30

  gb_object_animate_full (theatric,
                          GB_ANIMATION_EASE_IN_CUBIC,
                          250,
                          gtk_widget_get_frame_clock (GTK_WIDGET (view)),
                          g_object_unref,
                          theatric,
                          "x", rect.x - X_GROW,
                          "width", rect.width + (X_GROW * 2),
                          "y", rect.y - Y_GROW,
                          "height", rect.height + (Y_GROW * 2),
                          "alpha", 0.0,
                          NULL);

#undef X_GROW
#undef Y_GROW
}

void
gb_source_view_push_snippet (GbSourceView    *view,
                             GbSourceSnippet *snippet)
{
  GbSourceSnippetContext *context;
  GbSourceViewPrivate *priv;
  GbSourceSnippet *previous;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  gboolean has_more_tab_stops;
  gboolean insert_spaces;
  gchar *name;
  guint tab_width;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));

  priv = view->priv;

  context = gb_source_snippet_get_context (snippet);

  name = get_filename (view);
  gb_source_snippet_context_add_variable (context, "filename", name);
  g_free (name);

  if ((previous = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_pause (previous);

  g_queue_push_head (priv->snippets, g_object_ref (snippet));

  insert_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (view));
  gb_source_snippet_context_set_use_spaces (context, insert_spaces);

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));
  gb_source_snippet_context_set_tab_width (context, tab_width);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  gb_source_view_block_handlers (view);
  has_more_tab_stops = gb_source_snippet_begin (snippet, buffer, &iter);
  gb_source_view_scroll_to_insert (view);
  gb_source_view_unblock_handlers (view);

  {
    GtkTextMark *mark_begin;
    GtkTextMark *mark_end;
    GtkTextIter begin;
    GtkTextIter end;

    mark_begin = gb_source_snippet_get_mark_begin (snippet);
    mark_end = gb_source_snippet_get_mark_end (snippet);

    gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
    gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

    /*
     * HACK: We need to let the GtkTextView catch up with us so that we can
     *       get a realistic area back for the location of the end iter.
     *       Without pumping the main loop, GtkTextView will clamp the
     *       result to the height of the insert line.
     */
    while (gtk_events_pending ())
      gtk_main_iteration ();

    animate_in (view, &begin, &end);
  }

  if (!has_more_tab_stops)
    gb_source_view_pop_snippet (view);

  invalidate_window (view);
}

static void
on_insert_text (GtkTextBuffer *buffer,
                GtkTextIter   *iter,
                gchar         *text,
                gint           len,
                gpointer       user_data)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = user_data;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  gb_source_view_block_handlers (view);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_before_insert_text (snippet, buffer, iter, text, len);

  gb_source_view_unblock_handlers (view);
}

static void
on_insert_text_after (GtkTextBuffer *buffer,
                      GtkTextIter   *iter,
                      gchar         *text,
                      gint           len,
                      gpointer       user_data)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = user_data;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  gb_source_view_block_handlers (view);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_after_insert_text (snippet, buffer, iter, text, len);

  gb_source_view_unblock_handlers (view);
}

static void
on_delete_range (GtkTextBuffer *buffer,
                 GtkTextIter   *begin,
                 GtkTextIter   *end,
                 gpointer       user_data)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = user_data;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  gb_source_view_block_handlers (view);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_before_delete_range (snippet, buffer, begin, end);

  gb_source_view_unblock_handlers (view);
}

static void
on_delete_range_after (GtkTextBuffer *buffer,
                       GtkTextIter   *begin,
                       GtkTextIter   *end,
                       gpointer       user_data)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = user_data;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  gb_source_view_block_handlers (view);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_after_delete_range (snippet, buffer, begin, end);

  gb_source_view_unblock_handlers (view);
}

static void
on_mark_set (GtkTextBuffer *buffer,
             GtkTextIter   *iter,
             GtkTextMark   *mark,
             gpointer       user_data)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = user_data;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (iter);
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));

  priv = view->priv;

  gb_source_view_block_handlers (view);

  if ((mark == gtk_text_buffer_get_insert (buffer)))
    {
again:
      if ((snippet = g_queue_peek_head (priv->snippets)))
        {
          if (!gb_source_snippet_insert_set (snippet, mark))
            {
              gb_source_view_pop_snippet (view);
              goto again;
            }
        }
    }

  gb_source_view_unblock_handlers (view);
}

static void
gb_source_view_notify_buffer (GObject    *object,
                              GParamSpec *pspec,
                              gpointer    user_data)
{
  GbSourceViewPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_VIEW (object));
  g_return_if_fail (pspec);
  g_return_if_fail (!g_strcmp0 (pspec->name, "buffer"));

  priv = GB_SOURCE_VIEW (object)->priv;

  if (priv->buffer)
    {
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_insert_text_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_insert_text_after_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_delete_range_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_delete_range_after_handler);
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_mark_set_handler);
      priv->buffer_insert_text_handler = 0;
      priv->buffer_insert_text_after_handler = 0;
      priv->buffer_delete_range_handler = 0;
      priv->buffer_delete_range_after_handler = 0;
      priv->buffer_mark_set_handler = 0;
      g_object_remove_weak_pointer (G_OBJECT (priv->buffer),
                                    (gpointer *) &priv->buffer);
      priv->buffer = NULL;
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (object));

  if (buffer)
    {
      priv->buffer = buffer;
      g_object_add_weak_pointer (G_OBJECT (buffer),
                                 (gpointer *) &priv->buffer);
      priv->buffer_insert_text_handler =
        g_signal_connect_object (buffer,
                                 "insert-text",
                                 G_CALLBACK (on_insert_text),
                                 object,
                                 0);
      priv->buffer_insert_text_after_handler =
        g_signal_connect_object (buffer,
                                 "insert-text",
                                 G_CALLBACK (on_insert_text_after),
                                 object,
                                 G_CONNECT_AFTER);
      priv->buffer_delete_range_handler =
        g_signal_connect_object (buffer,
                                 "delete-range",
                                 G_CALLBACK (on_delete_range),
                                 object,
                                 0);
      priv->buffer_delete_range_after_handler =
        g_signal_connect_object (buffer,
                                 "delete-range",
                                 G_CALLBACK (on_delete_range_after),
                                 object,
                                 G_CONNECT_AFTER);
      priv->buffer_mark_set_handler =
        g_signal_connect_object (buffer,
                                 "mark-set",
                                 G_CALLBACK (on_mark_set),
                                 object,
                                 0);
    }
}

static gboolean
gb_source_view_key_press_event (GtkWidget   *widget,
                                GdkEventKey *event)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = (GbSourceView *) widget;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);
  g_return_val_if_fail (event, FALSE);

  priv = view->priv;

  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      switch ((gint) event->keyval)
        {
        case GDK_KEY_Escape:
          gb_source_view_block_handlers (view);
          gb_source_view_pop_snippet (view);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        case GDK_KEY_Tab:
          gb_source_view_block_handlers (view);
          if (!gb_source_snippet_move_next (snippet))
            gb_source_view_pop_snippet (view);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        case GDK_KEY_ISO_Left_Tab:
          gb_source_view_block_handlers (view);
          gb_source_snippet_move_previous (snippet);
          gb_source_view_scroll_to_insert (view);
          gb_source_view_unblock_handlers (view);
          return TRUE;

        default:
          break;
        }
    }

  return GTK_WIDGET_CLASS (gb_source_view_parent_class)->key_press_event (widget, event);
}

static cairo_region_t *
region_create_bounds (GtkTextView       *text_view,
                      const GtkTextIter *begin,
                      const GtkTextIter *end)
{
  cairo_rectangle_int_t r;
  cairo_region_t *region;
  GtkAllocation alloc;
  GdkRectangle rect;
  GdkRectangle rect2;
  gint x = 0;

  gtk_widget_get_allocation (GTK_WIDGET (text_view), &alloc);

  gtk_text_view_get_iter_location (text_view, begin, &rect);
  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         rect.x, rect.y, &rect.x, &rect.y);

  gtk_text_view_get_iter_location (text_view, end, &rect2);
  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         rect2.x, rect2.y,
                                         &rect2.x, &rect2.y);

  gtk_text_view_buffer_to_window_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         0, 0, &x, NULL);

  if (rect.y == rect2.y)
    {
      r.x = rect.x;
      r.y = rect.y;
      r.width = rect2.x - rect.x;
      r.height = MAX (rect.height, rect2.height);
      return cairo_region_create_rectangle (&r);
    }

  region = cairo_region_create ();

  r.x = rect.x;
  r.y = rect.y;
  r.width = alloc.width;
  r.height = rect.height;
  /* gb_cairo_rounded_rectangle (cr, &r, 5, 5); */
  cairo_region_union_rectangle (region, &r);

  r.x = x;
  r.y = rect.y + rect.height;
  r.width = alloc.width;
  r.height = rect2.y - rect.y - rect.height;
  if (r.height > 0)
    /* gb_cairo_rounded_rectangle (cr, &r, 5, 5); */
    cairo_region_union_rectangle (region, &r);

  r.x = 0;
  r.y = rect2.y;
  r.width = rect2.x + rect2.width;
  r.height = rect2.height;
  /* gb_cairo_rounded_rectangle (cr, &r, 5, 5); */
  cairo_region_union_rectangle (region, &r);

  return region;
}

static void
draw_snippet_background (GbSourceView    *view,
                         cairo_t         *cr,
                         GbSourceSnippet *snippet,
                         gint             width)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextMark *mark_begin;
  GtkTextMark *mark_end;
  GdkRectangle r;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (cr);
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  mark_begin = gb_source_snippet_get_mark_begin (snippet);
  mark_end = gb_source_snippet_get_mark_end (snippet);

  if (!mark_begin || !mark_end)
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

  get_rect_for_iters (GTK_TEXT_VIEW (view), &begin, &end, &r,
                      GTK_TEXT_WINDOW_TEXT);

#if 0
  /*
   * This will draw the snippet highlight across the entire widget,
   * instead of just where the contents are.
   */
  r.x = 0;
  r.width = width;
#endif

  gb_cairo_rounded_rectangle (cr, &r, 5, 5);

  cairo_fill (cr);
}

static void
gb_source_view_draw_snippets_background (GbSourceView *view,
                                         cairo_t      *cr)
{
  static GdkRGBA rgba;
  static gboolean did_rgba;
  GbSourceSnippet *snippet;
  GtkTextView *text_view = GTK_TEXT_VIEW (view);
  GdkWindow *window;
  gint len;
  gint i;
  gint width;

  g_assert (GB_IS_SOURCE_VIEW (view));
  g_assert (cr);

  if (!did_rgba)
    {
      gdk_rgba_parse (&rgba, "#204a87");
      rgba.alpha = 0.1;
      did_rgba = TRUE;
    }

  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);
  width = gdk_window_get_width (window);

  gdk_cairo_set_source_rgba (cr, &rgba);

  len = view->priv->snippets->length;

  cairo_save (cr);

  for (i = 0; i < len; i++)
    {
      snippet = g_queue_peek_nth (view->priv->snippets, i);
      draw_snippet_background (view, cr, snippet, width - ((len - i) * 10));
    }

  cairo_restore (cr);
}

static void
gb_source_view_draw_snippet_chunks (GbSourceView    *view,
                                    GbSourceSnippet *snippet,
                                    cairo_t         *cr)
{
  GbSourceSnippetChunk *chunk;
  GdkRGBA rgba;
  guint n_chunks;
  guint i;
  gint tab_stop;
  gint current_stop;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));
  g_return_if_fail (cr);

  cairo_save (cr);

  gdk_rgba_parse (&rgba, "#fcaf3e");

  n_chunks = gb_source_snippet_get_n_chunks (snippet);
  current_stop = gb_source_snippet_get_tab_stop (snippet);

  for (i = 0; i < n_chunks; i++)
    {
      chunk = gb_source_snippet_get_nth_chunk (snippet, i);
      tab_stop = gb_source_snippet_chunk_get_tab_stop (chunk);

      if (tab_stop > 0)
        {
          GtkTextIter begin;
          GtkTextIter end;
          cairo_region_t *region;

          rgba.alpha = (tab_stop == current_stop) ? 0.7 : 0.3;
          gdk_cairo_set_source_rgba (cr, &rgba);

          gb_source_snippet_get_chunk_range (snippet, chunk, &begin, &end);

          region = region_create_bounds (GTK_TEXT_VIEW (view), &begin, &end);
          gdk_cairo_region (cr, region);
          cairo_region_destroy (region);

          cairo_fill (cr);
        }
    }

  cairo_restore (cr);
}

static void
gb_source_view_draw_layer (GtkTextView     *text_view,
                           GtkTextViewLayer layer,
                           cairo_t         *cr)
{
  GbSourceViewPrivate *priv = GB_SOURCE_VIEW (text_view)->priv;

  GTK_TEXT_VIEW_CLASS (gb_source_view_parent_class)->draw_layer (text_view, layer, cr);

  if (layer == GTK_TEXT_VIEW_LAYER_BELOW)
    {
      if (priv->snippets->length)
        {
          GbSourceSnippet *snippet = g_queue_peek_head (priv->snippets);

          gb_source_view_draw_snippets_background (GB_SOURCE_VIEW (text_view),
                                                   cr);
          gb_source_view_draw_snippet_chunks (GB_SOURCE_VIEW (text_view),
                                              snippet, cr);
        }
    }
  else if (layer == GTK_TEXT_VIEW_LAYER_ABOVE)
    {
      if (priv->show_shadow && priv->search_highlighter)
        {
          cairo_save (cr);
          gb_source_search_highlighter_draw (priv->search_highlighter, cr);
          cairo_restore (cr);
        }
    }
}

static void
gb_source_view_grab_focus (GtkWidget *widget)
{
  invalidate_window (GB_SOURCE_VIEW (widget));

  GTK_WIDGET_CLASS (gb_source_view_parent_class)->grab_focus (widget);
}

static void
gb_source_view_finalize (GObject *object)
{
  GbSourceViewPrivate *priv;

  priv = GB_SOURCE_VIEW (object)->priv;

  g_clear_pointer (&priv->snippets, g_queue_free);
  g_clear_object (&priv->search_highlighter);

  G_OBJECT_CLASS (gb_source_view_parent_class)->finalize (object);
}

static void
gb_source_view_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GbSourceView *view = GB_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_HIGHLIGHTER:
      g_value_set_object (value, gb_source_view_get_search_highlighter (view));
      break;

    case PROP_SHOW_SHADOW:
      g_value_set_boolean (value, gb_source_view_get_show_shadow (view));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_view_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GbSourceView *view = GB_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_SEARCH_HIGHLIGHTER:
      gb_source_view_set_search_highlighter (view, g_value_get_object (value));
      break;

    case PROP_SHOW_SHADOW:
      gb_source_view_set_show_shadow (view, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_view_class_init (GbSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);

  object_class->finalize = gb_source_view_finalize;
  object_class->get_property = gb_source_view_get_property;
  object_class->set_property = gb_source_view_set_property;

  widget_class->grab_focus = gb_source_view_grab_focus;
  widget_class->key_press_event = gb_source_view_key_press_event;

  text_view_class->draw_layer = gb_source_view_draw_layer;

  gParamSpecs[PROP_SHOW_SHADOW] =
    g_param_spec_boolean ("show-shadow",
                          _ ("Show Shadow"),
                          _ ("Show the search shadow"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_SHADOW,
                                   gParamSpecs[PROP_SHOW_SHADOW]);

  gParamSpecs[PROP_SEARCH_HIGHLIGHTER] =
    g_param_spec_object ("search-highlighter",
                         _ ("Search Highlighter"),
                         _ ("Search Highlighter"),
                         GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_HIGHLIGHTER,
                                   gParamSpecs[PROP_SEARCH_HIGHLIGHTER]);
}

static void
gb_source_view_init (GbSourceView *view)
{
  view->priv = gb_source_view_get_instance_private (view);

  view->priv->snippets = g_queue_new ();

  g_signal_connect (view,
                    "notify::buffer",
                    G_CALLBACK (gb_source_view_notify_buffer),
                    NULL);
}
