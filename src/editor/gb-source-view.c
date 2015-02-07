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
#include <gtksourceview/completion-providers/words/gtksourcecompletionwords.h>

#include "gb-animation.h"
#include "gb-source-auto-indenter.h"
#include "gb-source-auto-indenter-c.h"
#include "gb-source-auto-indenter-python.h"
#include "gb-source-auto-indenter-xml.h"
#include "gb-box-theatric.h"
#include "gb-cairo.h"
#include "gb-dnd.h"
#include "gb-editor-document.h"
#include "gb-gtk.h"
#include "gb-html-completion-provider.h"
#include "gb-log.h"
#include "gb-pango.h"
#include "gb-source-auto-indenter.h"
#include "gb-source-search-highlighter.h"
#include "gb-source-snippets.h"
#include "gb-source-snippets-manager.h"
#include "gb-source-snippet-completion-provider.h"
#include "gb-source-snippet-context.h"
#include "gb-source-snippet-private.h"
#include "gb-source-view.h"
#include "gb-source-vim.h"
#include "gb-source-emacs.h" 
#include "gb-widget.h"

enum {
  TARGET_URI_LIST = 100
};

struct _GbSourceViewPrivate
{
  GQueue                      *snippets;
  GbSourceSearchHighlighter   *search_highlighter;
  GtkTextBuffer               *buffer;
  GbSourceAutoIndenter        *auto_indenter;
  GtkSourceCompletionProvider *html_provider;
  GtkSourceCompletionProvider *snippets_provider;
  GtkSourceCompletionProvider *words_provider;
  GbSourceVim                 *vim;
  GbSourceEmacs               *emacs;
  GtkCssProvider              *css_provider;

  GSettings                   *language_settings;
  GSettings                   *editor_settings;

  guint                        buffer_insert_text_handler;
  guint                        buffer_insert_text_after_handler;
  guint                        buffer_delete_range_handler;
  guint                        buffer_delete_range_after_handler;
  guint                        buffer_mark_set_handler;
  guint                        buffer_notify_language_handler;

  gint                         saved_line;
  gint                         saved_line_offset;

  guint                        auto_indent : 1;
  guint                        enable_word_completion : 1;
  guint                        insert_matching_brace : 1;
  guint                        show_shadow : 1;
  guint                        overwrite_braces : 1;
};

typedef void (*GbSourceViewMatchFunc) (GbSourceView      *view,
                                       const GtkTextIter *match_begin,
                                       const GtkTextIter *match_end,
                                       gpointer           user_data);

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceView, gb_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_ENABLE_WORD_COMPLETION,
  PROP_FONT_NAME,
  PROP_INSERT_MATCHING_BRACE,
  PROP_OVERWRITE_BRACES,
  PROP_SEARCH_HIGHLIGHTER,
  PROP_SHOW_SHADOW,
  PROP_SMART_HOME_END_SIMPLE,
  PROP_SHOW_GRID_LINES,
  LAST_PROP
};

enum {
  BEGIN_SEARCH,
  DISPLAY_DOCUMENTATION,
  DRAW_LAYER,
  POP_SNIPPET,
  PUSH_SNIPPET,
  REQUEST_DOCUMENTATION,
  DROP_URIS,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GbSourceVim *
gb_source_view_get_vim (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), NULL);

  return view->priv->vim;
}

GbSourceEmacs *
gb_source_view_get_emacs (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), NULL);

  return view->priv->emacs;
}

gboolean
gb_source_view_get_insert_matching_brace (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);

  return view->priv->insert_matching_brace;
}

void
gb_source_view_set_insert_matching_brace (GbSourceView *view,
                                          gboolean      insert_matching_brace)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  if (view->priv->insert_matching_brace != insert_matching_brace)
    {
      view->priv->insert_matching_brace = !!insert_matching_brace;
      g_object_notify_by_pspec (G_OBJECT (view),
                                gParamSpecs [PROP_INSERT_MATCHING_BRACE]);
    }
}

gboolean
gb_source_view_get_overwrite_braces (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);

  return view->priv->overwrite_braces;
}

void
gb_source_view_set_overwrite_braces (GbSourceView *view,
                                     gboolean      overwrite_braces)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  if (view->priv->overwrite_braces != overwrite_braces)
    {
      view->priv->overwrite_braces = !!overwrite_braces;
      g_object_notify_by_pspec (G_OBJECT (view),
                                gParamSpecs [PROP_OVERWRITE_BRACES]);
    }
}

gboolean
gb_source_view_get_enable_word_completion (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);

  return view->priv->enable_word_completion;
}

void
gb_source_view_set_enable_word_completion (GbSourceView *view,
                                           gboolean      enable_word_completion)
{
  GbSourceViewPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  if (enable_word_completion != priv->enable_word_completion)
    {
      GtkSourceCompletion *completion;

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));

      if (!enable_word_completion)
        gtk_source_completion_remove_provider (
            completion,
            GTK_SOURCE_COMPLETION_PROVIDER (priv->words_provider),
            NULL);
      else
        gtk_source_completion_add_provider (
            completion,
            GTK_SOURCE_COMPLETION_PROVIDER (priv->words_provider),
            NULL);

      priv->enable_word_completion = !!enable_word_completion;
      g_object_notify_by_pspec (G_OBJECT (view),
                                gParamSpecs [PROP_ENABLE_WORD_COMPLETION]);
    }
}

