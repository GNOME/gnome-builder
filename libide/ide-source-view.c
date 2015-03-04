/* ide-source-view.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-source-view"

#include <glib/gi18n.h>

#include "ide-animation.h"
#include "ide-back-forward-item.h"
#include "ide-back-forward-list.h"
#include "ide-box-theatric.h"
#include "ide-buffer.h"
#include "ide-context.h"
#include "ide-debug.h"
#include "ide-diagnostic.h"
#include "ide-enums.h"
#include "ide-file.h"
#include "ide-file-settings.h"
#include "ide-highlighter.h"
#include "ide-internal.h"
#include "ide-indenter.h"
#include "ide-language.h"
#include "ide-line-change-gutter-renderer.h"
#include "ide-line-diagnostics-gutter-renderer.h"
#include "ide-pango.h"
#include "ide-source-snippet.h"
#include "ide-source-snippet-chunk.h"
#include "ide-source-snippet-completion-provider.h"
#include "ide-source-snippet-context.h"
#include "ide-source-snippet-private.h"
#include "ide-source-snippets-manager.h"
#include "ide-source-location.h"
#include "ide-source-view.h"
#include "ide-source-view-mode.h"
#include "ide-source-view-movements.h"

#define DEFAULT_FONT_DESC "Monospace 11"
#define ANIMATION_X_GROW  50
#define ANIMATION_Y_GROW  30

typedef struct
{
  IdeBackForwardList          *back_forward_list;
  IdeBuffer                   *buffer;
  GtkCssProvider              *css_provider;
  PangoFontDescription        *font_desc;
  IdeIndenter                 *indenter;
  GtkSourceGutterRenderer     *line_change_renderer;
  GtkSourceGutterRenderer     *line_diagnostics_renderer;
  IdeSourceViewMode           *mode;
  GQueue                      *snippets;
  GtkSourceCompletionProvider *snippets_provider;

  gulong                       buffer_changed_handler;
  gulong                       buffer_delete_range_after_handler;
  gulong                       buffer_delete_range_handler;
  gulong                       buffer_insert_text_after_handler;
  gulong                       buffer_insert_text_handler;
  gulong                       buffer_line_flags_changed_handler;
  gulong                       buffer_mark_set_handler;
  gulong                       buffer_notify_file_handler;
  gulong                       buffer_notify_highlight_diagnostics_handler;
  gulong                       buffer_notify_language_handler;

  guint                        auto_indent : 1;
  guint                        insert_matching_brace : 1;
  guint                        overwrite_braces : 1;
  guint                        show_grid_lines : 1;
  guint                        show_line_changes : 1;
  guint                        snippet_completion : 1;
} IdeSourceViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_AUTO_INDENT,
  PROP_BACK_FORWARD_LIST,
  PROP_FONT_NAME,
  PROP_FONT_DESC,
  PROP_INSERT_MATCHING_BRACE,
  PROP_OVERWRITE_BRACES,
  PROP_SHOW_GRID_LINES,
  PROP_SHOW_LINE_CHANGES,
  PROP_SNIPPET_COMPLETION,
  LAST_PROP
};

enum {
  ACTION,
  INSERT_AT_CURSOR_AND_INDENT,
  JUMP,
  MOVEMENT,
  PUSH_SNIPPET,
  POP_SNIPPET,
  SET_MODE,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void
activate_action (GtkWidget   *widget,
                 const gchar *prefix,
                 const gchar *action_name,
                 GVariant    *parameter)
{
  GApplication *app;
  GtkWidget *toplevel;
  GActionGroup *group = NULL;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (prefix);
  g_return_if_fail (action_name);

  app = g_application_get_default ();
  toplevel = gtk_widget_get_toplevel (widget);

  while ((group == NULL) && (widget != NULL))
    {
      group = gtk_widget_get_action_group (widget, prefix);
      widget = gtk_widget_get_parent (widget);
    }

  if (!group && g_str_equal (prefix, "win") && G_IS_ACTION_GROUP (toplevel))
    group = G_ACTION_GROUP (toplevel);

  if (!group && g_str_equal (prefix, "app") && G_IS_ACTION_GROUP (app))
    group = G_ACTION_GROUP (app);

  if (group)
    {
      if (g_action_group_has_action (group, action_name))
        {
          g_action_group_activate_action (group, action_name, parameter);
          return;
        }
    }

  if (parameter && g_variant_is_floating (parameter))
    {
      parameter = g_variant_ref_sink (parameter);
      g_variant_unref (parameter);
    }

  g_warning ("Failed to resolve action %s.%s", prefix, action_name);
}

static void
ide_source_view_block_handlers (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->buffer)
    {
      g_signal_handler_block (priv->buffer, priv->buffer_insert_text_handler);
      g_signal_handler_block (priv->buffer, priv->buffer_insert_text_after_handler);
      g_signal_handler_block (priv->buffer, priv->buffer_delete_range_handler);
      g_signal_handler_block (priv->buffer, priv->buffer_delete_range_after_handler);
      g_signal_handler_block (priv->buffer, priv->buffer_mark_set_handler);
    }
}

static void
ide_source_view_unblock_handlers (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->buffer)
    {
      g_signal_handler_unblock (priv->buffer, priv->buffer_insert_text_handler);
      g_signal_handler_unblock (priv->buffer, priv->buffer_insert_text_after_handler);
      g_signal_handler_unblock (priv->buffer, priv->buffer_delete_range_handler);
      g_signal_handler_unblock (priv->buffer, priv->buffer_delete_range_after_handler);
      g_signal_handler_unblock (priv->buffer, priv->buffer_mark_set_handler);
    }
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

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (iter1);
  g_assert (iter2);
  g_assert (rect);

  gtk_text_view_get_iter_location (text_view, iter1, &area);

  gtk_text_iter_assign (&iter, iter1);

  do
    {
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);

      gtk_text_iter_forward_to_line_end (&iter);
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);

      if (!gtk_text_iter_forward_char (&iter))
        break;
    }
  while (gtk_text_iter_compare (&iter, iter2) <= 0);

  gtk_text_view_buffer_to_window_coords (text_view, window_type, area.x, area.y, &area.x, &area.y);

  *rect = area;
}

static void
animate_in (IdeSourceView     *self,
            const GtkTextIter *begin,
            const GtkTextIter *end)
{
  IdeBoxTheatric *theatric;
  GtkAllocation alloc;
  GdkRectangle rect = { 0 };

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (begin);
  g_assert (end);

  get_rect_for_iters (GTK_TEXT_VIEW (self), begin, end, &rect, GTK_TEXT_WINDOW_WIDGET);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  rect.height = MIN (rect.height, alloc.height - rect.y);

  theatric = g_object_new (IDE_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", rect.height,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  ide_object_animate_full (theatric,
                           IDE_ANIMATION_EASE_IN_CUBIC,
                           250,
                           gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                           g_object_unref,
                           theatric,
                           "x", rect.x - ANIMATION_X_GROW,
                           "width", rect.width + (ANIMATION_X_GROW * 2),
                           "y", rect.y - ANIMATION_Y_GROW,
                           "height", rect.height + (ANIMATION_Y_GROW * 2),
                           "alpha", 0.0,
                           NULL);
}

static void
ide_source_view_scroll_to_insert (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self), &iter, 0.0, FALSE, 0.0, 0.0);
}

static void
ide_source_view_invalidate_window (IdeSourceView *self)
{
  GdkWindow *window;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((window = gtk_text_view_get_window (GTK_TEXT_VIEW (self), GTK_TEXT_WINDOW_WIDGET)))
    {
      gdk_window_invalidate_rect (window, NULL, TRUE);
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static gchar *
text_iter_get_line_prefix (const GtkTextIter *iter)
{
  GtkTextIter begin;
  GString *str;

  g_assert (iter);

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

static void
ide_source_view_reload_snippets (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippets *snippets = NULL;
  IdeContext *context = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((priv->buffer != NULL) && (context = ide_buffer_get_context (priv->buffer)))
    {
      IdeSourceSnippetsManager *manager;
      GtkSourceLanguage *source_language;

      manager = ide_context_get_snippets_manager (context);
      source_language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (priv->buffer));
      if (source_language != NULL)
        snippets = ide_source_snippets_manager_get_for_language (manager, source_language);
    }

  g_object_set (priv->snippets_provider, "snippets", snippets, NULL);
}

static void
ide_source_view_reload_indenter (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->auto_indent && !priv->indenter)
    gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), TRUE);
  else
    gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), FALSE);
}

static void
ide_source_view_set_indenter (IdeSourceView *self,
                              IdeIndenter   *indenter)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (!indenter || IDE_IS_INDENTER (indenter));

  if (g_set_object (&priv->indenter, indenter))
    ide_source_view_reload_indenter (self);
}

static void
ide_source_view__file_load_settings_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  g_autoptr(IdeSourceView) self = user_data;
  g_autoptr(IdeFileSettings) file_settings = NULL;
  g_autoptr(GError) error = NULL;
  IdeFile *file = (IdeFile *)object;
  IdeIndentStyle indent_style;
  guint right_margin_position;
  guint tab_width;
  gint indent_width;

  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  file_settings = ide_file_load_settings_finish (file, result, &error);

  if (!file_settings)
    {
      g_message ("%s", error->message);
      return;
    }

  indent_width = ide_file_settings_get_indent_width (file_settings);
  indent_style = ide_file_settings_get_indent_style (file_settings);
  right_margin_position = ide_file_settings_get_right_margin_position (file_settings);
  tab_width = ide_file_settings_get_tab_width (file_settings);

  gtk_source_view_set_indent_width (GTK_SOURCE_VIEW (self), indent_width);
  gtk_source_view_set_tab_width (GTK_SOURCE_VIEW (self), tab_width);
  gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (self), right_margin_position);
  gtk_source_view_set_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self),
                                                     (indent_style == IDE_INDENT_STYLE_SPACES));
}

static void
ide_source_view_reload_file_settings (IdeSourceView *self)
{
  IdeBuffer *buffer;
  IdeFile *file;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self))));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)));
  file = ide_buffer_get_file (buffer);

  ide_file_load_settings_async (file,
                                NULL,
                                ide_source_view__file_load_settings_cb,
                                g_object_ref (self));
}

static void
ide_source_view_reload_language (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  IdeFile *file = NULL;
  IdeLanguage *language = NULL;
  GtkSourceLanguage *source_language = NULL;
  IdeIndenter *indenter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  language = ide_file_get_language (file);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_LANGUAGE (language));

  source_language = ide_language_get_source_language (language);
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), source_language);

  indenter = ide_language_get_indenter (language);
  ide_source_view_set_indenter (self, indenter);
}

static void
ide_source_view__buffer_notify_file_cb (IdeSourceView *self,
                                        GParamSpec    *pspec,
                                        IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_source_view_reload_language (self);
  ide_source_view_reload_file_settings (self);
  ide_source_view_reload_snippets (self);
}

static void
ide_source_view__buffer_notify_language_cb (IdeSourceView *self,
                                            GParamSpec    *pspec,
                                            IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
}

static void
ide_source_view__buffer_changed_cb (IdeSourceView *self,
                                    IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

}

static void
ide_source_view_rebuild_css (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (!priv->css_provider)
    {
      GtkStyleContext *style_context;

      priv->css_provider = gtk_css_provider_new ();
      style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
      gtk_style_context_add_provider (style_context,
                                      GTK_STYLE_PROVIDER (priv->css_provider),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  if (priv->font_desc)
    {
      g_autofree gchar *str = NULL;
      g_autofree gchar *css = NULL;

      str = ide_pango_font_description_to_css (priv->font_desc);
      css = g_strdup_printf ("IdeSourceView { %s }", str ?: "");
      gtk_css_provider_load_from_data (priv->css_provider, css, -1, NULL);
    }
}

static void
ide_source_view_invalidate_range_mark (IdeSourceView *self,
                                       GtkTextMark   *mark_begin,
                                       GtkTextMark   *mark_end)
{
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GtkTextIter begin;
  GtkTextIter end;
  GdkWindow *window;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_MARK (mark_begin));
  g_assert (GTK_IS_TEXT_MARK (mark_end));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

  get_rect_for_iters (GTK_TEXT_VIEW (self), &begin, &end, &rect, GTK_TEXT_WINDOW_TEXT);
  window = gtk_text_view_get_window (GTK_TEXT_VIEW (self), GTK_TEXT_WINDOW_TEXT);
  gdk_window_invalidate_rect (window, &rect, FALSE);
}

static void
ide_source_view__buffer_insert_text_cb (GtkTextBuffer *buffer,
                                        GtkTextIter   *iter,
                                        gchar         *text,
                                        gint           len,
                                        gpointer       user_data)
{
  IdeSourceView *self= user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ide_source_view_block_handlers (self);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_before_insert_text (snippet, buffer, iter, text, len);

  ide_source_view_unblock_handlers (self);
}

static void
ide_source_view__buffer_insert_text_after_cb (GtkTextBuffer *buffer,
                                              GtkTextIter   *iter,
                                              gchar         *text,
                                              gint           len,
                                              gpointer       user_data)
{
  IdeSourceView *self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      GtkTextMark *begin;
      GtkTextMark *end;

      ide_source_view_block_handlers (self);
      ide_source_snippet_after_insert_text (snippet, buffer, iter, text, len);
      ide_source_view_unblock_handlers (self);

      begin = ide_source_snippet_get_mark_begin (snippet);
      end = ide_source_snippet_get_mark_end (snippet);
      ide_source_view_invalidate_range_mark (self, begin, end);
    }
}

static void
ide_source_view__buffer_delete_range_cb (GtkTextBuffer *buffer,
                                         GtkTextIter   *begin,
                                         GtkTextIter   *end,
                                         gpointer       user_data)
{
  IdeSourceView *self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      GtkTextMark *begin_mark;
      GtkTextMark *end_mark;

      ide_source_view_block_handlers (self);
      ide_source_snippet_before_delete_range (snippet, buffer, begin, end);
      ide_source_view_unblock_handlers (self);

      begin_mark = ide_source_snippet_get_mark_begin (snippet);
      end_mark = ide_source_snippet_get_mark_end (snippet);
      ide_source_view_invalidate_range_mark (self, begin_mark, end_mark);
    }
}

static void
ide_source_view__buffer_delete_range_after_cb (GtkTextBuffer *buffer,
                                               GtkTextIter   *begin,
                                               GtkTextIter   *end,
                                               gpointer       user_data)
{
  IdeSourceView *self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ide_source_view_block_handlers (self);

  if ((snippet = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_after_delete_range (snippet, buffer, begin, end);

  ide_source_view_unblock_handlers (self);
}

static void
ide_source_view__buffer_mark_set_cb (GtkTextBuffer *buffer,
                                     GtkTextIter   *iter,
                                     GtkTextMark   *mark,
                                     gpointer       user_data)
{
  IdeSourceView *self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (iter);
  g_assert (GTK_IS_TEXT_MARK (mark));

  ide_source_view_block_handlers (self);

  if (mark == gtk_text_buffer_get_insert (buffer))
    {
      while ((snippet = g_queue_peek_head (priv->snippets)) &&
             !ide_source_snippet_insert_set (snippet, mark))
        ide_source_view_pop_snippet (self);
    }

  ide_source_view_unblock_handlers (self);
}

static void
ide_source_view__buffer_notify_highlight_diagnostics_cb (IdeSourceView *self,
                                                         GParamSpec    *pspec,
                                                         IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (priv->line_diagnostics_renderer)
    {
      gboolean visible;

      visible = ide_buffer_get_highlight_diagnostics (buffer);
      g_object_set (priv->line_diagnostics_renderer,
                    "visible", visible,
                    NULL);
    }
}

static void
ide_source_view__buffer_line_flags_changed_cb (IdeSourceView *self,
                                               IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_source_gutter_renderer_queue_draw (priv->line_change_renderer);
  gtk_source_gutter_renderer_queue_draw (priv->line_diagnostics_renderer);
}

static void
ide_source_view_connect_buffer (IdeSourceView *self,
                                IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  priv->buffer_changed_handler =
      g_signal_connect_object (buffer,
                               "changed",
                               G_CALLBACK (ide_source_view__buffer_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_line_flags_changed_handler =
      g_signal_connect_object (buffer,
                               "line-flags-changed",
                               G_CALLBACK (ide_source_view__buffer_line_flags_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_notify_highlight_diagnostics_handler =
      g_signal_connect_object (buffer,
                               "notify::highlight-diagnostics",
                               G_CALLBACK (ide_source_view__buffer_notify_highlight_diagnostics_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_notify_file_handler =
      g_signal_connect_object (buffer,
                               "notify::file",
                               G_CALLBACK (ide_source_view__buffer_notify_file_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_notify_language_handler =
      g_signal_connect_object (buffer,
                               "notify::language",
                               G_CALLBACK (ide_source_view__buffer_notify_language_cb),
                               self,
                               G_CONNECT_SWAPPED);

  priv->buffer_insert_text_handler =
      g_signal_connect_object (buffer,
                               "insert-text",
                               G_CALLBACK (ide_source_view__buffer_insert_text_cb),
                               self,
                               0);

  priv->buffer_insert_text_after_handler =
      g_signal_connect_object (buffer,
                               "insert-text",
                               G_CALLBACK (ide_source_view__buffer_insert_text_after_cb),
                               self,
                               G_CONNECT_AFTER);

  priv->buffer_delete_range_handler =
      g_signal_connect_object (buffer,
                               "delete-range",
                               G_CALLBACK (ide_source_view__buffer_delete_range_cb),
                               self,
                               0);

  priv->buffer_delete_range_after_handler =
      g_signal_connect_object (buffer,
                               "delete-range",
                               G_CALLBACK (ide_source_view__buffer_delete_range_after_cb),
                               self,
                               G_CONNECT_AFTER);

  priv->buffer_mark_set_handler =
      g_signal_connect_object (buffer,
                               "mark-set",
                               G_CALLBACK (ide_source_view__buffer_mark_set_cb),
                               self,
                               0);

  ide_source_view__buffer_notify_language_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_file_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_highlight_diagnostics_cb (self, NULL, buffer);
}

static void
ide_source_view_disconnect_buffer (IdeSourceView *self,
                                   IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_clear_signal_handler (buffer, &priv->buffer_delete_range_after_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_delete_range_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_insert_text_after_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_insert_text_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_line_flags_changed_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_mark_set_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_notify_highlight_diagnostics_handler);
  ide_clear_signal_handler (buffer, &priv->buffer_notify_language_handler);

  ide_source_view_set_indenter (self, NULL);
}

static void
ide_source_view_notify_buffer (IdeSourceView *self,
                               GParamSpec    *pspec,
                               gpointer       user_data)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (priv->buffer != (IdeBuffer *)buffer)
    {
      if (priv->buffer != NULL)
        {
          ide_source_view_disconnect_buffer (self, priv->buffer);
          g_clear_object (&priv->buffer);
        }

      /*
       * Only enable IdeSourceView features if this is an IdeBuffer.
       * Ignore for GtkSourceBuffer, and GtkTextBuffer.
       */
      if (IDE_IS_BUFFER (buffer))
        {
          priv->buffer = g_object_ref (buffer);
          ide_source_view_connect_buffer (self, priv->buffer);
        }
    }
}

