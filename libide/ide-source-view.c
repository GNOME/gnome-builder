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
#include <stdlib.h>

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

#define BEGIN_USER_ACTION(self) \
  G_STMT_START { \
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self)); \
    IDE_TRACE_MSG ("begin_user_action()"); \
    gtk_text_buffer_begin_user_action(b); \
  } G_STMT_END
#define END_USER_ACTION(self) \
  G_STMT_START { \
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self)); \
    IDE_TRACE_MSG ("end_user_action()"); \
    gtk_text_buffer_end_user_action(b); \
  } G_STMT_END

#define _GDK_RECTANGLE_X2(rect) ((rect)->x + (rect)->width)
#define _GDK_RECTANGLE_Y2(rect) ((rect)->y + (rect)->height)
#define _GDK_RECTANGLE_CONTAINS(rect,other) \
  (((rect)->x <= (other)->x) && \
   (_GDK_RECTANGLE_X2(rect) >= _GDK_RECTANGLE_X2(other)) && \
   ((rect)->y <= (other)->y) && \
   (_GDK_RECTANGLE_Y2(rect) >= _GDK_RECTANGLE_Y2(other)))
#define _GDK_RECTANGLE_CENTER_X(rect) ((rect)->x + ((rect)->width/2))
#define _GDK_RECTANGLE_CENTER_Y(rect) ((rect)->y + ((rect)->height/2))
#define TRACE_RECTANGLE(name, rect) \
  IDE_TRACE_MSG ("%s = Rectangle(x=%d, y=%d, width=%d, height=%d)", \
                 name, (rect)->x, (rect)->y, (rect)->width, (rect)->height)

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
  GQueue                      *selections;
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

  guint                        change_sequence;

  gint                         target_line_offset;
  guint                        count;

  guint                        scroll_offset;
  gint                         cached_char_height;
  gint                         cached_char_width;

  guint                        saved_line;
  guint                        saved_line_offset;

  guint                        auto_indent : 1;
  guint                        completion_visible : 1;
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
  PROP_SCROLL_OFFSET,
  PROP_SHOW_GRID_LINES,
  PROP_SHOW_LINE_CHANGES,
  PROP_SNIPPET_COMPLETION,
  LAST_PROP
};

enum {
  ACTION,
  APPEND_TO_COUNT,
  AUTO_INDENT,
  CHANGE_CASE,
  CLEAR_COUNT,
  CLEAR_SELECTION,
  CYCLE_COMPLETION,
  DELETE_SELECTION,
  INDENT_SELECTION,
  INSERT_AT_CURSOR_AND_INDENT,
  JOIN_LINES,
  JUMP,
  MOVEMENT,
  PASTE_CLIPBOARD_EXTENDED,
  POP_SELECTION,
  PUSH_SELECTION,
  PUSH_SNIPPET,
  POP_SNIPPET,
  RESTORE_INSERT_MARK,
  SAVE_INSERT_MARK,
  SELECTION_THEATRIC,
  SET_MODE,
  SET_OVERWRITE,
  SORT,
  SWAP_SELECTION_BOUNDS,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void ide_source_view_real_set_mode (IdeSourceView         *self,
                                           const gchar           *name,
                                           IdeSourceViewModeType  type);

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

static gboolean
ide_source_view_get_at_bottom (IdeSourceView *self)
{
  GtkAdjustment *vadj;
  gdouble value;
  gdouble page_size;
  gdouble upper;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));
  value = gtk_adjustment_get_value (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  page_size = gtk_adjustment_get_page_size (vadj);

  return ((value + page_size) == upper);
}

static void
ide_source_view_scroll_to_bottom__changed_cb (GtkAdjustment *vadj,
                                              GParamSpec    *pspec,
                                              gpointer       user_data)
{
  gdouble page_size;
  gdouble upper;
  gdouble value;

  g_assert (GTK_IS_ADJUSTMENT (vadj));

  g_signal_handlers_disconnect_by_func (vadj,
                                        G_CALLBACK (ide_source_view_scroll_to_bottom__changed_cb),
                                        NULL);

  page_size = gtk_adjustment_get_page_size (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  value = upper - page_size;

  gtk_adjustment_set_value (vadj, value);
}

static void
ide_source_view_scroll_to_bottom (IdeSourceView *self)
{
  GtkAdjustment *vadj;
  gdouble page_size;
  gdouble upper;
  gdouble value;
  gdouble new_value;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));
  upper = gtk_adjustment_get_upper (vadj);
  page_size = gtk_adjustment_get_page_size (vadj);
  value = gtk_adjustment_get_value (vadj);
  new_value = upper - page_size;

  if (new_value == value)
    {
      /*
       * HACK:
       *
       * GtkTextView wont calculate the new heights until an idle handler.
       * So wait until that happens and then jump.
       */
      g_signal_connect (vadj,
                        "notify::upper",
                        G_CALLBACK (ide_source_view_scroll_to_bottom__changed_cb),
                        NULL);
      return;
    }

  gtk_adjustment_set_value (vadj, new_value);
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
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (iter1);
  g_assert (iter2);
  g_assert (rect);

  begin = *iter1;
  end = *iter2;

  if (gtk_text_iter_equal (&begin, &end))
    goto finish;

  gtk_text_iter_order (&begin, &end);

  if (gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&end))
    {
      gtk_text_view_get_iter_location (text_view, &begin, &area);
      gtk_text_view_get_iter_location (text_view, &end, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);
      goto finish;
    }

  gtk_text_view_get_iter_location (text_view, &begin, &area);

  iter = begin;

  do
    {
      /* skip trailing newline */
      if ((gtk_text_iter_starts_line (&iter) && gtk_text_iter_equal (&iter, &end)))
        break;

      gtk_text_view_get_iter_location (text_view, &iter, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);

      gtk_text_iter_forward_to_line_end (&iter);
      gtk_text_view_get_iter_location (text_view, &iter, &tmp);
      gdk_rectangle_union (&area, &tmp, &area);

      if (!gtk_text_iter_forward_char (&iter))
        break;
    }
  while (gtk_text_iter_compare (&iter, &end) <= 0);