static void
gb_source_view_disconnect_settings (GbSourceView *view)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  if (!GB_IS_EDITOR_DOCUMENT (buffer))
    return;

  g_settings_unbind (buffer, "show-grid-lines");
  g_settings_unbind (buffer, "highlight-matching-brackets");
  g_settings_unbind (buffer, "style-scheme-name");

  g_settings_unbind (view, "auto-indent");
  g_settings_unbind (view, "highlight-current-line");
  g_settings_unbind (view, "indent-width");
  g_settings_unbind (view, "insert-spaces-instead-of-tabs");
  g_settings_unbind (view, "right-margin-position");
  g_settings_unbind (view, "show-line-numbers");
  g_settings_unbind (view, "show-right-margin");
  g_settings_unbind (view, "tab-width");
  g_settings_unbind (view, "font-name");
  g_settings_unbind (view->priv->vim, "enabled");
  g_settings_unbind (view->priv->emacs, "enabled");

  g_clear_object (&view->priv->language_settings);
  g_clear_object (&view->priv->editor_settings);
}

static void
gb_source_view_connect_settings (GbSourceView *view)
{
  GtkTextBuffer *buffer;
  GtkSourceLanguage *language;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  if (!GB_IS_EDITOR_DOCUMENT (buffer))
    return;

  if (language)
    {
      GSettings *settings;
      gchar *path;

      path = g_strdup_printf ("/org/gnome/builder/editor/language/%s/",
                              gtk_source_language_get_id (language));
      settings = g_settings_new_with_path ("org.gnome.builder.editor.language",
                                           path);
      g_free (path);

      g_settings_bind (settings, "auto-indent", view, "auto-indent",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "indent-width", view, "indent-width",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "insert-matching-brace",
                       view, "insert-matching-brace",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "insert-spaces-instead-of-tabs",
                       view, "insert-spaces-instead-of-tabs",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "overwrite-braces",
                       view, "overwrite-braces",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "right-margin-position",
                       view, "right-margin-position",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "show-right-margin",
                       view, "show-right-margin",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "tab-width", view, "tab-width",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "trim-trailing-whitespace",
                       buffer, "trim-trailing-whitespace",
                       G_SETTINGS_BIND_GET);

      view->priv->language_settings = settings;
    }

  view->priv->editor_settings = g_settings_new ("org.gnome.builder.editor");

  g_settings_bind (view->priv->editor_settings, "font-name",
                   view, "font-name", G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "style-scheme-name",
                   buffer, "style-scheme-name", G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "vim-mode",
                   view->priv->vim, "enabled", G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "emacs-mode",
                   view->priv->emacs, "enabled", G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "word-completion",
                   view, "enable-word-completion", G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "show-line-numbers",
                   view, "show-line-numbers",G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "highlight-current-line",
                   view, "highlight-current-line",G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "highlight-matching-brackets",
                   buffer, "highlight-matching-brackets",G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "smart-home-end",
                   view, "smart-home-end-simple",G_SETTINGS_BIND_GET);
  g_settings_bind (view->priv->editor_settings, "show-grid-lines",
                   view, "show-grid-lines",G_SETTINGS_BIND_GET);
}

void
gb_source_view_begin_search (GbSourceView     *view,
                             GtkDirectionType  direction,
                             const gchar      *search_text)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  g_signal_emit (view, gSignals [BEGIN_SEARCH], 0, direction, search_text);
}

static void
gb_source_view_vim_begin_search (GbSourceView     *view,
                                 GtkDirectionType  direction,
                                 const gchar      *text,
                                 GbSourceVim      *vim)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  gb_source_view_begin_search (view, direction, text);
}

static void
gb_source_view_vim_jump_to_doc (GbSourceView *view,
                                const gchar  *text,
                                GbSourceVim  *vim)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (text)
    g_signal_emit (view, gSignals [DISPLAY_DOCUMENTATION], 0, text);
}

static void
on_search_highlighter_changed (GbSourceSearchHighlighter *highlighter,
                               GbSourceView              *view)
{
  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_SEARCH_HIGHLIGHTER (highlighter));

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

  view->priv->show_shadow = !!show_shadow;
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs[PROP_SHOW_SHADOW]);
  invalidate_window (view);
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
      gdk_rectangle_union (&area, &tmp, &area);

      /* ive seen a crash on the following line due to invalid textiter.
       * none of these functions appear to me to be doing modifications,
       * so im a bit perplexed. maybe stack corruption?
       */
      gtk_text_iter_forward_to_line_end (&iter);
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);

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
  GtkTextIter iter;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  gb_gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (view), &iter,
                                   0.0, FALSE, 0.0, 0.0);
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
      g_signal_emit (view, gSignals [POP_SNIPPET], 0, snippet);
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

static void
gb_source_view_invalidate_range_mark (GbSourceView *view,
                                      GtkTextMark  *mark_begin,
                                      GtkTextMark  *mark_end)
{
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GtkTextIter begin;
  GtkTextIter end;
  GdkWindow *window;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark_begin));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark_end));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

  gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

  get_rect_for_iters (GTK_TEXT_VIEW (view), &begin, &end, &rect,
                      GTK_TEXT_WINDOW_TEXT);

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
                                     GTK_TEXT_WINDOW_TEXT);
  gdk_window_invalidate_rect (window, &rect, FALSE);
}

