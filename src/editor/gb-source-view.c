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
#include "gb-editor-document.h"
#include "gb-gtk.h"
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
#include "gb-widget.h"

struct _GbSourceViewPrivate
{
  GQueue                      *snippets;
  GbSourceSearchHighlighter   *search_highlighter;
  GtkTextBuffer               *buffer;
  GbSourceAutoIndenter        *auto_indenter;
  GtkSourceCompletionProvider *snippets_provider;
  GtkSourceCompletionProvider *words_provider;
  GbSourceVim                 *vim;
  GtkCssProvider              *css_provider;

  GSettings                   *language_settings;
  GSettings                   *editor_settings;

  guint                        buffer_insert_text_handler;
  guint                        buffer_insert_text_after_handler;
  guint                        buffer_delete_range_handler;
  guint                        buffer_delete_range_after_handler;
  guint                        buffer_mark_set_handler;
  guint                        buffer_notify_language_handler;

  guint                        auto_indent : 1;
  guint                        enable_word_completion : 1;
  guint                        show_shadow : 1;
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
  PROP_SEARCH_HIGHLIGHTER,
  PROP_SHOW_SHADOW,
  PROP_SMART_HOME_END_SIMPLE,
  LAST_PROP
};

enum {
  BEGIN_SEARCH,
  DISPLAY_DOCUMENTATION,
  DRAW_LAYER,
  POP_SNIPPET,
  PUSH_SNIPPET,
  REQUEST_DOCUMENTATION,
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

      priv->enable_word_completion = enable_word_completion;
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

  g_settings_unbind (buffer, "highlight-matching-brackets");
  g_settings_unbind (buffer, "style-scheme-name");