finish:
  gtk_text_view_buffer_to_window_coords (text_view, window_type, area.x, area.y, &area.x, &area.y);

  *rect = area;
}

static void
animate_expand (IdeSourceView     *self,
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
animate_shrink (IdeSourceView     *self,
                const GtkTextIter *begin,
                const GtkTextIter *end)
{
  IdeBoxTheatric *theatric;
  GtkAllocation alloc;
  GdkRectangle rect = { 0 };
  GdkRectangle char_rect = { 0 };
  GtkTextIter copy_begin;
  GtkTextIter copy_end;
  gboolean is_whole_line;
  gboolean is_single_line;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (begin);
  g_assert (end);

  get_rect_for_iters (GTK_TEXT_VIEW (self), begin, begin, &char_rect, GTK_TEXT_WINDOW_WIDGET);
  get_rect_for_iters (GTK_TEXT_VIEW (self), begin, end, &rect, GTK_TEXT_WINDOW_WIDGET);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  rect.height = MIN (rect.height, alloc.height - rect.y);

  copy_begin = *begin;
  copy_end = *end;

  gtk_text_iter_order (&copy_begin, &copy_end);

  is_single_line = (gtk_text_iter_get_line (&copy_begin) == gtk_text_iter_get_line (&copy_end));
  is_whole_line = ((gtk_text_iter_get_line (&copy_begin) + 1 ==
                    gtk_text_iter_get_line (&copy_end)) &&
                   (gtk_text_iter_starts_line (&copy_begin) &&
                    gtk_text_iter_starts_line (&copy_end)));

  theatric = g_object_new (IDE_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", rect.height,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  if (is_whole_line)
    ide_object_animate_full (theatric,
                             IDE_ANIMATION_EASE_OUT_QUAD,
                             150,
                             gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                             g_object_unref,
                             theatric,
                             "x", rect.x,
                             "width", rect.width,
                             "y", rect.y,
                             "height", 0,
                             "alpha", 0.3,
                             NULL);
  else if (is_single_line)
    ide_object_animate_full (theatric,
                             IDE_ANIMATION_EASE_OUT_QUAD,
                             150,
                             gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                             g_object_unref,
                             theatric,
                             "x", rect.x,
                             "width", 0,
                             "y", rect.y,
                             "height", rect.height,
                             "alpha", 0.3,
                             NULL);
  else
    ide_object_animate_full (theatric,
                             IDE_ANIMATION_EASE_OUT_QUAD,
                             150,
                             gtk_widget_get_frame_clock (GTK_WIDGET (self)),
                             g_object_unref,
                             theatric,
                             "x", rect.x,
                             "width", 0,
                             "y", rect.y,
                             "height", char_rect.height,
                             "alpha", 0.3,
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
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  priv->change_sequence++;
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

  if (priv->mode && ide_source_view_mode_get_coalesce_undo (priv->mode))
    BEGIN_USER_ACTION (self);
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
  g_autofree gchar *indent = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean at_bottom;
  gint cursor_offset = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (event);

  at_bottom = ide_source_view_get_at_bottom (self);

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
      gtk_text_buffer_end_user_action (buffer);

      /*
       * Make sure we stay in the visible rect.
       */
      ide_source_view_scroll_mark_onscreen (self, insert);

      /*
       * Keep our selves pinned to the bottom of the document if that makes sense.
       */
      if (at_bottom)
        ide_source_view_scroll_to_bottom (self);

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

  IDE_EXIT;
}

static gboolean
ide_source_view_do_mode (IdeSourceView *self,
                         GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean ret = FALSE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

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
            {
              if (ide_source_view_mode_get_coalesce_undo (mode))
                END_USER_ACTION (self);
              g_clear_object (&priv->mode);
            }
        }

      g_object_unref (mode);

      if (handled)
        ret = TRUE;
    }

  if (!priv->mode)
    ide_source_view_real_set_mode (self, NULL,  IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);

  return ret;
}

static gboolean
ide_source_view_key_press_event (GtkWidget   *widget,
                                 GdkEventKey *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  IdeSourceSnippet *snippet;
  gboolean ret = FALSE;
  guint change_sequence;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);

  /*
   * Check our current change sequence. If the buffer has changed during the
   * key-press handler, we'll refocus our selves at the insert caret.
   */
  change_sequence = priv->change_sequence;

  /*
   * If we are in a non-default mode, dispatch the event to the mode. This allows custom
   * keybindings like Emacs and Vim to be implemented using gtk-bindings CSS.
   */
  if (ide_source_view_do_mode (self, event))
    return TRUE;

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

  if (priv->change_sequence != change_sequence)
    ide_source_view_scroll_mark_onscreen (self, insert);

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
ide_source_view_real_style_updated (GtkWidget *widget)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  PangoContext *context;
  PangoLayout *layout;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  GTK_WIDGET_CLASS (ide_source_view_parent_class)->style_updated (widget);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);
  pango_layout_set_text (layout, "X", 1);
  pango_layout_get_pixel_size (layout, &priv->cached_char_width, &priv->cached_char_height);
  g_object_unref (layout);
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
ide_source_view_real_append_to_count (IdeSourceView *self,
                                      gint           digit)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_return_if_fail (digit >= 0);
  g_return_if_fail (digit <= 9);

  priv->count = (priv->count * 10) + digit;
}