static gunichar
peek_previous_char (const GtkTextIter *iter)
{
  GtkTextIter copy = *iter;
  gunichar ch = 0;

  if (!gtk_text_iter_is_start (&copy))
    {
      gtk_text_iter_backward_char (&copy);
      ch = gtk_text_iter_get_char (&copy);
    }

  return ch;
}

static void
ide_source_view_maybe_overwrite (IdeSourceView *self,
                                 GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextMark *mark;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gunichar ch;
  gunichar prev_ch;
  gboolean ignore = FALSE;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (event);

  /*
   * Some auto-indenters will perform triggers on certain key-press that we
   * would hijack by otherwise "doing nothing" during this key-press. So to
   * avoid that, we actually delete the previous value and then allow this
   * key-press event to continue.
   */

  if (!priv->overwrite_braces)
    return;

  /*
   * WORKAROUND:
   *
   * If we are inside of a snippet, then let's not do anything. It really
   * messes with the position tracking. Once we can better integrate these
   * things, go ahead and remove this.
   */
  if (priv->snippets->length)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ch = gtk_text_iter_get_char (&iter);
  prev_ch = peek_previous_char (&iter);

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
count_chars_on_line (IdeSourceView      *view,
                     gunichar           expected_char,
                     const GtkTextIter *iter)
{
  GtkTextIter cur;
  guint count = 0;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (view), 0);
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
ide_source_view_maybe_insert_match (IdeSourceView *self,
                                    GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
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

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (event);

  /*
   * If we are disabled, then do nothing.
   */
  if (!priv->insert_matching_brace)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
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

          count = count_chars_on_line (self, '"', &iter);
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
ide_source_view_maybe_delete_match (IdeSourceView *self,
                                    GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter prev;
  gunichar ch;
  gunichar match;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (event);
  g_assert_cmpint (event->keyval, ==, GDK_KEY_BackSpace);

  if (!priv->insert_matching_brace)
    return FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
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

  if (match && (gtk_text_iter_get_char (&iter) == match))
    {
      gtk_text_iter_forward_char (&iter);
      gtk_text_buffer_delete (buffer, &prev, &iter);

      return TRUE;
    }

  return FALSE;
}

static void
ide_source_view_do_indent (IdeSourceView *self,
                           GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkWidget *widget = (GtkWidget *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  gchar *indent;
  gint cursor_offset = 0;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (event);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  /*
   * Insert into the buffer so the auto-indenter can see it. If
   * GtkSourceView:auto-indent is set, then we will end up with very
   * unpredictable results.
   */
  GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);

  /*
   * Set begin and end to the position of the new insertion point.
   */
  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (priv->buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &begin, insert);
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &end, insert);

  /*
   * Let the formatter potentially set the replacement text.
   */
  indent = ide_indenter_format (priv->indenter, GTK_TEXT_VIEW (self), &begin, &end,
                                &cursor_offset, event);

  if (indent)
    {
      /*
       * Insert the indention text.
       */
      gtk_text_buffer_begin_user_action (buffer);
      if (!gtk_text_iter_equal (&begin, &end))
        gtk_text_buffer_delete (buffer, &begin, &end);
      gtk_text_buffer_insert (buffer, &begin, indent, -1);
      g_free (indent);
      gtk_text_buffer_end_user_action (buffer);

      /*
       * Place the cursor, as it could be somewhere within our indent text.
       */
      gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
      if (cursor_offset > 0)
        gtk_text_iter_forward_chars (&begin, cursor_offset);
      else if (cursor_offset < 0)
        gtk_text_iter_backward_chars (&begin, ABS (cursor_offset));
      gtk_text_buffer_select_range (buffer, &begin, &begin);
    }
}

static gboolean
ide_source_view_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  IdeSourceSnippet *snippet;
  gboolean ret = FALSE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  /*
   * If we are in a non-default mode, dispatch the event to the mode. This allows custom
   * keybindings like Emacs and Vim to be implemented using gtk-bindings CSS.
   */
  if (priv->mode)
    {
      IdeSourceViewMode *mode;
      gboolean handled;
      gboolean remove = FALSE;

#ifndef IDE_DISABLE_TRACE
      {
        gunichar ch = 0;
        gchar *name = NULL;

        g_object_get (priv->mode, "name", &name, NULL);
        if (event->string)
          ch = g_utf8_get_char (event->string);
        IDE_TRACE_MSG ("dispatching to mode \"%s\": (%s)",
                       name, g_unichar_isprint (ch) ? event->string : "");
        g_free (name);
      }
#endif

      /* hold a reference incase binding changes mode */
      mode = g_object_ref (priv->mode);

      handled = _ide_source_view_mode_do_event (priv->mode, event, &remove);

      if (remove)
        {
          /* only remove mode if it is still active */
          if (priv->mode == mode)
            g_clear_object (&priv->mode);
        }

      g_object_unref (mode);

      if (handled)
        return TRUE;
    }

  /*
   * Handle movement through the tab stops of the current snippet if needed.
   */
  if ((snippet = g_queue_peek_head (priv->snippets)))
    {
      switch ((gint) event->keyval)
        {
        case GDK_KEY_Escape:
          ide_source_view_block_handlers (self);
          ide_source_view_pop_snippet (self);
          ide_source_view_scroll_to_insert (self);
          ide_source_view_unblock_handlers (self);
          return TRUE;

        case GDK_KEY_KP_Tab:
        case GDK_KEY_Tab:
          ide_source_view_block_handlers (self);
          if (!ide_source_snippet_move_next (snippet))
            ide_source_view_pop_snippet (self);
          ide_source_view_scroll_to_insert (self);
          ide_source_view_unblock_handlers (self);
          return TRUE;

        case GDK_KEY_ISO_Left_Tab:
          ide_source_view_block_handlers (self);
          ide_source_snippet_move_previous (snippet);
          ide_source_view_scroll_to_insert (self);
          ide_source_view_unblock_handlers (self);
          return TRUE;

        default:
          break;
        }
    }

  /*
   * Allow the Input Method Context to potentially filter this keystroke.
   */
  if ((event->keyval == GDK_KEY_Return) || (event->keyval == GDK_KEY_KP_Enter))
    if (gtk_text_view_im_context_filter_keypress (GTK_TEXT_VIEW (self), event))
      return TRUE;

  /*
   * If we are going to insert the same character as the next character in the
   * buffer, we may want to remove it first. This allows us to still trigger
   * the auto-indent engine (instead of just short-circuiting the key-press).
   */
  ide_source_view_maybe_overwrite (self, event);

  /*
   * If we are backspacing, and the next character is the matching brace,
   * then we might want to delete it too.
   */
  if ((event->keyval == GDK_KEY_BackSpace) && !gtk_text_buffer_get_has_selection (buffer))
    if (ide_source_view_maybe_delete_match (self, event))
      return TRUE;

  /*
   * If we have an auto-indenter and the event is for a trigger key, then we
   * chain up to the parent class to insert the character, and then let the
   * auto-indenter fix things up.
   */
  if ((priv->buffer != NULL) &&
      (priv->auto_indent != FALSE) &&
      (priv->indenter != NULL) &&
      ide_indenter_is_trigger (priv->indenter, event))
    {
      ide_source_view_do_indent (self, event);
      return TRUE;
    }

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);

  if (ret)
    ide_source_view_maybe_insert_match (self, event);

  return ret;
}