  g_settings_unbind (view, "auto-indent");
  g_settings_unbind (view, "highlight-current-line");
  g_settings_unbind (view, "insert-spaces-instead-of-tabs");
  g_settings_unbind (view, "right-margin-position");
  g_settings_unbind (view, "show-line-marks");
  g_settings_unbind (view, "show-line-numbers");
  g_settings_unbind (view, "show-right-margin");
  g_settings_unbind (view, "tab-width");
  g_settings_unbind (view, "font-name");
  g_settings_unbind (view->priv->vim, "enabled");

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
      g_settings_bind (settings, "auto-indent", view, "auto-indent",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "highlight-current-line",
                       view, "highlight-current-line",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "highlight-matching-brackets",
                       buffer, "highlight-matching-brackets",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "insert-spaces-instead-of-tabs",
                       view, "insert-spaces-instead-of-tabs",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "right-margin-position",
                       view, "right-margin-position",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "smart-home-end",
                       view, "smart-home-end-simple",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "show-line-marks",
                       view, "show-line-marks",
                       G_SETTINGS_BIND_GET);
      g_settings_bind (settings, "show-line-numbers",
                       view,"show-line-numbers",
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
  g_settings_bind (view->priv->editor_settings, "word-completion",
                   view, "enable-word-completion", G_SETTINGS_BIND_GET);
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
gb_source_view_vim_begin_search (GbSourceView *view,
                                 const gchar  *text,
                                 GbSourceVim  *vim)
{
  g_return_if_fail (GB_IS_SOURCE_VIEW (view));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  gb_source_view_begin_search (view, GTK_DIR_DOWN, text);
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

  view->priv->show_shadow = show_shadow;
  g_object_notify_by_pspec (G_OBJECT (view), gParamSpecs[PROP_SHOW_SHADOW]);
  invalidate_window (view);
}

void
gb_source_view_indent_selection (GbSourceView *view)
{
  GbSourceViewPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;
  GtkTextMark *mark_begin;
  GtkTextMark *mark_end;
  gboolean use_spaces = FALSE;
  guint tab_width = 0;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  g_object_get (view,
                "insert-spaces-instead-of-tabs", &use_spaces,
                "tab-width", &tab_width,
                NULL);

  if (!gtk_text_buffer_get_has_selection (priv->buffer))
    return;

  gtk_text_buffer_get_selection_bounds (priv->buffer, &begin, &end);

  /*
   * Expand the iters to the beginning of the first line.
   */
  while (!gtk_text_iter_starts_line (&begin))
    gtk_text_iter_backward_char (&begin);

  /*
   * Set marks so we can track our end position after every edit.
   * Also allows us to reselect the whole range at the end.
   */
  mark_begin = gtk_text_buffer_create_mark (priv->buffer,
                                            "tab-selection-begin",
                                            &begin, TRUE);
  mark_end = gtk_text_buffer_create_mark (priv->buffer,
                                          "tab-selection-end",
                                          &end, FALSE);
  gtk_text_iter_assign (&iter, &begin);

  /*
   * Walk through each selected line, adding a tab or the proper number of
   * spaces at the beginning of each line.
   */
  gtk_text_buffer_begin_user_action (priv->buffer);
  do
    {
      if (use_spaces)
        {
          guint i;

          for (i = 0; i < tab_width; i++)
            gtk_text_buffer_insert (priv->buffer, &iter, " ", 1);
        }
      else
        gtk_text_buffer_insert (priv->buffer, &iter, "\t", 1);

      if (!gtk_text_iter_forward_line (&iter))
        break;

      gtk_text_buffer_get_iter_at_mark (priv->buffer, &end, mark_end);
    }
  while (gtk_text_iter_compare (&iter, &end) < 0);
  gtk_text_buffer_end_user_action (priv->buffer);

  /*
   * Reselect our expanded range.
   */
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &end, mark_end);
  gtk_text_buffer_select_range (priv->buffer, &begin, &end);

  /*
   * Remove our temporary marks.
   */
  gtk_text_buffer_delete_mark (priv->buffer, mark_begin);
  gtk_text_buffer_delete_mark (priv->buffer, mark_end);

  EXIT;
}

void
gb_source_view_unindent_selection (GbSourceView *view)
{
  GbSourceViewPrivate *priv;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;
  GtkTextMark *mark_begin;
  GtkTextMark *mark_end;
  gboolean use_spaces = FALSE;
  guint tab_width = 0;

  ENTRY;

  g_return_if_fail (GB_IS_SOURCE_VIEW (view));

  priv = view->priv;

  g_object_get (view,
                "insert-spaces-instead-of-tabs", &use_spaces,
                "tab-width", &tab_width,
                NULL);

  if (!gtk_text_buffer_get_has_selection (priv->buffer))
    return;

  gtk_text_buffer_get_selection_bounds (priv->buffer, &begin, &end);

  /*
   * Adjust the starting iter to include the whole line.
   */
  while (!gtk_text_iter_starts_line (&begin))
    gtk_text_iter_backward_char (&begin);

  /*
   * Set marks so we can track our end position after every edit.
   * Also allows us to reselect the whole range at the end.
   */
  mark_begin = gtk_text_buffer_create_mark (priv->buffer,
                                            "tab-selection-begin",
                                            &begin, TRUE);
  mark_end = gtk_text_buffer_create_mark (priv->buffer,
                                          "tab-selection-end",
                                          &end, FALSE);
  gtk_text_iter_assign (&iter, &begin);

  /*
   * Walk through each selected line, removing up to `tab_width`
   * spaces from the beginning of the line, or a single tab.
   */
  gtk_text_buffer_begin_user_action (priv->buffer);
  do
    {
      GtkTextIter next;
      gboolean found_tab = FALSE;
      guint n_spaces = 0;
      gunichar ch;

      while (!found_tab && (n_spaces < tab_width))
        {
          ch = gtk_text_iter_get_char (&iter);

          if ((ch == '\t') || (ch == ' '))
            {
              gtk_text_iter_assign (&next, &iter);
              gtk_text_iter_forward_char (&next);
              gtk_text_buffer_delete (priv->buffer, &iter, &next);

              found_tab = (ch == '\t');
              n_spaces += (ch == ' ');
            }
          else
            break;
        }

      if (!gtk_text_iter_forward_line (&iter))
        break;

      gtk_text_buffer_get_iter_at_mark (priv->buffer, &end, mark_end);
    }
  while (gtk_text_iter_compare (&iter, &end) < 0);
  gtk_text_buffer_end_user_action (priv->buffer);

  /*
   * Reselect our expanded range.
   */
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (priv->buffer, &end, mark_end);
  gtk_text_buffer_select_range (priv->buffer, &begin, &end);

  /*
   * Remove our temporary marks.
   */
  gtk_text_buffer_delete_mark (priv->buffer, mark_begin);
  gtk_text_buffer_delete_mark (priv->buffer, mark_end);

  EXIT;
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

  if (language)
    {
      const gchar *lang_id;

      lang_id = gtk_source_language_get_id (language);

      if (g_str_equal (lang_id, "c") || g_str_equal (lang_id, "chdr"))
        auto_indenter = gb_source_auto_indenter_c_new ();
      else if (g_str_equal (lang_id, "python"))
        auto_indenter = gb_source_auto_indenter_python_new ();
      else if (g_str_equal (lang_id, "xml"))
        auto_indenter = gb_source_auto_indenter_xml_new ();
    }

  g_clear_object (&view->priv->auto_indenter);

  view->priv->auto_indenter = auto_indenter;
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
  gb_source_view_reload_snippets (view);
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

      gb_source_view_reload_snippets (view);

      gb_source_view_connect_settings (view);
    }
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
   * If we come across tab or shift tab (left tab) while we have a selection,
   * try to (un)indent the selected lines.
   */
  if (gtk_text_buffer_get_has_selection (priv->buffer))
    {
      switch ((gint) event->keyval)
        {
        case GDK_KEY_KP_Tab:
        case GDK_KEY_Tab:
          gb_source_view_indent_selection (view);
          return TRUE;

        case GDK_KEY_ISO_Left_Tab:
          gb_source_view_unindent_selection (view);
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
  static GdkRGBA lines = { 0 };
  GbSourceViewPrivate *priv = view->priv;
  GtkSourceStyleScheme *scheme;
  GtkTextView *text_view = GTK_TEXT_VIEW (view);
  GtkTextBuffer *buffer;

  /*
   * WORKAROUND:
   *
   * We can't do our grid background from a GtkSourceStyleScheme, so we hack it
   * in here. Once we can do this with CSS background's, we should probably do
   * it there.
   */
  if ((layer == GTK_TEXT_VIEW_LAYER_BELOW) &&
      (buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))) &&
      GTK_SOURCE_IS_BUFFER (buffer) &&
      (scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer))) &&
      g_strcmp0 ("builder", gtk_source_style_scheme_get_id (scheme)) == 0)
    {
      GdkRectangle clip;
      GdkRectangle vis;
      gdouble x;
      gdouble y;
      PangoContext *context;
      PangoLayout *layout;
      int grid_width = 16;
      int grid_height = 16;

      context = gtk_widget_get_pango_context (GTK_WIDGET (view));
      layout = pango_layout_new (context);
      pango_layout_set_text (layout, "X", 1);
      pango_layout_get_pixel_size (layout, &grid_width, &grid_height);
      g_object_unref (layout);

      /* each character becomes 2 stacked boxes. */
      grid_height /= 2;

      if (lines.alpha == 0.0)
        gdk_rgba_parse (&lines, "rgba(.125,.125,.125,.025)");

      cairo_save (cr);
      cairo_set_line_width (cr, 1.0);
      gdk_cairo_set_source_rgba (cr, &lines);
      gdk_cairo_get_clip_rectangle (cr, &clip);
      gtk_text_view_get_visible_rect (text_view, &vis);

      /*
       * The following constants come from gtktextview.c pixel cache
       * settings. Sadly, I didn't expose those in public API so we have to
       * just keep them in sync here. 64 for X, height/2 for Y.
       */
      x = (grid_width - (vis.x % grid_width)) - (64 / grid_width * grid_width)
        - grid_width + 2;
      y = (grid_height - (vis.y % grid_height))
          - (vis.height / 2 / grid_height * grid_height)
          - grid_height;

      for (; x <= clip.x + clip.width; x += grid_width)
        {
          cairo_move_to (cr, x + .5, clip.y - .5);
          cairo_line_to (cr, x + .5, clip.y + clip.height - .5);
        }

      for (; y <= clip.y + clip.height; y += grid_height)
        {
          cairo_move_to (cr, clip.x + .5, y - .5);
          cairo_line_to (cr, clip.x + clip.width + .5, y - .5);
        }

      cairo_stroke (cr);
      cairo_restore (cr);
    }

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