static void
ide_source_view_real_auto_indent (IdeSourceView *self)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  if (!gtk_text_iter_is_start (&iter))
    {
      GdkEvent fake_event = { 0 };
      GtkTextIter copy;
      gchar str[8] = { 0 };
      gunichar ch;

      copy = iter;

      gtk_text_iter_backward_char (&copy);
      ch = gtk_text_iter_get_char (&copy);
      g_unichar_to_utf8 (ch, str);

      /*
       * Now delete the character since the indenter will take care of
       * reinserting it based on the GdkEventKey.
       */
      gtk_text_buffer_delete (buffer, &copy, &iter);

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
      fake_event.key.string = str;
      fake_event.key.hardware_keycode = 0;
      fake_event.key.group = 0;
      fake_event.key.is_modifier = 0;

      /* Be nice during the common case */
      if (*str == '\n')
        fake_event.key.keyval = GDK_KEY_KP_Enter;
      else
        fake_event.key.keyval = gdk_unicode_to_keyval (ch);

      ide_source_view_do_indent (self, &fake_event.key);
    }
}

static void
ide_source_view_real_change_case (IdeSourceView           *self,
                                  GtkSourceChangeCaseType  type)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (GTK_SOURCE_IS_BUFFER (buffer))
    gtk_source_buffer_change_case (GTK_SOURCE_BUFFER (buffer), type, &begin, &end);
}

static void
ide_source_view_real_clear_count (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  priv->count = 0;
}

static void
ide_source_view_real_clear_selection (IdeSourceView *self)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GTK_IS_TEXT_VIEW (text_view));

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_view_scroll_mark_onscreen (text_view, insert);
}

static void
ide_source_view_real_cycle_completion (IdeSourceView    *self,
                                       GtkDirectionType  direction)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceView *source_view = (GtkSourceView *)self;
  GtkSourceCompletion *completion;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  completion = gtk_source_view_get_completion (source_view);

  if (!priv->completion_visible)
    {
      g_signal_emit_by_name (self, "show-completion");
      return;
    }

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_DOWN:
      g_signal_emit_by_name (completion, "move-cursor", GTK_SCROLL_STEPS, 1);
      break;

    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_UP:
      g_signal_emit_by_name (completion, "move-cursor", GTK_SCROLL_STEPS, -1);
      break;

    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
    default:
      break;
    }
}

static void
ide_source_view_real_delete_selection (IdeSourceView *self)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  gboolean editable;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));

  editable = gtk_text_view_get_editable (text_view);
  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_delete_selection (buffer, TRUE, editable);
}

static void
ide_source_view_real_indent_selection (IdeSourceView *self,
                                       gint           level)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceView *source_view = (GtkSourceView *)self;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  /*
   * Use count to increase direction.
   */
  if (priv->count)
    level = priv->count * ((level > 0) ? 1 : -1);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (level < 0)
    {
      for (; level < 0; level++)
        {
          if (gtk_text_buffer_get_selection_bounds (buffer, &iter, &selection))
            gtk_source_view_unindent_lines (source_view, &iter, &selection);
        }
    }
  else
    {
      for (; level > 0; level--)
        {
          if (gtk_text_buffer_get_selection_bounds (buffer, &iter, &selection))
            gtk_source_view_indent_lines (source_view, &iter, &selection);
        }
    }
}

static void
ide_source_view_real_insert_at_cursor_and_indent (IdeSourceView *self,
                                                  const gchar   *str)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  gboolean at_bottom;
  GdkEvent fake_event = { 0 };
  GString *gstr;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (str);

  buffer = gtk_text_view_get_buffer (text_view);

  at_bottom = ide_source_view_get_at_bottom (self);

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
      IDE_GOTO (maybe_scroll);
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

maybe_scroll:
  ide_source_view_scroll_mark_onscreen (self, gtk_text_buffer_get_insert (buffer));
  if (at_bottom)
    ide_source_view_scroll_to_bottom (self);

  IDE_EXIT;
}