static gboolean
ide_source_view_query_tooltip (GtkWidget  *widget,
                               gint        x,
                               gint        y,
                               gboolean    keyboard_mode,
                               GtkTooltip *tooltip)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)widget;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (GTK_IS_TOOLTIP (tooltip));

  if (priv->buffer != NULL)
    {
      IdeDiagnostic *diagnostic;
      GtkTextIter iter;

      gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_WIDGET, x, y, &x, &y);
      gtk_text_view_get_iter_at_location (text_view, &iter, x, y);
      diagnostic = ide_buffer_get_diagnostic_at_iter (priv->buffer, &iter);

      if (diagnostic)
        {
          g_autofree gchar *str = NULL;

          str = ide_diagnostic_get_text_for_display (diagnostic);
          gtk_tooltip_set_text (tooltip, str);

          return TRUE;
        }
    }

  return FALSE;
}

static void
ide_source_view_real_action (IdeSourceView *self,
                             const gchar   *prefix,
                             const gchar   *action_name,
                             const gchar   *param)
{
  GVariant *variant = NULL;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (*param != 0)
    {
      g_autoptr(GError) error = NULL;

      variant = g_variant_parse (NULL, param, NULL, NULL, &error);

      if (variant == NULL)
        {
          g_warning ("can't parse keybinding parameters \"%s\": %s",
                     param, error->message);
          return;
        }
    }

  activate_action (GTK_WIDGET (self), prefix, action_name, variant);
}