static gchar *
gb_source_view_get_line_prefix (GbSourceView      *view,
                                const GtkTextIter *iter)
{
  GtkTextIter begin;
  GString *str;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), NULL);
  g_return_val_if_fail (iter, NULL);

  gtk_text_iter_assign (&begin, iter);
  gtk_text_iter_set_line_offset (&begin, 0);

  str = g_string_new (NULL);

  if (gtk_text_iter_compare (&begin, iter) != 0)
    {
      do
        {
          gunichar c;

          c = gtk_text_iter_get_char (&begin);

          switch (c)
            {
            case '\t':
            case ' ':
              g_string_append_unichar (str, c);
              break;
            default:
              g_string_append_c (str, ' ');
              break;
            }
        }
      while (gtk_text_iter_forward_char (&begin) &&
             (gtk_text_iter_compare (&begin, iter) < 0));
    }

  return g_string_free (str, FALSE);
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
  gchar *line_prefix;
  guint tab_width;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));

  priv = view->priv;

  context = gb_source_snippet_get_context (snippet);

  if ((previous = g_queue_peek_head (priv->snippets)))
    gb_source_snippet_pause (previous);

  g_queue_push_head (priv->snippets, g_object_ref (snippet));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  insert_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (view));
  gb_source_snippet_context_set_use_spaces (context, insert_spaces);

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (view));
  gb_source_snippet_context_set_tab_width (context, tab_width);

  line_prefix = gb_source_view_get_line_prefix (view, &iter);
  gb_source_snippet_context_set_line_prefix (context, line_prefix);
  g_free (line_prefix);

  g_signal_emit (view, gSignals [PUSH_SNIPPET], 0,
                 snippet, context, &iter);

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

  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      GtkTextMark *begin;
      GtkTextMark *end;

      gb_source_view_block_handlers (view);
      gb_source_snippet_after_insert_text (snippet, buffer, iter, text, len);
      gb_source_view_unblock_handlers (view);

      begin = gb_source_snippet_get_mark_begin (snippet);
      end = gb_source_snippet_get_mark_end (snippet);
      gb_source_view_invalidate_range_mark (view, begin, end);
    }
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

  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      GtkTextMark *begin_mark;
      GtkTextMark *end_mark;

      gb_source_view_block_handlers (view);
      gb_source_snippet_before_delete_range (snippet, buffer, begin, end);
      gb_source_view_unblock_handlers (view);

      begin_mark = gb_source_snippet_get_mark_begin (snippet);
      end_mark = gb_source_snippet_get_mark_end (snippet);
      gb_source_view_invalidate_range_mark (view, begin_mark, end_mark);
    }
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

  if (mark == gtk_text_buffer_get_insert (buffer))
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
gb_source_view_reload_snippets (GbSourceView *source_view)
{
  GbSourceSnippetsManager *mgr;
  GbSourceSnippets *snippets = NULL;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  if (language)
    {
      mgr = gb_source_snippets_manager_get_default ();
      snippets = gb_source_snippets_manager_get_for_language (mgr, language);
    }

  g_object_set (source_view->priv->snippets_provider,
                "snippets", snippets,
                NULL);
}

static void
gb_source_view_reload_auto_indenter (GbSourceView *view)
{
  GbSourceAutoIndenter *auto_indenter = NULL;
  GtkTextBuffer *buffer;
  GtkSourceLanguage *language;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));

  /*
   * Disable default auto indenter.
   */
  gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (view), FALSE);

  if (language)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (language);

      if (g_str_equal (lang_id, "c") || g_str_equal (lang_id, "chdr"))
        auto_indenter = gb_source_auto_indenter_c_new ();
      else if (g_str_equal (lang_id, "python") ||
               g_str_equal (lang_id, "python3"))
        auto_indenter = gb_source_auto_indenter_python_new ();
      else if (g_str_equal (lang_id, "xml") || g_str_equal (lang_id, "html"))
        auto_indenter = gb_source_auto_indenter_xml_new ();
    }

  g_clear_object (&view->priv->auto_indenter);

  view->priv->auto_indenter = auto_indenter;

  /*
   * Fallback to built in auto indenter if necessary.
   */
  if (view->priv->auto_indent && !view->priv->auto_indenter)
    gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (view), TRUE);
}

static void
gb_source_view_reload_providers (GbSourceView *view)
{
  GtkSourceCompletion *completion;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  const gchar *lang_id = NULL;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));

  if (language)
    lang_id = gtk_source_language_get_id (language);

  if (view->priv->html_provider)
    {
      gtk_source_completion_remove_provider (completion,
                                             view->priv->html_provider,
                                             NULL);
      g_clear_object (&view->priv->html_provider);
    }

  if (g_strcmp0 (lang_id, "html") == 0)
    {
      view->priv->html_provider = gb_html_completion_provider_new ();
      gtk_source_completion_add_provider (completion,
                                          view->priv->html_provider,
                                          NULL);
    }

  gb_source_view_reload_snippets (view);
}

static void
on_language_set (GtkSourceBuffer *buffer,
                 GParamSpec      *pspec,
                 GbSourceView    *view)
{
  g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  gb_source_view_disconnect_settings (view);
  gb_source_view_reload_auto_indenter (view);
  gb_source_view_reload_providers (view);
  gb_source_view_connect_settings (view);
}