static void
ide_source_view_real_join_lines (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  guint line;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    return;

  gtk_text_iter_order (&begin, &end);

  line = gtk_text_iter_get_line (&begin);

  if (gtk_text_iter_starts_line (&end) && !gtk_text_iter_ends_line (&end))
    gtk_text_iter_backward_char (&end);

  if (GTK_SOURCE_IS_BUFFER (buffer))
    gtk_source_buffer_join_lines (GTK_SOURCE_BUFFER (buffer), &begin, &end);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_set_line (&begin, line);
  gtk_text_buffer_select_range (buffer, &begin, &begin);
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
ide_source_view_real_paste_clipboard_extended (IdeSourceView *self,
                                               gboolean       smart_lines,
                                               gboolean       after_cursor,
                                               gboolean       place_cursor_at_original)

{
  GtkTextView *text_view = (GtkTextView *)self;
  g_autofree gchar *text = NULL;
  GtkClipboard *clipboard;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint target_line;
  guint target_line_offset;

  /*
   * NOTE:
   *
   * In this function, we try to improve how pasteing works in GtkTextView. There are some
   * semenatics that make things easier by tracking the paste of an entire line verses small
   * snippets of text.
   *
   * Basically, we are implementing something close to Vim. However that is not a strict
   * requirement, just what we are starting with. In fact, the rest of the handling to be like vim
   * is handled within vim.css (for example, what character to leave the insert mark on).
   */

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self), GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  target_line = gtk_text_iter_get_line (&iter);
  target_line_offset = gtk_text_iter_get_line_offset (&iter);

  gtk_text_buffer_begin_user_action (buffer);

  /*
   * If we are pasting an entire line, we don't want to paste it at the current location. We want
   * to insert a new line after the current line, and then paste it there (so move the insert mark
   * first).
   */
  if (smart_lines && text && g_str_has_suffix (text, "\n"))
    {
      g_autofree gchar *trimmed = NULL;

      /*
       * WORKAROUND:
       *
       * This is a hack so that we can continue to use the paste code from within GtkTextBuffer.
       *
       * We needed to keep the trailing \n in the text so that we know when we are selecting whole
       * lines. We also need to insert a new line manually based on the context. Furthermore, we
       * need to remove the trailing line since we already added one.
       *
       * Terribly annoying, but the result is something that feels very nice, similar to Vim.
       */
      trimmed = g_strndup (text, strlen (text) - 1);

      if (after_cursor)
        {
          if (!gtk_text_iter_ends_line (&iter))
            gtk_text_iter_forward_to_line_end (&iter);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
          g_signal_emit_by_name (self, "insert-at-cursor", "\n");
        }
      else
        {
          gtk_text_iter_set_line_offset (&iter, 0);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
          g_signal_emit_by_name (self, "insert-at-cursor", "\n");
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          gtk_text_iter_backward_line (&iter);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
        }

      if (!place_cursor_at_original)
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          target_line = gtk_text_iter_get_line (&iter);
          target_line_offset = gtk_text_iter_get_line_offset (&iter);
        }

      gtk_clipboard_set_text (clipboard, trimmed, -1);
      GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->paste_clipboard (text_view);
      gtk_clipboard_set_text (clipboard, text, -1);
    }
  else
    {
      if (after_cursor)
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          gtk_text_iter_forward_char (&iter);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
        }

      GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->paste_clipboard (text_view);

      if (!place_cursor_at_original)
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          target_line = gtk_text_iter_get_line (&iter);
          target_line_offset = gtk_text_iter_get_line_offset (&iter);
        }
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, target_line);
  for (; target_line_offset; target_line_offset--)
    if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
      break;
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  gtk_text_buffer_end_user_action (buffer);
}

static void
ide_source_view_real_selection_theatric (IdeSourceView         *self,
                                         IdeSourceViewTheatric  theatric)
{
  GtkSettings *settings;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean enable_animations = TRUE;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert ((theatric == IDE_SOURCE_VIEW_THEATRIC_EXPAND) ||
            (theatric == IDE_SOURCE_VIEW_THEATRIC_SHRINK));

  settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (self)));
  g_object_get (settings, "gtk-enable-animations", &enable_animations, NULL);

  if (!enable_animations)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    return;

  if (gtk_text_iter_starts_line (&end))
    gtk_text_iter_backward_char (&end);

  switch (theatric)
    {
    case IDE_SOURCE_VIEW_THEATRIC_EXPAND:
      animate_expand (self, &begin, &end);
      break;

    case IDE_SOURCE_VIEW_THEATRIC_SHRINK:
      animate_shrink (self, &begin, &end);
      break;

    default:
      break;
    }
}

static void
ide_source_view_real_set_mode (IdeSourceView         *self,
                               const gchar           *mode,
                               IdeSourceViewModeType  type)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean overwrite;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

#ifndef IDE_DISABLE_TRACE
  {
    const gchar *old_mode = "null";

    if (priv->mode)
      old_mode = ide_source_view_mode_get_name (priv->mode);
    IDE_TRACE_MSG ("transition from mode (%s) to (%s)", old_mode, mode ?: "<default>");
  }
#endif

  if (priv->mode)
    {
      IdeSourceViewMode *old_mode = g_object_ref (priv->mode);

      g_clear_object (&priv->mode);
      if (ide_source_view_mode_get_coalesce_undo (old_mode))
        END_USER_ACTION (self);
      g_object_unref (old_mode);
    }

  if (mode == NULL)
    {
      mode = "default";
      type = IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT;
    }

  /* reset the count when switching to permanent mode */
  if (type == IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT)
    priv->count = 0;

  priv->mode = _ide_source_view_mode_new (GTK_WIDGET (self), mode, type);

  if (ide_source_view_mode_get_coalesce_undo (priv->mode))
    BEGIN_USER_ACTION (self);

  overwrite = ide_source_view_mode_get_block_cursor (priv->mode);
  if (overwrite != gtk_text_view_get_overwrite (GTK_TEXT_VIEW (self)))
    gtk_text_view_set_overwrite (GTK_TEXT_VIEW (self), overwrite);

  IDE_EXIT;
}

static void
ide_source_view_real_set_overwrite (IdeSourceView *self,
                                    gboolean       overwrite)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  IDE_TRACE_MSG ("Setting overwrite to %s", overwrite ? "TRUE" : "FALSE");

  gtk_text_view_set_overwrite (GTK_TEXT_VIEW (self), overwrite);
}

static void
ide_source_view_real_swap_selection_bounds (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextIter insert;
  GtkTextIter selection_bound;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_selection_bounds (buffer, &insert, &selection_bound);
  gtk_text_buffer_select_range (buffer, &selection_bound, &insert);
}

static void
ide_source_view_real_movement (IdeSourceView         *self,
                               IdeSourceViewMovement  movement,
                               gboolean               extend_selection,
                               gboolean               exclusive,
                               gboolean               apply_count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  guint count = 0;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (apply_count)
    count = priv->count;

  _ide_source_view_apply_movement (self, movement, extend_selection, exclusive,
                                   count, &priv->target_line_offset);
}