static void
ide_source_view_real_insert_at_cursor_and_indent (IdeSourceView *self,
                                                  const gchar   *str)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GdkEvent fake_event = { 0 };
  GString *gstr;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (str);

  buffer = gtk_text_view_get_buffer (text_view);

  /*
   * Ignore if there is nothing to do.
   */
  if (g_utf8_strlen (str, -1) == 0)
    IDE_EXIT;

  /*
   * If we do not have an indenter registered, just go ahead and insert text.
   */
  if (!priv->auto_indent || !priv->indenter)
    {
      g_signal_emit_by_name (self, "insert-at-cursor", str);
      IDE_EXIT;
    }

  gtk_text_buffer_begin_user_action (buffer);

  /*
   * insert all but last character at once.
   */
  gstr = g_string_new (NULL);
  for (; *str && *g_utf8_next_char (str); str = g_utf8_next_char (str))
    g_string_append_unichar (gstr, g_utf8_get_char (str));
  if (gstr->len)
    g_signal_emit_by_name (self, "insert-at-cursor", gstr->str);
  g_string_free (gstr, TRUE);

  /*
   * Sanity check.
   */
  g_assert (str != NULL);
  g_assert (*str != '\0');

  /*
   * Now insert the last character (presumably something like \n) with a
   * synthesized event that the indenter can deal with.
   */
  fake_event.key.type = GDK_KEY_PRESS;
  fake_event.key.window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);
  fake_event.key.send_event = FALSE;
  fake_event.key.time = GDK_CURRENT_TIME;
  fake_event.key.state = 0;
  fake_event.key.length = 1;
  fake_event.key.string = (gchar *)str;
  fake_event.key.hardware_keycode = 0;
  fake_event.key.group = 0;
  fake_event.key.is_modifier = 0;

  /* Be nice during the common case */
  if (*str == '\n')
    fake_event.key.keyval = GDK_KEY_KP_Enter;
  else
    fake_event.key.keyval = gdk_unicode_to_keyval (g_utf8_get_char (str));

  ide_source_view_do_indent (self, &fake_event.key);

  gtk_text_buffer_end_user_action (buffer);

  IDE_EXIT;
}