static void
gb_source_view_notify_buffer (GObject    *object,
                              GParamSpec *pspec,
                              gpointer    user_data)
{
  GbSourceViewPrivate *priv;
  GtkTextBuffer *buffer;
  GbSourceView *view = (GbSourceView *)object;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (pspec);
  g_return_if_fail (!g_strcmp0 (pspec->name, "buffer"));

  priv = view->priv;

  if (priv->buffer)
    {
      gb_source_view_disconnect_settings (view);
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
      g_signal_handler_disconnect (priv->buffer,
                                   priv->buffer_notify_language_handler);
      priv->buffer_insert_text_handler = 0;
      priv->buffer_insert_text_after_handler = 0;
      priv->buffer_delete_range_handler = 0;
      priv->buffer_delete_range_after_handler = 0;
      priv->buffer_mark_set_handler = 0;
      priv->buffer_notify_language_handler = 0;
      gtk_source_completion_words_unregister (
          GTK_SOURCE_COMPLETION_WORDS (priv->words_provider),
          GTK_TEXT_BUFFER (priv->buffer));
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
      priv->buffer_notify_language_handler =
        g_signal_connect_object (buffer,
                                 "notify::language",
                                 G_CALLBACK (on_language_set),
                                 object,
                                 0);

      gtk_source_completion_words_register (
          GTK_SOURCE_COMPLETION_WORDS (priv->words_provider),
          GTK_TEXT_BUFFER (buffer));

      gb_source_view_reload_auto_indenter (view);
      gb_source_view_reload_providers (view);

      gb_source_view_connect_settings (view);
    }
}

static void
gb_source_view_maybe_overwrite (GbSourceView *view,
                                GdkEventKey  *event)
{
  GtkTextMark *mark;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gunichar ch;
  gunichar prev_ch;
  gboolean ignore = FALSE;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (event);

  /*
   * Some auto-indenters will perform triggers on certain key-press that we
   * would hijack by otherwise "doing nothing" during this key-press. So to
   * avoid that, we actually delete the previous value and then allow this
   * key-press event to continue.
   */

  if (!view->priv->overwrite_braces)
    return;

  /*
   * WORKAROUND:
   *
   * If we are inside of a snippet, then let's not do anything. It really
   * messes with the position tracking. Once we can better integrate these
   * things, go ahead and remove this.
   */
  if (view->priv->snippets->length)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ch = gtk_text_iter_get_char (&iter);
  prev_ch = gb_gtk_text_iter_get_previous_char (&iter);

  switch (event->keyval)
    {
    case GDK_KEY_parenright:
      ignore = (ch == ')');
      break;

    case GDK_KEY_bracketright:
      ignore = (ch == ']');
      break;

    case GDK_KEY_braceright:
      ignore = (ch == '}');
      break;

    case GDK_KEY_quotedbl:
      ignore = (ch == '"') && (prev_ch != '\\');
      break;

    case GDK_KEY_quoteleft:
    case GDK_KEY_quoteright:
      ignore = (ch == '\'');
      break;

    default:
      break;
    }

  if (ignore && !gtk_text_buffer_get_has_selection (buffer))
    {
      GtkTextIter next = iter;

      if (!gtk_text_iter_forward_char (&next))
        gtk_text_buffer_get_end_iter (buffer, &next);

      gtk_text_buffer_select_range (buffer, &iter, &next);
    }
}

static gboolean
is_closing_char (gunichar ch)
{
  switch (ch)
    {
    case '}':
    case ')':
    case '"':
    case '\'':
    case ']':
      return TRUE;

    default:
      return FALSE;
    }
}

static guint
count_chars_on_line (GbSourceView      *view,
                     gunichar           expected_char,
                     const GtkTextIter *iter)
{
  GtkTextIter cur;
  guint count = 0;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), 0);
  g_return_val_if_fail (iter, 0);

  cur = *iter;

  gtk_text_iter_set_line_offset (&cur, 0);

  while (!gtk_text_iter_ends_line (&cur))
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&cur);

      if (ch == '\\')
        {
          gtk_text_iter_forward_chars (&cur, 2);
          continue;
        }

      count += (ch == expected_char);
      gtk_text_iter_forward_char (&cur);
    }

  return count;
}