static void
ide_source_view_real_restore_insert_mark (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint line_offset;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_iter_at_line (buffer, &iter, priv->saved_line);

  line_offset = priv->saved_line_offset;

  for (; line_offset; line_offset--)
    {
      if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
        break;
    }

  gtk_text_buffer_select_range (buffer, &iter, &iter);

  insert = gtk_text_buffer_get_insert (buffer);
  ide_source_view_scroll_mark_onscreen (self, insert);
}

static void
ide_source_view_real_save_insert_mark (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  priv->saved_line = gtk_text_iter_get_line (&iter);
  priv->saved_line_offset = gtk_text_iter_get_line_offset (&iter);
}

static void
ide_source_view__completion_hide_cb (IdeSourceView       *self,
                                     GtkSourceCompletion *completion)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  priv->completion_visible = FALSE;
}

static void
ide_source_view__completion_show_cb (IdeSourceView       *self,
                                     GtkSourceCompletion *completion)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  priv->completion_visible = TRUE;
}

static void
ide_source_view_real_pop_selection (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection_bound;
  GtkTextIter insert_iter;
  GtkTextIter selection_bound_iter;
  gpointer *data;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  data = g_queue_pop_head (priv->selections);

  if (!data)
    {
      g_warning ("request to pop selection that does not exist!");
      return;
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  insert = gtk_text_buffer_get_insert (buffer);
  selection_bound = gtk_text_buffer_get_selection_bound (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, data [0]);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound_iter, data [1]);

  gtk_text_buffer_move_mark (buffer, insert, &insert_iter);
  gtk_text_buffer_move_mark (buffer, selection_bound, &selection_bound_iter);

  gtk_text_buffer_delete_mark (buffer, data [0]);
  gtk_text_buffer_delete_mark (buffer, data [1]);

  g_object_unref (data [0]);
  g_object_unref (data [1]);
  g_free (data);
}

static void
ide_source_view_real_push_selection (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection_bound;
  GtkTextIter insert_iter;
  GtkTextIter selection_bound_iter;
  gpointer *data;
  gboolean left_gravity;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert);

  selection_bound = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_bound_iter, selection_bound);

  left_gravity = (gtk_text_iter_compare (&insert_iter, &selection_bound_iter) <= 0);
  insert = gtk_text_buffer_create_mark (buffer, NULL, &insert_iter, left_gravity);

  left_gravity = (gtk_text_iter_compare (&selection_bound_iter, &insert_iter) < 0);
  selection_bound = gtk_text_buffer_create_mark (buffer, NULL, &selection_bound_iter, left_gravity);

  data = g_new0 (gpointer, 2);
  data [0] = g_object_ref (insert);
  data [1] = g_object_ref (selection_bound);

  g_queue_push_head (priv->selections, data);
}

static void
ide_source_view_real_push_snippet (IdeSourceView           *self,
                                   IdeSourceSnippet        *snippet,
                                   IdeSourceSnippetContext *context,
                                   const GtkTextIter       *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (IDE_IS_SOURCE_SNIPPET_CONTEXT (context));

  if (priv->buffer != NULL)
    {
      IdeFile *file;

      file = ide_buffer_get_file (priv->buffer);

      if (file != NULL)
        {
          GFile *gfile;

          gfile = ide_file_get_file (file);

          if (gfile != NULL)
            {
              gchar *name = NULL;

              name = g_file_get_basename (gfile);
              ide_source_snippet_context_add_variable (context, "filename", name);
              g_free (name);
            }
        }
    }
}