static void
ide_source_view_real_jump (IdeSourceView     *self,
                           const GtkTextIter *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(IdeSourceLocation) srcloc = NULL;
  g_autoptr(IdeBackForwardItem) item = NULL;
  IdeContext *context;
  IdeFile *file;
  guint line;
  guint line_offset;
  guint offset;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (location);

  if (!priv->back_forward_list)
    return;

  if (!priv->buffer)
    return;

  context = ide_buffer_get_context (priv->buffer);
  if (!context)
    return;

  file = ide_buffer_get_file (priv->buffer);
  if (!file)
    return;

  line = gtk_text_iter_get_line (location);
  line_offset = gtk_text_iter_get_line_offset (location);
  offset = gtk_text_iter_get_offset (location);

  srcloc = ide_source_location_new (file, line, line_offset, offset);
  item = ide_back_forward_item_new (context, srcloc);

  ide_back_forward_list_push (priv->back_forward_list, item);
}

static void
ide_source_view_real_set_mode (IdeSourceView         *self,
                               const gchar           *mode,
                               IdeSourceViewModeType  type)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;
  IDE_TRACE_MSG ("mode (%s)", mode ?: "<default>");

  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_clear_object (&priv->mode);

  if (mode != NULL)
    priv->mode = _ide_source_view_mode_new (GTK_WIDGET (self), mode, type);

  IDE_EXIT;
}