static gboolean
gb_source_view_maybe_insert_match (GbSourceView *view,
                                   GdkEventKey  *event)
{
  GtkSourceBuffer *sbuf;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter prev_iter;
  GtkTextIter next_iter;
  gunichar next_ch = 0;
  gchar ch = 0;

  /*
   * TODO: I think we should put this into a base class for auto
   *       indenters. It would make some things a lot more convenient, like
   *       changing which characters we won't add matching characters for.
   */

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);
  g_return_val_if_fail (event, FALSE);

  /*
   * If we are disabled, then do nothing.
   */
  if (!view->priv->insert_matching_brace)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  sbuf = GTK_SOURCE_BUFFER (buffer);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  next_ch = gtk_text_iter_get_char (&iter);

  prev_iter = iter;
  gtk_text_iter_backward_chars (&prev_iter, 2);

  /*
   * If the source language has marked this region as a string or comment,
   * then do nothing.
   */
  if (gtk_source_buffer_iter_has_context_class (sbuf, &prev_iter, "string") ||
      gtk_source_buffer_iter_has_context_class (sbuf, &prev_iter, "comment"))
    return FALSE;

  switch (event->keyval)
    {
    case GDK_KEY_braceleft:
      ch = '}';
      break;

    case GDK_KEY_parenleft:
      ch = ')';
      break;

    case GDK_KEY_bracketleft:
      ch = ']';
      break;

    case GDK_KEY_quotedbl:
      ch = '"';
      break;

#if 0
    /*
     * TODO: We should avoid this when we are in comments, etc. That will
     *       require some communication with the syntax engine.
     */
    case GDK_KEY_quoteleft:
    case GDK_KEY_quoteright:
      ch = '\'';
      break;
#endif

    default:
      return FALSE;
    }

  /*
   * Insert the match if one of the following is true:
   *
   *  - We are at EOF
   *  - The next character is whitespace
   *  - The next character is a closing brace.
   *  - If the char is ", then there must be an even number already on
   *    the current line.
   */

  next_iter = iter;
  if (gtk_text_iter_forward_char (&next_iter))
    next_ch = gtk_text_iter_get_char (&next_iter);

  if (!next_ch || g_unichar_isspace (next_ch) || is_closing_char (next_ch))
    {
      /*
       * Special case for working with double quotes.
       *
       * Ignore double quote if we just added enough to make there be an
       * even number on this line. However, if it was the first quote on
       * the line, we still need to include a second.
       */
      if (ch == '"')
        {
          guint count;

          count = count_chars_on_line (view, '"', &iter);
          if ((count > 1) && ((count % 2) == 0))
            return FALSE;
        }

      gtk_text_buffer_insert_at_cursor (buffer, &ch, 1);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      gtk_text_iter_backward_char (&iter);
      gtk_text_buffer_select_range (buffer, &iter, &iter);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gb_source_view_maybe_delete_match (GbSourceView *view,
                                   GdkEventKey  *event)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter prev;
  gunichar ch;
  gunichar match;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (event->keyval == GDK_KEY_BackSpace, FALSE);

  if (!view->priv->insert_matching_brace)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  prev = iter;
  if (!gtk_text_iter_backward_char (&prev))
    return FALSE;

  ch = gtk_text_iter_get_char (&prev);

  switch (ch)
    {
    case '[':  match = ']';  break;
    case '{':  match = '}';  break;
    case '(':  match = ')';  break;
    case '"':  match = '"';  break;
    case '\'': match = '\''; break;
    default:   match = 0;    break;
    }

  if (gtk_text_iter_get_char (&iter) == match)
    {
      gtk_text_iter_forward_char (&iter);
      gtk_text_buffer_delete (buffer, &prev, &iter);

      return TRUE;
    }

  return FALSE;
}

static gboolean
gb_source_view_key_press_event (GtkWidget   *widget,
                                GdkEventKey *event)
{
  GbSourceViewPrivate *priv;
  GbSourceSnippet *snippet;
  GbSourceView *view = (GbSourceView *) widget;
  gboolean ret;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), FALSE);
  g_return_val_if_fail (event, FALSE);

  priv = view->priv;

  /*
   * Handle movement through the tab stops of the current snippet if needed.
   */
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

        case GDK_KEY_KP_Tab:
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

  /*
   * Allow the Input Method Context to potentially filter this keystroke.
   */
  if ((event->keyval == GDK_KEY_Return) || (event->keyval == GDK_KEY_KP_Enter))
    if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (view), event))
      return TRUE;

  /*
   * If we are going to insert the same character as the next character in the
   * buffer, we may want to remove it first. This allows us to still trigger
   * the auto-indent engine (instead of just short-circuiting the key-press).
   */
  gb_source_view_maybe_overwrite (view, event);

  /*
   * If we are backspacing, and the next character is the matching brace,
   * then we might want to delete it too.
   */
  if (event->keyval == GDK_KEY_BackSpace)
    if (gb_source_view_maybe_delete_match (view, event))
      return TRUE;

  /*
   * If we have an auto-indenter and the event is for a trigger key, then we
   * chain up to the parent class to insert the character, and then let the
   * auto-indenter fix things up.
   */
  if (priv->auto_indent &&
      priv->auto_indenter &&
      gb_source_auto_indenter_is_trigger (priv->auto_indenter, event))
    {
      GtkTextMark *insert;
      GtkTextIter begin;
      GtkTextIter end;
      gchar *indent;
      gint cursor_offset = 0;

      /*
       * Insert into the buffer so the auto-indenter can see it. If
       * GtkSourceView:auto-indent is set, then we will end up with very
       * unpredictable results.
       */
      GTK_WIDGET_CLASS (gb_source_view_parent_class)->key_press_event (widget, event);

      /*
       * Set begin and end to the position of the new insertion point.
       */
      insert = gtk_text_buffer_get_insert (priv->buffer);
      gtk_text_buffer_get_iter_at_mark (priv->buffer, &begin, insert);
      gtk_text_buffer_get_iter_at_mark (priv->buffer, &end, insert);

      /*
       * Let the formatter potentially set the replacement text.
       */
      indent = gb_source_auto_indenter_format (priv->auto_indenter,
                                               GTK_TEXT_VIEW (view),
                                               priv->buffer, &begin, &end,
                                               &cursor_offset, event);

      if (indent)
        {
          /*
           * Insert the indention text.
           */
          gtk_text_buffer_begin_user_action (priv->buffer);
          if (!gtk_text_iter_equal (&begin, &end))
            gtk_text_buffer_delete (priv->buffer, &begin, &end);
          gtk_text_buffer_insert (priv->buffer, &begin, indent, -1);
          g_free (indent);
          gtk_text_buffer_end_user_action (priv->buffer);

          /*
           * Place the cursor, as it could be somewhere within our indent text.
           */
          gtk_text_buffer_get_iter_at_mark (priv->buffer, &begin, insert);
          if (cursor_offset > 0)
            gtk_text_iter_forward_chars (&begin, cursor_offset);
          else if (cursor_offset < 0)
            gtk_text_iter_backward_chars (&begin, ABS (cursor_offset));
          gtk_text_buffer_select_range (priv->buffer, &begin, &begin);
        }

      return TRUE;
    }

  ret = GTK_WIDGET_CLASS (gb_source_view_parent_class)->key_press_event (widget, event);

  if (ret)
    gb_source_view_maybe_insert_match (view, event);

  return ret;
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
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (cr);

  g_signal_emit (text_view, gSignals [DRAW_LAYER], 0, layer, cr);
}