static void
ide_source_view_constructed (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceGutter *gutter;
  GtkSourceCompletion *completion;
  gboolean visible;

  G_OBJECT_CLASS (ide_source_view_parent_class)->constructed (object);

  ide_source_view_real_set_mode (self, NULL, IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);

  /*
   * Completion does not have a way to retrieve visibility, so we need to track that ourselves
   * by connecting to hide/show. We use this to know if we need to move to the next item in the
   * result set during IdeSourceView:cycle-completion.
   */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  g_signal_connect_object (completion,
                           "show",
                           G_CALLBACK (ide_source_view__completion_show_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_signal_connect_object (completion,
                           "hide",
                           G_CALLBACK (ide_source_view__completion_hide_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

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
ide_source_view_real_undo (GtkSourceView *source_view)
{
  IdeSourceView *self = (IdeSourceView *)source_view;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean needs_unlock = !!priv->mode;

  if (needs_unlock)
    END_USER_ACTION (self);
  GTK_SOURCE_VIEW_CLASS (ide_source_view_parent_class)->undo (source_view);
  if (needs_unlock)
    BEGIN_USER_ACTION (self);
}

static void
ide_source_view_real_redo (GtkSourceView *source_view)
{
  IdeSourceView *self = (IdeSourceView *)source_view;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean needs_unlock = !!priv->mode;

  if (needs_unlock)
    END_USER_ACTION (self);
  GTK_SOURCE_VIEW_CLASS (ide_source_view_parent_class)->redo (source_view);
  if (needs_unlock)
    BEGIN_USER_ACTION (self);
}

static void
ide_source_view_real_insert_at_cursor (GtkTextView *text_view,
                                       const gchar *str)
{
  IdeSourceView *self = (IdeSourceView *)text_view;
  GtkTextBuffer *buffer;
  gboolean at_bottom;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (str);

  at_bottom = ide_source_view_get_at_bottom (self);

  GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->insert_at_cursor (text_view, str);

  buffer = gtk_text_view_get_buffer (text_view);
  ide_source_view_scroll_mark_onscreen (self, gtk_text_buffer_get_insert (buffer));

  if (at_bottom)
    ide_source_view_scroll_to_bottom (self);
}

static int
_strcasecmp_reversed (const void *aptr,
                      const void *bptr)
{
  const gchar * const *a = aptr;
  const gchar * const *b = bptr;

  return -strcasecmp (*a, *b);
}

static int
_strcasecmp_normal (const void *aptr,
                    const void *bptr)
{
  const gchar * const *a = aptr;
  const gchar * const *b = bptr;

  return strcasecmp (*a, *b);
}

static int
_strcmp_reversed (const void *aptr,
                  const void *bptr)
{
  const gchar * const *a = aptr;
  const gchar * const *b = bptr;

  return -strcmp (*a, *b);
}

static int
_strcmp_normal (const void *aptr,
                const void *bptr)
{
  const gchar * const *a = aptr;
  const gchar * const *b = bptr;

  return strcmp (*a, *b);
}

static void
ide_source_view_real_sort (IdeSourceView *self,
                           gboolean       ignore_case,
                           gboolean       reverse)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter cursor;
  int (*sort_func) (const void *, const void *) = _strcmp_normal;
  guint cursor_offset;
  gchar *text;
  gchar **parts;

  g_assert (GTK_TEXT_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    gtk_text_buffer_get_bounds (buffer, &begin, &end);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, insert);
  cursor_offset = gtk_text_iter_get_offset (&cursor);

  gtk_text_iter_order (&begin, &end);
  if (gtk_text_iter_starts_line (&end))
    gtk_text_iter_backward_char (&end);

  text = gtk_text_iter_get_slice (&begin, &end);
  parts = g_strsplit (text, "\n", 0);
  g_free (text);

  if (reverse && ignore_case)
    sort_func = _strcasecmp_reversed;
  else if (ignore_case)
    sort_func = _strcasecmp_normal;
  else
    sort_func = _strcmp_reversed;

  qsort (parts, g_strv_length (parts), sizeof (gchar *), sort_func);

  text = g_strjoinv ("\n", parts);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, text, -1);
  g_free (text);
  g_strfreev (parts);
  gtk_text_buffer_get_iter_at_offset (buffer, &begin, cursor_offset);
  gtk_text_buffer_select_range (buffer, &begin, &begin);
  gtk_text_buffer_end_user_action (buffer);
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
  g_clear_pointer (&priv->selections, g_queue_free);
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

    case PROP_SCROLL_OFFSET:
      g_value_set_uint (value, ide_source_view_get_scroll_offset (self));
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

    case PROP_SCROLL_OFFSET:
      ide_source_view_set_scroll_offset (self, g_value_get_uint (value));
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
  GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);
  GtkSourceViewClass *source_view_class = GTK_SOURCE_VIEW_CLASS (klass);

  object_class->constructed = ide_source_view_constructed;
  object_class->dispose = ide_source_view_dispose;
  object_class->finalize = ide_source_view_finalize;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  widget_class->key_press_event = ide_source_view_key_press_event;
  widget_class->query_tooltip = ide_source_view_query_tooltip;
  widget_class->style_updated = ide_source_view_real_style_updated;

  text_view_class->insert_at_cursor = ide_source_view_real_insert_at_cursor;

  source_view_class->undo = ide_source_view_real_undo;
  source_view_class->redo = ide_source_view_real_redo;

  klass->action = ide_source_view_real_action;
  klass->append_to_count = ide_source_view_real_append_to_count;
  klass->auto_indent = ide_source_view_real_auto_indent;
  klass->change_case = ide_source_view_real_change_case;
  klass->clear_count = ide_source_view_real_clear_count;
  klass->clear_selection = ide_source_view_real_clear_selection;
  klass->cycle_completion = ide_source_view_real_cycle_completion;
  klass->delete_selection = ide_source_view_real_delete_selection;
  klass->indent_selection = ide_source_view_real_indent_selection;
  klass->insert_at_cursor_and_indent = ide_source_view_real_insert_at_cursor_and_indent;
  klass->join_lines = ide_source_view_real_join_lines;
  klass->jump = ide_source_view_real_jump;
  klass->movement = ide_source_view_real_movement;
  klass->paste_clipboard_extended = ide_source_view_real_paste_clipboard_extended;
  klass->pop_selection = ide_source_view_real_pop_selection;
  klass->push_selection = ide_source_view_real_push_selection;
  klass->push_snippet = ide_source_view_real_push_snippet;
  klass->restore_insert_mark = ide_source_view_real_restore_insert_mark;
  klass->save_insert_mark = ide_source_view_real_save_insert_mark;
  klass->selection_theatric = ide_source_view_real_selection_theatric;
  klass->set_mode = ide_source_view_real_set_mode;
  klass->set_overwrite = ide_source_view_real_set_overwrite;
  klass->sort = ide_source_view_real_sort;
  klass->swap_selection_bounds = ide_source_view_real_swap_selection_bounds;

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

  gParamSpecs [PROP_SCROLL_OFFSET] =
    g_param_spec_uint ("scroll-offset",
                       _("Scroll Offset"),
                       _("The number of lines between the insertion cursor and screen boundary."),
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SCROLL_OFFSET,
                                   gParamSpecs [PROP_SCROLL_OFFSET]);

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

  gSignals [ACTION] =
    g_signal_new ("action",
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

  gSignals [APPEND_TO_COUNT] =
    g_signal_new ("append-to-count",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, append_to_count),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  /**
   * IdeSourceView::auto-indent:
   *
   * Requests that the auto-indenter perform an indent request using the last
   * inserted character. For example, if on the first character of a line, the
   * last inserted character would be a newline and therefore "\n".
   *
   * If on the first character of the buffer, this signal will do nothing.
   */
  gSignals [AUTO_INDENT] =
    g_signal_new ("auto-indent",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, auto_indent),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [CHANGE_CASE] =
    g_signal_new ("change-case",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, change_case),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  GTK_SOURCE_TYPE_CHANGE_CASE_TYPE);

  gSignals [CLEAR_COUNT] =
    g_signal_new ("clear-count",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_count),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [CLEAR_SELECTION] =
    g_signal_new ("clear-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_selection),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [CYCLE_COMPLETION] =
    g_signal_new ("cycle-completion",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, cycle_completion),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  gSignals [DELETE_SELECTION] =
    g_signal_new ("delete-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, delete_selection),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [INDENT_SELECTION] =
    g_signal_new ("indent-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, indent_selection),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

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

  gSignals [JOIN_LINES] =
    g_signal_new ("join-lines",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, join_lines),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

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
                  4,
                  IDE_TYPE_SOURCE_VIEW_MOVEMENT,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  gSignals [PASTE_CLIPBOARD_EXTENDED] =
    g_signal_new ("paste-clipboard-extended",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, paste_clipboard_extended),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  3,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  /**
   * IdeSourceView::pop-selection:
   *
   * Reselects a previousl selected range of text that was saved using
   * IdeSourceView::push-selection.
   */
  gSignals [POP_SELECTION] =
    g_signal_new ("pop-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, pop_selection),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::push-selection:
   *
   * Saves the current selection away to be restored by a call to
   * IdeSourceView::pop-selection. You must pop the selection to keep
   * the selection stack in consistent order.
   */
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

  gSignals [PUSH_SELECTION] =
    g_signal_new ("push-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, push_selection),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

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

  gSignals [RESTORE_INSERT_MARK] =
    g_signal_new ("restore-insert-mark",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, restore_insert_mark),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [SAVE_INSERT_MARK] =
    g_signal_new ("save-insert-mark",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, save_insert_mark),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  gSignals [SELECTION_THEATRIC] =
    g_signal_new ("selection-theatric",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, selection_theatric),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SOURCE_VIEW_THEATRIC);

  gSignals [SET_MODE] =
    g_signal_new ("set-mode",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_mode),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  IDE_TYPE_SOURCE_VIEW_MODE_TYPE);

  gSignals [SET_OVERWRITE] =
    g_signal_new ("set-overwrite",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_overwrite),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  /**
   * IdeSourceView::sort:
   * @self: an #IdeSourceView.
   * @ignore_case: If character case should be ignored.
   * @reverse: If the lines should be sorted in reverse order
   *
   * This signal is meant to be activated from keybindings to sort the currently selected lines.
   * The lines are sorted using qsort() and either strcmp() or strcasecmp().
   */
  gSignals [SORT] =
    g_signal_new ("sort",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, sort),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  gSignals [SWAP_SELECTION_BOUNDS] =
    g_signal_new ("swap-selection-bounds",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, swap_selection_bounds),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  priv->target_line_offset = -1;
  priv->snippets = g_queue_new ();
  priv->selections = g_queue_new ();

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
  gboolean at_bottom;
  gchar *line_prefix;
  guint tab_width;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (snippet));

  at_bottom = ide_source_view_get_at_bottom (self);

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

    animate_expand (self, &begin, &end);
  }

  if (!has_more_tab_stops)
    ide_source_view_pop_snippet (self);

  ide_source_view_invalidate_window (self);

  if (at_bottom)
    ide_source_view_scroll_to_bottom (self);
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

/**
 * ide_source_view_get_scroll_offset:
 *
 * Gets the #IdeSourceView:scroll-offset property. This property contains the number of lines
 * that should be kept above or below the line containing the insertion cursor relative to the
 * top and bottom of the visible text window.
 */
guint
ide_source_view_get_scroll_offset (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), 0);

  return priv->scroll_offset;
}

/**
 * ide_source_view_set_scroll_offset:
 *
 * Sets the #IdeSourceView:scroll-offset property. See ide_source_view_get_scroll_offset() for
 * more information. Set to 0 to unset this property.
 */
void
ide_source_view_set_scroll_offset (IdeSourceView *self,
                                   guint          scroll_offset)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (scroll_offset != priv->scroll_offset)
    {
      priv->scroll_offset = scroll_offset;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SCROLL_OFFSET]);
    }
}