static gboolean
gb_source_view_focus_in_event (GtkWidget     *widget,
                               GdkEventFocus *event)
{
  GtkSourceCompletion *completion;
  gboolean ret;

  g_return_val_if_fail (GB_IS_SOURCE_VIEW (widget), FALSE);
  g_return_val_if_fail (event, FALSE);

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
  g_clear_object (&priv->snippets_provider);
  g_clear_object (&priv->words_provider);
  g_clear_object (&priv->vim);
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
      view->priv->auto_indent = g_value_get_boolean (value);
      break;

    case PROP_ENABLE_WORD_COMPLETION:
      gb_source_view_set_enable_word_completion (view,
                                                 g_value_get_boolean (value));
      break;

    case PROP_FONT_NAME:
      gb_source_view_set_font_name (view, g_value_get_string (value));
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

  view->priv = gb_source_view_get_instance_private (view);

  view->priv->css_provider = gtk_css_provider_new ();

  view->priv->snippets = g_queue_new ();

  g_signal_connect (view,
                    "notify::buffer",
                    G_CALLBACK (gb_source_view_notify_buffer),
                    NULL);

  view->priv->snippets_provider =
    g_object_new (GB_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                  "source-view", view,
                  NULL);

  view->priv->words_provider =
    g_object_new (GTK_SOURCE_TYPE_COMPLETION_WORDS,
                  "minimum-word-size", 4,
                  NULL);

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

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
  gtk_source_completion_block_interactive (completion);
}