static void
gb_source_view_real_draw_layer (GbSourceView     *view,
                                GtkTextViewLayer  layer,
                                cairo_t          *cr)
{
  GbSourceViewPrivate *priv = view->priv;
  GtkTextView *text_view = GTK_TEXT_VIEW (view);

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
          gb_source_search_highlighter_draw (priv->search_highlighter,
                                             text_view, cr);
          cairo_restore (cr);
        }
    }
}

static void
gb_source_view_display_documentation (GbSourceView *view,
                                      const gchar  *search_text)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (search_text);

}

static void
gb_source_view_request_documentation (GbSourceView *view)
{
  GtkTextIter begin;
  GtkTextIter end;
  gchar *word;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  word = gb_source_vim_get_current_word (view->priv->vim, &begin, &end);

  if (word)
    {
      g_signal_emit (view, gSignals [DISPLAY_DOCUMENTATION], 0, word);
      g_free (word);
    }

  EXIT;
}

static void
gb_source_view_grab_focus (GtkWidget *widget)
{
  invalidate_window (GB_SOURCE_VIEW (widget));

  GTK_WIDGET_CLASS (gb_source_view_parent_class)->grab_focus (widget);
}

void
gb_source_view_set_font_name (GbSourceView *view,
                              const gchar  *font_name)
{
  PangoFontDescription *font_desc;
  GdkWindow *window;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  font_desc = pango_font_description_from_string (font_name);

  if (font_desc)
    {
      gchar *str;
      gchar *css;

      str = gb_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("GbSourceView { %s }", str);
      gtk_css_provider_load_from_data (view->priv->css_provider, css, -1, NULL);
      pango_font_description_free (font_desc);

      g_free (css);
      g_free (str);
    }
  else
    gtk_css_provider_load_from_data (view->priv->css_provider, "", -1, NULL);

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
                                     GTK_TEXT_WINDOW_WIDGET);
  if (window)
    gdk_window_invalidate_rect (window, NULL, TRUE);
}

GbSourceAutoIndenter *
gb_source_view_get_auto_indenter (GbSourceView *view)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (view), NULL);

  if (view->priv->auto_indent)
    return view->priv->auto_indenter;

  return NULL;
}

static void
gb_source_view_constructed (GObject *object)
{
  GbSourceViewPrivate *priv;
  GtkSourceCompletion *completion;
  GbSourceView *source_view = (GbSourceView *)object;
  GtkStyleContext *context;

  priv = source_view->priv;

  G_OBJECT_CLASS (gb_source_view_parent_class)->constructed (object);

  context = gtk_widget_get_style_context (GTK_WIDGET (object));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (priv->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (object));
  gtk_source_completion_add_provider (completion,
                                      source_view->priv->snippets_provider,
                                      NULL);
}

static void
gb_source_view_drag_data_received (GtkWidget        *widget,
                                   GdkDragContext   *context,
                                   gint              x,
                                   gint              y,
                                   GtkSelectionData *selection_data,
                                   guint             info,
                                   guint             timestamp)
{
  gchar **uri_list;

  g_return_if_fail (GB_IS_SOURCE_VIEW (widget));

  switch (info)
    {
    case TARGET_URI_LIST:
      uri_list = gb_dnd_get_uri_list (selection_data);

      if (uri_list)
        {
          g_signal_emit (widget, gSignals [DROP_URIS], 0, uri_list);
          g_strfreev (uri_list);
        }

      gtk_drag_finish (context, TRUE, FALSE, timestamp);
      break;

    default:
      GTK_WIDGET_CLASS (gb_source_view_parent_class)->drag_data_received (widget,
                                                                          context,
                                                                          x, y,
                                                                          selection_data,
                                                                          info,
                                                                          timestamp);
      break;
    }
}

void
gb_source_view_clear_saved_cursor (GbSourceView *view)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  view->priv->saved_line = -1;
  view->priv->saved_line_offset = -1;
}

static void
gb_source_view_save_cursor (GbSourceView *view)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  view->priv->saved_line = gtk_text_iter_get_line (&iter);
  view->priv->saved_line_offset = gtk_text_iter_get_line_offset (&iter);
}

static void
gb_source_view_restore_cursor (GbSourceView *view)
{
  GbSourceViewPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  if (priv->saved_line == -1 || priv->saved_line_offset == -1)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  if ((priv->saved_line == gtk_text_iter_get_line (&iter)) &&
      (priv->saved_line_offset == gtk_text_iter_get_line_offset (&iter)))
    return;

  if (gb_gtk_text_buffer_get_iter_at_line_and_offset (buffer, &iter,
                                                      priv->saved_line,
                                                      priv->saved_line_offset))
    gtk_text_buffer_select_range (buffer, &iter, &iter);
}