static void
ide_source_view_real_movement (IdeSourceView         *self,
                               IdeSourceViewMovement  movement,
                               gboolean               extend_selection)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  _ide_source_view_apply_movement (self, movement, extend_selection, 0 /* TODO */);
}

static void
ide_source_view_constructed (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceGutter *gutter;
  gboolean visible;

  G_OBJECT_CLASS (ide_source_view_parent_class)->constructed (object);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);

  priv->line_change_renderer = g_object_new (IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER,
                                             "size", 2,
                                             "visible", priv->show_line_changes,
                                             "xpad", 1,
                                             NULL);
  g_object_ref (priv->line_change_renderer);
  gtk_source_gutter_insert (gutter, priv->line_change_renderer, 0);

  visible = priv->buffer && ide_buffer_get_highlight_diagnostics (priv->buffer);
  priv->line_diagnostics_renderer = g_object_new (IDE_TYPE_LINE_DIAGNOSTICS_GUTTER_RENDERER,
                                                  "size", 16,
                                                  "visible", visible,
                                                  NULL);
  g_object_ref (priv->line_diagnostics_renderer);
  gtk_source_gutter_insert (gutter, priv->line_diagnostics_renderer, -100);
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  ide_source_view_clear_snippets (self);

  g_clear_object (&priv->indenter);
  g_clear_object (&priv->line_change_renderer);
  g_clear_object (&priv->line_diagnostics_renderer);
  g_clear_object (&priv->snippets_provider);
  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->mode);

  if (priv->buffer)
    {
      ide_source_view_disconnect_buffer (self, priv->buffer);
      g_clear_object (&priv->buffer);
    }

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_finalize (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_clear_pointer (&priv->font_desc, pango_font_description_free);
  g_clear_pointer (&priv->snippets, g_queue_free);

  G_OBJECT_CLASS (ide_source_view_parent_class)->finalize (object);
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      g_value_set_boolean (value, priv->auto_indent);
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      g_value_set_boolean (value, ide_source_view_get_insert_matching_brace (self));
      break;

    case PROP_OVERWRITE_BRACES:
      g_value_set_boolean (value, ide_source_view_get_overwrite_braces (self));
      break;

    case PROP_SHOW_GRID_LINES:
      g_value_set_boolean (value, ide_source_view_get_show_grid_lines (self));
      break;

    case PROP_SHOW_LINE_CHANGES:
      g_value_set_boolean (value, ide_source_view_get_show_line_changes (self));
      break;

    case PROP_SNIPPET_COMPLETION:
      g_value_set_boolean (value, ide_source_view_get_snippet_completion (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_AUTO_INDENT:
      priv->auto_indent = !!g_value_get_boolean (value);
      ide_source_view_reload_indenter (self);
      break;

    case PROP_FONT_NAME:
      ide_source_view_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      ide_source_view_set_insert_matching_brace (self, g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE_BRACES:
      ide_source_view_set_overwrite_braces (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_GRID_LINES:
      ide_source_view_set_show_grid_lines (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_CHANGES:
      ide_source_view_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    case PROP_SNIPPET_COMPLETION:
      ide_source_view_set_snippet_completion (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_source_view_class_init (IdeSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_source_view_constructed;
  object_class->dispose = ide_source_view_dispose;
  object_class->finalize = ide_source_view_finalize;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  widget_class->key_press_event = ide_source_view_key_press_event;
  widget_class->query_tooltip = ide_source_view_query_tooltip;

  klass->action = ide_source_view_real_action;
  klass->insert_at_cursor_and_indent = ide_source_view_real_insert_at_cursor_and_indent;
  klass->jump = ide_source_view_real_jump;
  klass->movement = ide_source_view_real_movement;
  klass->set_mode = ide_source_view_real_set_mode;

  g_object_class_override_property (object_class, PROP_AUTO_INDENT, "auto-indent");

  gParamSpecs [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        _("Font Description"),
                        _("The Pango font description to use for rendering source."),
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_DESC,
                                   gParamSpecs [PROP_FONT_DESC]);

  gParamSpecs [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         _("Font Name"),
                         _("The pango font name ot use for rendering source."),
                         "Monospace",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_FONT_NAME,
                                   gParamSpecs [PROP_FONT_NAME]);

  gParamSpecs [PROP_INSERT_MATCHING_BRACE] =
    g_param_spec_boolean ("insert-matching-brace",
                          _("Insert Matching Brace"),
                          _("Insert a matching brace/bracket/quotation/paren."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_INSERT_MATCHING_BRACE,
                                   gParamSpecs [PROP_INSERT_MATCHING_BRACE]);

  gParamSpecs [PROP_OVERWRITE_BRACES] =
    g_param_spec_boolean ("overwrite-braces",
                          _("Overwrite Braces"),
                          _("Overwrite a matching brace/bracket/quotation/paren."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_OVERWRITE_BRACES,
                                   gParamSpecs [PROP_OVERWRITE_BRACES]);

  gParamSpecs [PROP_SHOW_GRID_LINES] =
    g_param_spec_boolean ("show-grid-lines",
                          _("Show Grid Lines"),
                          _("If the background grid should be shown."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_GRID_LINES,
                                   gParamSpecs [PROP_SHOW_GRID_LINES]);

  gParamSpecs [PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes",
                          _("Show Line Changes"),
                          _("If line changes should be shown in the left gutter."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_LINE_CHANGES,
                                   gParamSpecs [PROP_SHOW_LINE_CHANGES]);

  gParamSpecs [PROP_SNIPPET_COMPLETION] =
    g_param_spec_boolean ("snippet-completion",
                          _("Snippet Completion"),
                          _("If snippet expansion should be enabled via the completion window."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SNIPPET_COMPLETION,
                                   gParamSpecs [PROP_SNIPPET_COMPLETION]);

  gSignals [ACTION] = g_signal_new ("action",
                                    G_TYPE_FROM_CLASS (klass),
                                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                    G_STRUCT_OFFSET (IdeSourceViewClass, action),
                                    NULL, NULL,
                                    g_cclosure_marshal_generic,
                                    G_TYPE_NONE,
                                    3,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

  gSignals [INSERT_AT_CURSOR_AND_INDENT] =
    g_signal_new ("insert-at-cursor-and-indent",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, insert_at_cursor_and_indent),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  gSignals [JUMP] =
    g_signal_new ("jump",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSourceViewClass, jump),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_ITER);

  gSignals [MOVEMENT] =
    g_signal_new ("movement",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, movement),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  IDE_TYPE_SOURCE_VIEW_MOVEMENT,
                  G_TYPE_BOOLEAN);

  gSignals [SET_MODE] =
    g_signal_new ("set-mode",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_mode),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  IDE_TYPE_SOURCE_VIEW_MODE_TYPE);

  gSignals [POP_SNIPPET] =
    g_signal_new ("pop-snippet",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSourceViewClass, pop_snippet),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SOURCE_SNIPPET);

  gSignals [PUSH_SNIPPET] =
    g_signal_new ("push-snippet",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSourceViewClass, push_snippet),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  3,
                  IDE_TYPE_SOURCE_SNIPPET,
                  IDE_TYPE_SOURCE_SNIPPET_CONTEXT,
                  GTK_TYPE_TEXT_ITER);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  priv->snippets = g_queue_new ();

  g_signal_connect (self,
                    "notify::buffer",
                    G_CALLBACK (ide_source_view_notify_buffer),
                    NULL);
}

const PangoFontDescription *
ide_source_view_get_font_desc (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->font_desc;
}

void
ide_source_view_set_font_desc (IdeSourceView              *self,
                               const PangoFontDescription *font_desc)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_desc != priv->font_desc)
    {
      g_clear_pointer (&priv->font_desc, pango_font_description_free);

      if (font_desc)
        priv->font_desc = pango_font_description_copy (font_desc);
      else
        priv->font_desc = pango_font_description_from_string (DEFAULT_FONT_DESC);

      ide_source_view_rebuild_css (self);
    }
}

void
ide_source_view_set_font_name (IdeSourceView *self,
                               const gchar   *font_name)
{
  PangoFontDescription *font_desc = NULL;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (font_name)
    font_desc = pango_font_description_from_string (font_name);
  ide_source_view_set_font_desc (self, font_desc);
  if (font_desc)
    pango_font_description_free (font_desc);
}

gboolean
ide_source_view_get_show_line_changes (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_line_changes;
}

void
ide_source_view_set_show_line_changes (IdeSourceView *self,
                                       gboolean       show_line_changes)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_line_changes = !!show_line_changes;

  if (show_line_changes != priv->show_line_changes)
    {
      priv->show_line_changes = show_line_changes;
      if (priv->line_change_renderer)
        gtk_source_gutter_renderer_set_visible (priv->line_change_renderer, show_line_changes);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SHOW_LINE_CHANGES]);
    }
}

gboolean
ide_source_view_get_show_grid_lines (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_grid_lines;
}

void
ide_source_view_set_show_grid_lines (IdeSourceView *self,
                                     gboolean       show_grid_lines)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_grid_lines = !!show_grid_lines;

  if (show_grid_lines != priv->show_grid_lines)
    {
      priv->show_grid_lines = show_grid_lines;
      if (show_grid_lines)
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID);
      else
        gtk_source_view_set_background_pattern (GTK_SOURCE_VIEW (self),
                                                GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SHOW_GRID_LINES]);
    }
}