/**
 * ide_source_view_get_visible_rect:
 * @self: An #IdeSourceView.
 * @visible_rect: (out): A #GdkRectangle.
 *
 * Gets the visible region in buffer coordinates that is the visible area of the buffer. This
 * is similar to gtk_text_view_get_visible_area() except that it takes into account the
 * #IdeSourceView:scroll-offset property to ensure there is space above and below the
 * visible_rect.
 */
void
ide_source_view_get_visible_rect (IdeSourceView *self,
                                  GdkRectangle  *visible_rect)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GdkRectangle area;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (visible_rect);

  gtk_text_view_get_visible_rect (text_view, &area);

  /*
   * If we don't have valid line height, not much we can do now. We can just adjust things
   * later once it becomes available.
   */
  if (priv->cached_char_height)
    {
      gint max_scroll_offset;
      gint scroll_offset;
      gint visible_lines;
      gint scroll_offset_height;

      visible_lines = area.height / priv->cached_char_height;
      max_scroll_offset = (visible_lines - 1) / 2;
      scroll_offset = MIN (priv->scroll_offset, max_scroll_offset);
      scroll_offset_height = priv->cached_char_height * scroll_offset;

      area.y += scroll_offset_height;
      area.height -= (2 * scroll_offset_height);

      /*
       * Use a multiple of the line height so we don't jump around when
       * focusing the last line (due to Y2 not fitting in the visible area).
       */
      area.height = (area.height / priv->cached_char_height) * priv->cached_char_height;
    }

  *visible_rect = area;
}