static gboolean
gb_source_view_focus_in_event (GtkWidget     *widget,
                               GdkEventFocus *event)
{
  GtkSourceCompletion *completion;
  gboolean ret;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (widget), FALSE);
  g_return_val_if_fail (event, FALSE);

  gb_source_view_restore_cursor (GB_SOURCE_VIEW (widget));

  ret = GTK_WIDGET_CLASS (gb_source_view_parent_class)->focus_in_event (widget, event);

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (widget));
  gtk_source_completion_unblock_interactive (completion);

  return ret;
}

static gboolean
gb_source_view_focus_out_event (GtkWidget     *widget,
                                GdkEventFocus *event)
{
  GtkSourceCompletion *completion;
  gboolean ret;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (widget), FALSE);
  g_return_val_if_fail (event, FALSE);

  gb_source_view_save_cursor (GB_SOURCE_VIEW (widget));

  ret = GTK_WIDGET_CLASS (gb_source_view_parent_class)->focus_out_event (widget, event);

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (widget));
  gtk_source_completion_block_interactive (completion);

  return ret;
}

static void
gb_source_view_finalize (GObject *object)
{
  GbSourceViewPrivate *priv;

  priv = GB_SOURCE_VIEW (object)->priv;

  if (priv->buffer)
    {
      g_object_remove_weak_pointer (G_OBJECT (priv->buffer),
                                    (gpointer *)&priv->buffer);
      priv->buffer = NULL;
    }

  gb_source_view_disconnect_settings (GB_SOURCE_VIEW (object));
  g_clear_pointer (&priv->snippets, g_queue_free);
  g_clear_object (&priv->search_highlighter);
  g_clear_object (&priv->auto_indenter);
  g_clear_object (&priv->html_provider);
  g_clear_object (&priv->snippets_provider);
  g_clear_object (&priv->words_provider);
  g_clear_object (&priv->vim);
  g_clear_object (&priv->emacs);
  g_clear_object (&priv->css_provider);

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
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value, view->priv->auto_indent);
      break;

    case PROP_ENABLE_WORD_COMPLETION:
      g_value_set_boolean (value,
                           gb_source_view_get_enable_word_completion (view));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      g_value_set_boolean (value,
                           gb_source_view_get_insert_matching_brace (view));
      break;

    case PROP_OVERWRITE_BRACES:
      g_value_set_boolean (value,
                           gb_source_view_get_overwrite_braces (view));
      break;

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
    case PROP_AUTO_INDENT:
      view->priv->auto_indent = !!g_value_get_boolean (value);
      gb_source_view_reload_auto_indenter (view);
      break;

    case PROP_ENABLE_WORD_COMPLETION:
      gb_source_view_set_enable_word_completion (view,
                                                 g_value_get_boolean (value));
      break;

    case PROP_FONT_NAME:
      gb_source_view_set_font_name (view, g_value_get_string (value));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      gb_source_view_set_insert_matching_brace (view,
                                                g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE_BRACES:
      gb_source_view_set_overwrite_braces (view, g_value_get_boolean (value));
      break;

    case PROP_SEARCH_HIGHLIGHTER:
      gb_source_view_set_search_highlighter (view, g_value_get_object (value));
      break;

    case PROP_SHOW_SHADOW:
      gb_source_view_set_show_shadow (view, g_value_get_boolean (value));
      break;

    case PROP_SMART_HOME_END_SIMPLE:
      if (g_value_get_boolean (value))
        gtk_source_view_set_smart_home_end (GTK_SOURCE_VIEW (view),
                                            GTK_SOURCE_SMART_HOME_END_BEFORE);
      else
        gtk_source_view_set_smart_home_end (GTK_SOURCE_VIEW (view),
                                            GTK_SOURCE_SMART_HOME_END_DISABLED);
      break;

    case PROP_SHOW_GRID_LINES:
      if (g_value_get_boolean (value))
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (view),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
      else
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (view),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
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
  GtkBindingSet *binding_set;

  object_class->constructed = gb_source_view_constructed;
  object_class->finalize = gb_source_view_finalize;
  object_class->get_property = gb_source_view_get_property;
  object_class->set_property = gb_source_view_set_property;

  widget_class->focus_in_event = gb_source_view_focus_in_event;
  widget_class->focus_out_event = gb_source_view_focus_out_event;
  widget_class->grab_focus = gb_source_view_grab_focus;
  widget_class->key_press_event = gb_source_view_key_press_event;
  widget_class->drag_data_received = gb_source_view_drag_data_received;

  text_view_class->draw_layer = gb_source_view_draw_layer;

  klass->draw_layer = gb_source_view_real_draw_layer;
  klass->display_documentation = gb_source_view_display_documentation;
  klass->request_documentation = gb_source_view_request_documentation;

  gParamSpecs [PROP_ENABLE_WORD_COMPLETION] =
    g_param_spec_boolean ("enable-word-completion",
                          _("Enable Word Completion"),
                          _("Enable Word Completion"),
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENABLE_WORD_COMPLETION,
                                   gParamSpecs [PROP_ENABLE_WORD_COMPLETION]);

  gParamSpecs [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         _("Font Name"),
                         _("The font name to apply to the widget."),
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_NAME,
                                   gParamSpecs [PROP_FONT_NAME]);

  gParamSpecs [PROP_INSERT_MATCHING_BRACE] =
    g_param_spec_boolean ("insert-matching-brace",
                          _("Insert Matching Brace"),
                          _("If we should insert matching braces."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INSERT_MATCHING_BRACE,
                                   gParamSpecs [PROP_INSERT_MATCHING_BRACE]);

  gParamSpecs [PROP_OVERWRITE_BRACES] =
    g_param_spec_boolean ("overwrite-braces",
                          _("Overwrite Braces"),
                          _("If we should overwrite braces, brackets, "
                            "parenthesis and quotes."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_OVERWRITE_BRACES,
                                   gParamSpecs [PROP_OVERWRITE_BRACES]);

  gParamSpecs[PROP_SHOW_SHADOW] =
    g_param_spec_boolean ("show-shadow",
                          _("Show Shadow"),
                          _("Show the search shadow"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_SHADOW,
                                   gParamSpecs[PROP_SHOW_SHADOW]);

  gParamSpecs[PROP_SEARCH_HIGHLIGHTER] =
    g_param_spec_object ("search-highlighter",
                         _("Search Highlighter"),
                         _("Search Highlighter"),
                         GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_HIGHLIGHTER,
                                   gParamSpecs[PROP_SEARCH_HIGHLIGHTER]);

  gParamSpecs [PROP_SMART_HOME_END_SIMPLE] =
    g_param_spec_boolean ("smart-home-end-simple",
                          _("Smart Home End"),
                          _("Enable smart home end in gtksourceview."),
                          TRUE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SMART_HOME_END_SIMPLE,
                                   gParamSpecs [PROP_SMART_HOME_END_SIMPLE]);

  gParamSpecs [PROP_SHOW_GRID_LINES] =
    g_param_spec_boolean ("show-grid-lines",
                          _("Show Grid Lines"),
                          _("Whether to show the grid lines."),
                          TRUE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_GRID_LINES,
                                   gParamSpecs [PROP_SHOW_GRID_LINES]);

  g_object_class_override_property (object_class,
                                    PROP_AUTO_INDENT,
                                    "auto-indent");

  gSignals [PUSH_SNIPPET] =
    g_signal_new ("push-snippet",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceViewClass, push_snippet),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  3,
                  GB_TYPE_SOURCE_SNIPPET,
                  GB_TYPE_SOURCE_SNIPPET_CONTEXT,
                  GTK_TYPE_TEXT_ITER);

  gSignals [POP_SNIPPET] =
    g_signal_new ("pop-snippet",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceViewClass, pop_snippet),
                  NULL,
                  NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  GB_TYPE_SOURCE_SNIPPET);

  gSignals [BEGIN_SEARCH] =
    g_signal_new ("begin-search",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceViewClass, begin_search),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_DIRECTION_TYPE,
                  G_TYPE_STRING);

  gSignals [DISPLAY_DOCUMENTATION] =
    g_signal_new ("display-documentation",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbSourceViewClass,
                                   display_documentation),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  gSignals [DRAW_LAYER] =
    g_signal_new ("draw-layer",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceViewClass, draw_layer),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_VIEW_LAYER,
                  G_TYPE_POINTER);

  gSignals [REQUEST_DOCUMENTATION] =
    g_signal_new ("request-documentation",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbSourceViewClass,
                                   request_documentation),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  0);

  gSignals [DROP_URIS] =
    g_signal_new ("drop-uris",
                  GB_TYPE_SOURCE_VIEW,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GbSourceViewClass, drop_uris),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRV);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_k,
                                GDK_SHIFT_MASK | GDK_CONTROL_MASK,
                                "request-documentation",
                                0);
}