gboolean
ide_source_view_get_insert_matching_brace (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->insert_matching_brace;
}

gboolean
ide_source_view_get_overwrite_braces (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->overwrite_braces;
}

void
ide_source_view_set_insert_matching_brace (IdeSourceView *self,
                                           gboolean       insert_matching_brace)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  insert_matching_brace = !!insert_matching_brace;

  if (insert_matching_brace != priv->insert_matching_brace)
    {
      priv->insert_matching_brace = insert_matching_brace;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_INSERT_MATCHING_BRACE]);
    }
}

void
ide_source_view_set_overwrite_braces (IdeSourceView *self,
                                      gboolean       overwrite_braces)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  overwrite_braces = !!overwrite_braces;

  if (overwrite_braces != priv->overwrite_braces)
    {
      priv->overwrite_braces = overwrite_braces;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_OVERWRITE_BRACES]);
    }
}

void
ide_source_view_pop_snippet (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if ((snippet = g_queue_pop_head (priv->snippets)))
    {
      ide_source_snippet_finish (snippet);
      g_signal_emit (self, gSignals [POP_SNIPPET], 0, snippet);
      g_object_unref (snippet);
    }

  if ((snippet = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_unpause (snippet);

  ide_source_view_invalidate_window (self);
}

void
ide_source_view_clear_snippets (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  while (priv->snippets->length)
    ide_source_view_pop_snippet (self);
}

void
ide_source_view_push_snippet (IdeSourceView    *self,
                              IdeSourceSnippet *snippet)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippetContext *context;
  IdeSourceSnippet *previous;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;
  gboolean has_more_tab_stops;
  gboolean insert_spaces;
  gchar *line_prefix;
  guint tab_width;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (snippet));

  context = ide_source_snippet_get_context (snippet);

  if ((previous = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_pause (previous);

  g_queue_push_head (priv->snippets, g_object_ref (snippet));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  insert_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self));
  ide_source_snippet_context_set_use_spaces (context, insert_spaces);

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self));
  ide_source_snippet_context_set_tab_width (context, tab_width);

  line_prefix = text_iter_get_line_prefix (&iter);
  ide_source_snippet_context_set_line_prefix (context, line_prefix);
  g_free (line_prefix);

  g_signal_emit (self, gSignals [PUSH_SNIPPET], 0, snippet, context, &iter);

  ide_source_view_block_handlers (self);
  has_more_tab_stops = ide_source_snippet_begin (snippet, buffer, &iter);
  ide_source_view_scroll_to_insert (self);
  ide_source_view_unblock_handlers (self);

  {
    GtkTextMark *mark_begin;
    GtkTextMark *mark_end;
    GtkTextIter begin;
    GtkTextIter end;

    mark_begin = ide_source_snippet_get_mark_begin (snippet);
    mark_end = ide_source_snippet_get_mark_end (snippet);

    gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
    gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

    /*
     * HACK:
     *
     * We need to let the GtkTextView catch up with us so that we can get a realistic area back for
     * the location of the end iter.  Without pumping the main loop, GtkTextView will clamp the
     * result to the height of the insert line.
     */
    while (gtk_events_pending ())
      gtk_main_iteration ();

    animate_in (self, &begin, &end);
  }

  if (!has_more_tab_stops)
    ide_source_view_pop_snippet (self);

  ide_source_view_invalidate_window (self);
}