void
ide_source_view_scroll_mark_onscreen (IdeSourceView *self,
                                      GtkTextMark   *mark)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GdkRectangle visible_rect;
  GdkRectangle mark_rect;
  GtkTextIter iter;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  ide_source_view_get_visible_rect (self, &visible_rect);

  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  gtk_text_view_get_iter_location (text_view, &iter, &mark_rect);

  if (!_GDK_RECTANGLE_CONTAINS (&visible_rect, &mark_rect))
    ide_source_view_scroll_to_mark (self, mark, 0.0, FALSE, 0.0, 0.0);

  IDE_EXIT;
}

gboolean
ide_source_view_move_mark_onscreen (IdeSourceView *self,
                                    GtkTextMark   *mark)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter end;
  GdkRectangle visible_rect;
  GdkRectangle iter_rect;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);
  g_return_val_if_fail (GTK_IS_TEXT_MARK (mark), FALSE);

  buffer = gtk_text_view_get_buffer (text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  gtk_text_buffer_get_end_iter (buffer, &end);

  ide_source_view_get_visible_rect (self, &visible_rect);
  gtk_text_view_get_iter_location (text_view, &iter, &iter_rect);

  if (_GDK_RECTANGLE_CONTAINS (&visible_rect, &iter_rect))
    return FALSE;

  if (_GDK_RECTANGLE_Y2 (&iter_rect) > _GDK_RECTANGLE_Y2 (&visible_rect))
    gtk_text_view_get_iter_at_location (text_view, &iter,
                                        _GDK_RECTANGLE_X2 (&visible_rect),
                                        _GDK_RECTANGLE_Y2 (&visible_rect));
  else if (iter_rect.y < visible_rect.y)
    gtk_text_view_get_iter_at_location (text_view, &iter, visible_rect.x, visible_rect.y);
  else
    return gtk_text_view_move_mark_onscreen (text_view, mark);

  gtk_text_buffer_move_mark (buffer, mark, &iter);

  return TRUE;
}

void
ide_source_view_scroll_to_iter (IdeSourceView     *self,
                                const GtkTextIter *iter,
                                gdouble            within_margin,
                                gboolean           use_align,
                                gdouble            xalign,
                                gdouble            yalign)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  GdkRectangle real_visible_rect;
  GdkRectangle visible_rect;
  GdkRectangle iter_rect;
  gdouble yvalue;
  gdouble xvalue;
  gint xoffset;
  gint yoffset;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (iter);
  g_return_if_fail (xalign >= 0.0);
  g_return_if_fail (xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0);
  g_return_if_fail (yalign <= 1.0);

  gtk_text_view_get_visible_rect (text_view, &real_visible_rect);
  ide_source_view_get_visible_rect (self, &visible_rect);

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (self));
  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));

  gtk_text_view_get_iter_location (text_view, iter, &iter_rect);

  TRACE_RECTANGLE ("visible_rect", &visible_rect);
  TRACE_RECTANGLE ("iter_rect", &iter_rect);

  /* leave a character of room to the right of the screen */
  visible_rect.width -= priv->cached_char_width;

  if (use_align == FALSE)
    {
      if (iter_rect.y < visible_rect.y)
        yalign = 0.0;
      else if (_GDK_RECTANGLE_Y2 (&iter_rect) > _GDK_RECTANGLE_Y2 (&visible_rect))
        yalign = 1.0;
      else
        yalign = (iter_rect.y - visible_rect.y)  / (gdouble)visible_rect.height;

      IDE_TRACE_MSG ("yalign = %lf", yalign);

      if (iter_rect.x < visible_rect.x)
        {
          /* if we can get all the way to the line start, do so */
          if (_GDK_RECTANGLE_X2 (&iter_rect) < visible_rect.width)
            xalign = 1.0;
          else
            xalign = 0.0;
        }
      else if (_GDK_RECTANGLE_X2 (&iter_rect) > _GDK_RECTANGLE_X2 (&visible_rect))
        xalign = 1.0;
      else
        xalign = (iter_rect.x - visible_rect.x) / (gdouble)visible_rect.width;
    }

  g_assert_cmpint (xalign, >=, 0.0);
  g_assert_cmpint (yalign, >=, 0.0);
  g_assert_cmpint (xalign, <=, 1.0);
  g_assert_cmpint (yalign, <=, 1.0);

  /* get the screen coordinates within the real visible area */
  xoffset = (visible_rect.x - real_visible_rect.x) + (xalign * visible_rect.width);
  yoffset = (visible_rect.y - real_visible_rect.y) + (yalign * visible_rect.height);

  /*
   * now convert those back to alignments in the real visible area, but leave
   * enough space for an input character.
   */
  xalign = xoffset / (gdouble)real_visible_rect.width;
  yalign = yoffset / (gdouble)(real_visible_rect.height + priv->cached_char_height);

  yvalue = iter_rect.y - (yalign * real_visible_rect.height);
  xvalue = iter_rect.x - (xalign * real_visible_rect.width);

  gtk_adjustment_set_value (hadj, xvalue);
  gtk_adjustment_set_value (vadj, yvalue);

  IDE_EXIT;
}

void
ide_source_view_scroll_to_mark (IdeSourceView *self,
                                GtkTextMark   *mark,
                                gdouble        within_margin,
                                gboolean       use_align,
                                gdouble        xalign,
                                gdouble        yalign)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (xalign >= 0.0);
  g_return_if_fail (xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0);
  g_return_if_fail (yalign <= 1.0);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  ide_source_view_scroll_to_iter (self, &iter, within_margin, use_align, xalign, yalign);
}

gboolean
ide_source_view_place_cursor_onscreen (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);

  return ide_source_view_move_mark_onscreen (self, insert);
}