static void
gb_source_view_init (GbSourceView *view)
{
  GtkSourceCompletion *completion;
  GtkTargetList *target_list;

  view->priv = gb_source_view_get_instance_private (view);

  view->priv->css_provider = gtk_css_provider_new ();

  view->priv->snippets = g_queue_new ();

  view->priv->saved_line = -1;
  view->priv->saved_line_offset = -1;

  g_signal_connect (view,
                    "notify::buffer",
                    G_CALLBACK (gb_source_view_notify_buffer),
                    NULL);

  /*
   * Add various completion providers.
   */
  view->priv->snippets_provider =
    g_object_new (GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                  "source-view", view,
                  NULL);
  view->priv->words_provider =
    g_object_new (GTK_SOURCE_TYPE_COMPLETION_WORDS,
                  "minimum-word-size", 4,
                  NULL);

  /*
   * Setup VIM integration.
   */
  view->priv->vim = g_object_new (GB_TYPE_SOURCE_VIM,
                                  "enabled", FALSE,
                                  "text-view", view,
                                  NULL);
  g_signal_connect_object (view->priv->vim,
                           "begin-search",
                           G_CALLBACK (gb_source_view_vim_begin_search),
                           view,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (view->priv->vim,
                           "jump-to-doc",
                           G_CALLBACK (gb_source_view_vim_jump_to_doc),
                           view,
                           G_CONNECT_SWAPPED);

  /*
   * Setup Emacs integration.
   */
    view->priv->emacs = g_object_new (GB_TYPE_SOURCE_EMACS,
                                      "enabled", FALSE,
                                      "text-view", view,
                                      NULL);

  /*
   * We block completion when we are not focused so that two SourceViews
   * viewing the same GtkTextBuffer do not both show completion windows.
   */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
  gtk_source_completion_block_interactive (completion);

  /*
   * Drag and drop support
   */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (view));
  if (target_list)
    gtk_target_list_add_uri_targets (target_list, TARGET_URI_LIST);
}