/**
 * ide_source_view_get_snippet_completion:
 *
 * Gets the #IdeSourceView:snippet-completion property.
 *
 * If enabled, snippet expansion can be performed via the auto completion drop down.
 */
gboolean
ide_source_view_get_snippet_completion (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->snippet_completion;
}

/**
 * ide_source_view_set_snippet_completion:
 *
 * Sets the #IdeSourceView:snippet-completion property. By setting this property to %TRUE,
 * snippets will be loaded for the currently activated source code language. See #IdeSourceSnippet
 * for more information on what can be provided via a snippet.
 *
 * See also: ide_source_view_get_snippet_completion()
 */
void
ide_source_view_set_snippet_completion (IdeSourceView *self,
                                        gboolean       snippet_completion)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  snippet_completion = !!snippet_completion;

  if (snippet_completion != priv->snippet_completion)
    {
      GtkSourceCompletion *completion;

      priv->snippet_completion = snippet_completion;

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));

      if (snippet_completion)
        {
          if (!priv->snippets_provider)
            {
              priv->snippets_provider = g_object_new (IDE_TYPE_SOURCE_SNIPPET_COMPLETION_PROVIDER,
                                                      "source-view", self,
                                                      NULL);
              ide_source_view_reload_snippets (self);
            }

          gtk_source_completion_add_provider (completion, priv->snippets_provider, NULL);
        }
      else
        {
          gtk_source_completion_remove_provider (completion, priv->snippets_provider, NULL);
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SNIPPET_COMPLETION]);
    }
}

/**
 * ide_source_view_get_back_forward_list:
 *
 * Gets the #IdeSourceView:back-forward-list property. This is the list that is used to manage
 * navigation history between multiple #IdeSourceView.
 *
 * Returns: (transfer none) (nullable): An #IdeBackForwardList or %NULL.
 */
IdeBackForwardList *
ide_source_view_get_back_forward_list (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->back_forward_list;
}

void
ide_source_view_set_back_forward_list (IdeSourceView      *self,
                                       IdeBackForwardList *back_forward_list)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (!back_forward_list || IDE_IS_BACK_FORWARD_LIST (back_forward_list));

  if (g_set_object (&priv->back_forward_list, back_forward_list))
    g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_BACK_FORWARD_LIST]);
}

void
ide_source_view_jump (IdeSourceView     *self,
                      const GtkTextIter *location)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (location);

  g_signal_emit (self, gSignals [JUMP], 0, location);
}
