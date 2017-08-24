/* ide-source-view.c
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

#define G_LOG_DOMAIN "ide-source-view"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "ide-context.h"
#include "ide-debug.h"
#include "ide-enums.h"
#include "ide-internal.h"

#include "application/ide-application.h"
#include "buffers/ide-buffer-manager.h"
#include "buffers/ide-buffer.h"
#include "diagnostics/ide-diagnostic.h"
#include "diagnostics/ide-fixit.h"
#include "diagnostics/ide-source-location.h"
#include "diagnostics/ide-source-range.h"
#include "files/ide-file-settings.h"
#include "files/ide-file.h"
#include "history/ide-back-forward-item.h"
#include "history/ide-back-forward-list.h"
#include "plugins/ide-extension-adapter.h"
#include "plugins/ide-extension-set-adapter.h"
#include "rename/ide-rename-provider.h"
#include "snippets/ide-source-snippet-chunk.h"
#include "snippets/ide-source-snippet-completion-provider.h"
#include "snippets/ide-source-snippet-context.h"
#include "snippets/ide-source-snippet-private.h"
#include "snippets/ide-source-snippet.h"
#include "snippets/ide-source-snippets-manager.h"
#include "sourceview/ide-completion-provider.h"
#include "sourceview/ide-indenter.h"
#include "sourceview/ide-line-change-gutter-renderer.h"
#include "sourceview/ide-line-diagnostics-gutter-renderer.h"
#include "sourceview/ide-source-iter.h"
#include "sourceview/ide-source-view-capture.h"
#include "sourceview/ide-source-view-mode.h"
#include "sourceview/ide-source-view-movements.h"
#include "sourceview/ide-source-view-private.h"
#include "sourceview/ide-source-view.h"
#include "sourceview/ide-text-util.h"
#include "sourceview/ide-cursor.h"
#include "symbols/ide-symbol.h"
#include "symbols/ide-symbol-resolver.h"
#include "util/ide-gtk.h"
#include "vcs/ide-vcs.h"
#include "workbench/ide-workbench-private.h"

#define INCLUDE_STATEMENTS "^#include[\\s]+[\\\"\\<][^\\s\\\"\\\'\\<\\>[:cntrl:]]+[\\\"\\>]"

#define DEFAULT_FONT_DESC "Monospace 11"
#define ANIMATION_X_GROW 50
#define ANIMATION_Y_GROW 30
#define SMALL_SCROLL_DURATION_MSEC 100
#define LARGE_SCROLL_DURATION_MSEC 250
#define FIXIT_LABEL_LEN_MAX 30
#define SCROLL_REPLAY_DELAY 1000
#define DEFAULT_OVERSCROLL_NUM_LINES 1
#define TAG_DEFINITION "action::hover-definition"
#define DEFINITION_HIGHLIGHT_MODIFIER GDK_CONTROL_MASK

#define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)

#define _GDK_RECTANGLE_X2(rect) dzl_cairo_rectangle_x2(rect)
#define _GDK_RECTANGLE_Y2(rect) dzl_cairo_rectangle_y2(rect)
#define _GDK_RECTANGLE_CONTAINS(rect,other) dzl_cairo_rectangle_contains_rectangle(rect,other)
#define _GDK_RECTANGLE_CENTER_X(rect) dzl_cairo_rectangle_center(rect)
#define _GDK_RECTANGLE_CENTER_Y(rect) dzl_cairo_rectangle_middle(rect)
#define TRACE_RECTANGLE(name, rect) \
  IDE_TRACE_MSG ("%s = Rectangle(x=%d, y=%d, width=%d, height=%d)", \
                 name, (rect)->x, (rect)->y, (rect)->width, (rect)->height)

typedef struct
{
  IdeBackForwardList          *back_forward_list;
  IdeBuffer                   *buffer;
  GtkCssProvider              *css_provider;
  PangoFontDescription        *font_desc;
  IdeExtensionAdapter         *indenter_adapter;
  GtkSourceGutterRenderer     *line_change_renderer;
  GtkSourceGutterRenderer     *line_diagnostics_renderer;
  IdeSourceViewCapture        *capture;
  gchar                       *display_name;
  IdeSourceViewMode           *mode;
  GList                       *providers;
  GtkTextMark                 *rubberband_mark;
  GtkTextMark                 *rubberband_insert_mark;
  GtkTextMark                 *scroll_mark;
  gchar                       *saved_search_text;
  GtkDirectionType             search_direction;
  GQueue                      *selections;
  GQueue                      *snippets;
  GtkSourceCompletionProvider *snippets_provider;
  GtkSourceSearchContext      *search_context;
  DzlAnimation                *hadj_animation;
  DzlAnimation                *vadj_animation;

  IdeExtensionSetAdapter      *completion_providers;
  DzlSignalGroup              *completion_providers_signals;

  DzlBindingGroup             *file_setting_bindings;
  DzlSignalGroup              *buffer_signals;

  guint                        change_sequence;

  guint                        target_line_column;
  GString                     *command_str;
  gunichar                     command;
  gunichar                     modifier;
  gunichar                     search_char;
  gint                         count;
  gunichar                     inner_left;
  gunichar                     inner_right;

  guint                        scroll_offset;
  gint                         cached_char_height;
  gint                         cached_char_width;

  guint                        saved_line;
  guint                        saved_line_column;
  guint                        saved_selection_line;
  guint                        saved_selection_line_column;

  GdkRGBA                      bubble_color1;
  GdkRGBA                      bubble_color2;
  GdkRGBA                      search_shadow_rgba;
  GdkRGBA                      snippet_area_background_rgba;

  guint                        font_scale;

  gint                         overscroll_num_lines;

  guint                        delay_size_allocate_chainup;
  GtkAllocation                delay_size_allocation;

  IdeSourceLocation           *definition_src_location;
  GtkTextMark                 *definition_highlight_start_mark;
  GtkTextMark                 *definition_highlight_end_mark;

  GRegex                      *include_regex;

  IdeCursor                   *cursor;

  guint                        auto_indent : 1;
  guint                        completion_blocked : 1;
  guint                        completion_visible : 1;
  guint                        enable_word_completion : 1;
  guint                        highlight_current_line : 1;
  guint                        in_key_press : 1;
  guint                        in_replay_macro : 1;
  guint                        insert_mark_cleared : 1;
  guint                        insert_matching_brace : 1;
  guint                        overwrite_braces : 1;
  guint                        recording_macro : 1;
  guint                        rubberband_search : 1;
  guint                        scrolling_to_scroll_mark : 1;
  guint                        show_grid_lines : 1;
  guint                        show_line_changes : 1;
  guint                        show_line_diagnostics : 1;
  guint                        show_search_bubbles : 1;
  guint                        show_search_shadow : 1;
  guint                        snippet_completion : 1;
  guint                        waiting_for_capture : 1;
} IdeSourceViewPrivate;

typedef struct
{
  gint              ref_count;
  gint              count;
  IdeSourceView    *self;
  guint             is_forward : 1;
  guint             extend_selection : 1;
  guint             select_match : 1;
  guint             exclusive : 1;
} SearchMovement;

typedef struct
{
  IdeSourceView    *self;
  GtkTextMark      *word_start_mark;
  GtkTextMark      *word_end_mark;
} DefinitionHighlightData;

G_DEFINE_TYPE_WITH_PRIVATE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)
DZL_DEFINE_COUNTER (instances, "IdeSourceView", "Instances", "Number of IdeSourceView instances")

enum {
  PROP_0,
  PROP_BACK_FORWARD_LIST,
  PROP_COUNT,
  PROP_ENABLE_WORD_COMPLETION,
  PROP_FILE_SETTINGS,
  PROP_FONT_NAME,
  PROP_FONT_DESC,
  PROP_INDENTER,
  PROP_INDENT_STYLE,
  PROP_INSERT_MATCHING_BRACE,
  PROP_MODE_DISPLAY_NAME,
  PROP_OVERWRITE_BRACES,
  PROP_RUBBERBAND_SEARCH,
  PROP_SCROLL_OFFSET,
  PROP_SEARCH_CONTEXT,
  PROP_SEARCH_DIRECTION,
  PROP_SHOW_GRID_LINES,
  PROP_SHOW_LINE_CHANGES,
  PROP_SHOW_LINE_DIAGNOSTICS,
  PROP_SHOW_SEARCH_BUBBLES,
  PROP_SHOW_SEARCH_SHADOW,
  PROP_SNIPPET_COMPLETION,
  PROP_OVERSCROLL,
  LAST_PROP,

  /* These are overridden */
  PROP_AUTO_INDENT,
  PROP_HIGHLIGHT_CURRENT_LINE,
  PROP_OVERWRITE
};

enum {
  ACTION,
  ADD_CURSOR,
  APPEND_TO_COUNT,
  AUTO_INDENT,
  BEGIN_MACRO,
  BEGIN_RENAME,
  BEGIN_USER_ACTION,
  CAPTURE_MODIFIER,
  CLEAR_COUNT,
  CLEAR_MODIFIER,
  CLEAR_SEARCH,
  CLEAR_SELECTION,
  CLEAR_SNIPPETS,
  CYCLE_COMPLETION,
  DOCUMENTATION_REQUESTED,
  DECREASE_FONT_SIZE,
  DELETE_SELECTION,
  DUPLICATE_ENTIRE_LINE,
  END_MACRO,
  END_USER_ACTION,
  FOCUS_LOCATION,
  FORMAT_SELECTION,
  FIND_REFERENCES,
  GOTO_DEFINITION,
  HIDE_COMPLETION,
  INCREASE_FONT_SIZE,
  INDENT_SELECTION,
  INSERT_AT_CURSOR_AND_INDENT,
  INSERT_MODIFIER,
  JUMP,
  MOVEMENT,
  MOVE_ERROR,
  MOVE_SEARCH,
  PASTE_CLIPBOARD_EXTENDED,
  POP_SELECTION,
  POP_SNIPPET,
  PUSH_SELECTION,
  PUSH_SNIPPET,
  REBUILD_HIGHLIGHT,
  REINDENT,
  REMOVE_CURSORS,
  REPLAY_MACRO,
  REQUEST_DOCUMENTATION,
  RESET,
  RESET_FONT_SIZE,
  RESTORE_INSERT_MARK,
  SAVE_COMMAND,
  SAVE_INSERT_MARK,
  SAVE_SEARCH_CHAR,
  SELECT_INNER,
  SELECT_TAG,
  SELECTION_THEATRIC,
  SET_MODE,
  SET_OVERWRITE,
  SET_SEARCH_TEXT,
  SORT,
  SWAP_SELECTION_BOUNDS,
  LAST_SIGNAL
};

enum {
  TARGET_URI_LIST = 100
};

enum {
  FONT_SCALE_XX_SMALL,
  FONT_SCALE_X_SMALL,
  FONT_SCALE_SMALL,
  FONT_SCALE_NORMAL,
  FONT_SCALE_LARGE,
  FONT_SCALE_X_LARGE,
  FONT_SCALE_XX_LARGE,
  FONT_SCALE_XXX_LARGE,
  LAST_FONT_SCALE
};

static GParamSpec *properties [LAST_PROP];
static guint       signals [LAST_SIGNAL];
static gdouble     fontScale [LAST_FONT_SCALE] = {
  0.57870, 0.69444, 0.83333,
  1.0,
  1.2, 1.44, 1.728, 2.48832,
};

static void ide_source_view_real_save_insert_mark    (IdeSourceView         *self);
static void ide_source_view_real_restore_insert_mark (IdeSourceView         *self);
static void ide_source_view_real_set_mode            (IdeSourceView         *self,
                                                      const gchar           *name,
                                                      IdeSourceViewModeType  type);
static void ide_source_view_save_column              (IdeSourceView         *self);
static void ide_source_view_maybe_overwrite          (IdeSourceView         *self,
                                                      GtkTextIter           *iter,
                                                      const gchar           *text,
                                                      gint                   len);

static SearchMovement *
search_movement_ref (SearchMovement *movement)
{
  g_return_val_if_fail (movement, NULL);
  g_return_val_if_fail (movement->ref_count > 0, NULL);

  movement->ref_count++;

  return movement;
}

static void
search_movement_unref (SearchMovement *movement)
{
  g_return_if_fail (movement);
  g_return_if_fail (movement->ref_count > 0);

  if (--movement->ref_count == 0)
    {
      g_object_unref (movement->self);
      g_free (movement);
    }
}

static void
definition_highlight_data_free (DefinitionHighlightData *data)
{
  if (data != NULL)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_mark_get_buffer (data->word_start_mark);

      gtk_text_buffer_delete_mark (buffer, data->word_start_mark);
      gtk_text_buffer_delete_mark (buffer, data->word_end_mark);

      g_clear_object (&data->self);
      g_clear_object (&data->word_start_mark);
      g_clear_object (&data->word_end_mark);

      g_slice_free (DefinitionHighlightData, data);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SearchMovement, search_movement_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (DefinitionHighlightData, definition_highlight_data_free)

static SearchMovement *
search_movement_new (IdeSourceView *self,
                     gboolean       is_forward,
                     gboolean       extend_selection,
                     gboolean       select_match,
                     gboolean       exclusive,
                     gboolean       use_count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  SearchMovement *mv;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  mv = g_new0 (SearchMovement, 1);
  mv->ref_count = 1;
  mv->self = g_object_ref (self);
  mv->is_forward = !!is_forward;
  mv->extend_selection = !!extend_selection;
  mv->select_match = !!select_match;
  mv->exclusive = !!exclusive;
  mv->count = use_count ? MAX (priv->count, 1) : 1;

  g_assert (mv->ref_count == 1);
  g_assert (mv->count > 0);

  return mv;
}

static gboolean
ide_source_view_can_animate (IdeSourceView *self)
{
  GtkSettings *settings;
  GdkScreen *screen;
  gboolean can_animate = FALSE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  screen = gtk_widget_get_screen (GTK_WIDGET (self));
  settings = gtk_settings_get_for_screen (screen);

  g_object_get (settings, "gtk-enable-animations", &can_animate, NULL);

  return can_animate;
}

static void
ide_source_view_sync_rubberband_mark (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * Occasionally, we need to sync the rubberband mark with the insert mark so
   * that forward searching does not jump around in the buffer. Good times to
   * do so are when focus leaves the buffer, or when the ::set_search_text()
   * vfunc is activated.
   */

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_buffer_move_mark (buffer, priv->rubberband_mark, &iter);
  gtk_text_buffer_move_mark (buffer, priv->rubberband_insert_mark, &iter);
}

void
_ide_source_view_set_count (IdeSourceView *self,
                            gint           count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  priv->count = count;
}

void
_ide_source_view_set_modifier (IdeSourceView *self,
                               gunichar       modifier)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  priv->modifier = modifier;

  if (priv->recording_macro && !priv->in_replay_macro)
    ide_source_view_capture_record_modifier (priv->capture, modifier);
}

static IdeIndenter *
ide_source_view_get_indenter (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->indenter_adapter != NULL)
    return ide_extension_adapter_get_extension (priv->indenter_adapter);

  return NULL;
}

static void
ide_source_view_block_handlers (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  dzl_signal_group_block (priv->buffer_signals);
}

static void
ide_source_view_unblock_handlers (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  dzl_signal_group_unblock (priv->buffer_signals);
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
  g_assert (iter1 != NULL);
  g_assert (iter2 != NULL);
  g_assert (rect != NULL);
  g_assert (gtk_text_iter_get_buffer (iter1) == gtk_text_iter_get_buffer (iter2));
  g_assert (gtk_text_view_get_buffer (text_view) == gtk_text_iter_get_buffer (iter1));

  begin = *iter1;
  end = *iter2;

  if (gtk_text_iter_equal (&begin, &end))
    {
      gtk_text_view_get_iter_location (text_view, &begin, &area);
      goto finish;
    }

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
  DzlBoxTheatric *theatric;
  GtkAllocation alloc;
  GdkRectangle rect = { 0 };

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);

  get_rect_for_iters (GTK_TEXT_VIEW (self), begin, end, &rect, GTK_TEXT_WINDOW_WIDGET);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);
  rect.height = MIN (rect.height, alloc.height - rect.y);

  theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", rect.height,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  dzl_object_animate_full (theatric,
                           DZL_ANIMATION_EASE_IN_CUBIC,
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
  DzlBoxTheatric *theatric;
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

  theatric = g_object_new (DZL_TYPE_BOX_THEATRIC,
                           "alpha", 0.3,
                           "background", "#729fcf",
                           "height", rect.height,
                           "target", self,
                           "width", rect.width,
                           "x", rect.x,
                           "y", rect.y,
                           NULL);

  if (is_whole_line)
    dzl_object_animate_full (theatric,
                             DZL_ANIMATION_EASE_OUT_QUAD,
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
    dzl_object_animate_full (theatric,
                             DZL_ANIMATION_EASE_OUT_QUAD,
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
    dzl_object_animate_full (theatric,
                             DZL_ANIMATION_EASE_OUT_QUAD,
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

void
ide_source_view_scroll_to_insert (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  _ide_buffer_cancel_cursor_restore (IDE_BUFFER (buffer));
  mark = gtk_text_buffer_get_insert (buffer);
  ide_source_view_scroll_mark_onscreen (self, mark, TRUE, 0.5, 0.5);

  IDE_EXIT;
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
ide_source_view_reload_word_completion (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeContext *context;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((priv->buffer != NULL) && (context = ide_buffer_get_context (priv->buffer)))
    {
      IdeBufferManager *bufmgr;
      GtkSourceCompletion *completion;
      IdeWordCompletionProvider *words;
      GList *list;

      bufmgr = ide_context_get_buffer_manager (context);
      words = ide_buffer_manager_get_word_completion (bufmgr);
      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
      list = gtk_source_completion_get_providers (completion);

      if (priv->enable_word_completion && !g_list_find (list, words))
        gtk_source_completion_add_provider (completion,
                                            GTK_SOURCE_COMPLETION_PROVIDER (words),
                                            NULL);
      else if (!priv->enable_word_completion && g_list_find (list, words))
        gtk_source_completion_remove_provider (completion,
                                               GTK_SOURCE_COMPLETION_PROVIDER (words),
                                               NULL);
    }
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

  if (priv->snippets_provider != NULL)
    g_object_set (priv->snippets_provider, "snippets", snippets, NULL);
}

static void
ide_source_view_update_auto_indent_override (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeIndenter *indenter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  indenter = ide_source_view_get_indenter (self);

  /*
   * Updates our override of auto-indent from the GtkSourceView underneath us.
   * Since we do our own mimicing of GtkSourceView, we always disable it. Also
   * updates our mode which needs to know if we have an indenter to provide
   * different CSS selectors.
   */
  gtk_source_view_set_auto_indent (GTK_SOURCE_VIEW (self), FALSE);
  if (priv->mode != NULL)
    ide_source_view_mode_set_has_indenter (priv->mode, !!indenter);
}

static void
ide_source_view_set_file_settings (IdeSourceView   *self,
                                   IdeFileSettings *file_settings)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_FILE_SETTINGS (file_settings));

  if (file_settings != ide_source_view_get_file_settings (self))
    {
      dzl_binding_group_set_source (priv->file_setting_bindings, file_settings);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE_SETTINGS]);
    }
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

  g_assert (IDE_IS_FILE (file));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  file_settings = ide_file_load_settings_finish (file, result, &error);

  if (!file_settings)
    {
      g_message ("%s", error->message);
      return;
    }

  ide_source_view_set_file_settings (self, file_settings);
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
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  IdeFile *file = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * Update source language, etc.
   */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  file = ide_buffer_get_file (IDE_BUFFER (buffer));
  language = ide_file_get_language (file);

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_FILE (file));
  g_assert (!language || GTK_SOURCE_IS_LANGUAGE (language));

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), language);
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
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceLanguage *language;
  const gchar *lang_id = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if ((language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer))))
    lang_id = gtk_source_language_get_id (language);

  /*
   * Update the indenter, which is provided by a plugin.
   */
  if (priv->indenter_adapter != NULL)
    ide_extension_adapter_set_value (priv->indenter_adapter, lang_id);
  ide_source_view_update_auto_indent_override (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INDENTER]);

  /*
   * Update the completion providers, which are provided by plugins.
   */
  if (priv->completion_providers != NULL)
    ide_extension_set_adapter_set_value (priv->completion_providers, lang_id);

  /*
   * Make sure the snippet engine reloads for the new language.
   */
  ide_source_view_reload_snippets (self);
}

static void
ide_source_view__buffer_notify_style_scheme_cb (IdeSourceView *self,
                                                GParamSpec    *pspec,
                                                IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceStyleScheme *scheme = NULL;
  GtkSourceStyle *search_match_style = NULL;
  GtkSourceStyle *search_shadow_style = NULL;
  GtkSourceStyle *snippet_area_style = NULL;
  g_autofree gchar *snippet_background = NULL;
  g_autofree gchar *search_shadow_background = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));
  if (scheme)
    {
      search_match_style = gtk_source_style_scheme_get_style (scheme, "search-match");
      search_shadow_style = gtk_source_style_scheme_get_style (scheme, "search-shadow");
      snippet_area_style = gtk_source_style_scheme_get_style (scheme, "snippet::area");
    }

  if (search_match_style)
    {
      g_autofree gchar *background = NULL;
      GdkRGBA color;

      g_object_get (search_match_style, "background", &background, NULL);
      gdk_rgba_parse (&color, background);
      dzl_rgba_shade (&color, &priv->bubble_color1, 0.8);
      dzl_rgba_shade (&color, &priv->bubble_color2, 1.1);
    }
  else
    {
      gdk_rgba_parse (&priv->bubble_color1, "#edd400");
      gdk_rgba_parse (&priv->bubble_color2, "#fce94f");
    }

  if (search_shadow_style)
    g_object_get (search_shadow_style, "background", &search_shadow_background, NULL);

  if (search_shadow_background)
    gdk_rgba_parse (&priv->search_shadow_rgba, search_shadow_background);
  else
    {
      gdk_rgba_parse (&priv->search_shadow_rgba, "#000000");
      priv->search_shadow_rgba.alpha = 0.2;
    }

  if (snippet_area_style)
    g_object_get (snippet_area_style, "background", &snippet_background, NULL);

  if (snippet_background)
    gdk_rgba_parse (&priv->snippet_area_background_rgba, snippet_background);
  else
    {
      gdk_rgba_parse (&priv->snippet_area_background_rgba, "#204a87");
      priv->snippet_area_background_rgba.alpha = 0.1;
    }
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
ide_source_view__search_settings_notify_search_text (IdeSourceView           *self,
                                                     GParamSpec              *pspec,
                                                     GtkSourceSearchSettings *search_settings)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  const gchar *search_text;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  search_text = gtk_source_search_settings_get_search_text (search_settings);

  /*
   * If we have IdeSourceView:rubberband-search enabled, then we should try to
   * autoscroll to the next search result starting from our saved search mark.
   */
  if ((search_text != NULL) && (search_text [0] != '\0') &&
      priv->rubberband_search && (priv->rubberband_insert_mark != NULL))
    {
      GtkTextBuffer *buffer;
      GtkTextIter begin_iter;
      GtkTextIter match_begin;
      GtkTextIter match_end;
      gboolean search_succeeded;
      gboolean has_wrapped;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
      gtk_text_buffer_get_iter_at_mark (buffer, &begin_iter, priv->rubberband_insert_mark);

      switch (priv->search_direction)
        {
        case GTK_DIR_LEFT:
        case GTK_DIR_UP:
          search_succeeded = gtk_source_search_context_backward2 (priv->search_context,
                                                                  &begin_iter,
                                                                  &match_begin,
                                                                  &match_end,
                                                                  &has_wrapped);
          break;
        case GTK_DIR_RIGHT:
        case GTK_DIR_DOWN:
          search_succeeded = gtk_source_search_context_forward2 (priv->search_context,
                                                                 &begin_iter,
                                                                 &match_begin,
                                                                 &match_end,
                                                                 &has_wrapped);
          break;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_TAB_BACKWARD:
        default:
          g_return_if_reached ();
        }

      if (search_succeeded)
        {
          gtk_text_buffer_move_mark (buffer, priv->rubberband_mark, &match_begin);
          ide_source_view_scroll_mark_onscreen (self, priv->rubberband_mark, TRUE, 0.5, 0.5);
        }
    }
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
      const PangoFontDescription *font_desc = priv->font_desc;
      PangoFontDescription *copy = NULL;

      if (priv->font_scale != FONT_SCALE_NORMAL)
        {
          gdouble font_scale;
          guint font_size;

          font_scale = fontScale [priv->font_scale];

          copy = pango_font_description_copy (priv->font_desc);
          font_size = pango_font_description_get_size (priv->font_desc);
          pango_font_description_set_size (copy, font_size * font_scale);

          font_desc = copy;
        }

      str = dzl_pango_font_description_to_css (font_desc);
      css = g_strdup_printf ("textview { %s }", str ?: "");
      gtk_css_provider_load_from_data (priv->css_provider, css, -1, NULL);

      g_clear_pointer (&copy, pango_font_description_free);
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
ide_source_view__buffer_insert_text_cb (IdeSourceView *self,
                                        GtkTextIter   *iter,
                                        gchar         *text,
                                        gint           len,
                                        GtkTextBuffer *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (iter != NULL);
  g_assert (text != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (_ide_buffer_get_loading (IDE_BUFFER (buffer)))
    return;

  gtk_text_buffer_begin_user_action (buffer);

  if (NULL != (snippet = g_queue_peek_head (priv->snippets)))
    {
      ide_source_view_block_handlers (self);
      ide_source_snippet_before_insert_text (snippet, buffer, iter, text, len);
      ide_source_view_unblock_handlers (self);
    }
}

static void
ide_source_view__buffer_insert_text_after_cb (IdeSourceView *self,
                                              GtkTextIter   *iter,
                                              gchar         *text,
                                              gint           len,
                                              GtkTextBuffer *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;
  GtkTextIter insert;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (iter != NULL);
  g_assert (text != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (_ide_buffer_get_loading (IDE_BUFFER (buffer)))
    return;

  if (NULL != (snippet = g_queue_peek_head (priv->snippets)))
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

  if (priv->in_key_press)
    {
      /*
       * If we are handling the key-press-event, we might have just inserted
       * a character that indicates we should overwrite the next character.
       * However, due to GtkIMContext constraints, we need to allow it to be
       * inserted and then handle it here.
       */
      ide_source_view_maybe_overwrite (self, iter, text, len);
    }

  gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert(buffer));
  if (gtk_text_iter_equal (iter, &insert))
    {
      ide_source_view_block_handlers (self);
      ide_cursor_insert_text (priv->cursor, text, len);
      ide_source_view_unblock_handlers (self);
      gtk_text_buffer_get_iter_at_mark (buffer, iter, gtk_text_buffer_get_insert (buffer));
    }

  gtk_text_buffer_end_user_action (buffer);

  return;
}

static void
ide_source_view__buffer_delete_range_cb (IdeSourceView *self,
                                         GtkTextIter   *begin,
                                         GtkTextIter   *end,
                                         GtkTextBuffer *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  if (NULL != (snippet = g_queue_peek_head (priv->snippets)))
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

  IDE_EXIT;
}

static void
ide_source_view__buffer_delete_range_after_cb (IdeSourceView *self,
                                               GtkTextIter   *begin,
                                               GtkTextIter   *end,
                                               GtkTextBuffer *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  ide_source_view_block_handlers (self);

  if (NULL != (snippet = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_after_delete_range (snippet, buffer, begin, end);

  ide_source_view_unblock_handlers (self);

  IDE_EXIT;
}

static void
ide_source_view__buffer_mark_set_cb (IdeSourceView *self,
                                     GtkTextIter   *iter,
                                     GtkTextMark   *mark,
                                     GtkTextBuffer *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippet *snippet;
  GtkTextMark *insert;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (iter != NULL);
  g_assert (GTK_IS_TEXT_MARK (mark));
  g_assert (GTK_IS_TEXT_BUFFER (buffer));

  insert = gtk_text_buffer_get_insert (buffer);

  if (mark == insert)
    {
      ide_source_view_block_handlers (self);
      while (NULL != (snippet = g_queue_peek_head (priv->snippets)) &&
             !ide_source_snippet_insert_set (snippet, mark))
        ide_source_view_pop_snippet (self);
      ide_source_view_unblock_handlers (self);
    }

#ifdef IDE_ENABLE_TRACE
  if (mark == insert || mark == gtk_text_buffer_get_selection_bound (buffer))
      {
        GtkTextIter begin;
        GtkTextIter end;

        if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
          {
            gtk_text_iter_order (&begin, &end);
            IDE_TRACE_MSG ("Selection is now %d:%d to %d:%d",
                           gtk_text_iter_get_line (&begin),
                           gtk_text_iter_get_line_offset (&begin),
                           gtk_text_iter_get_line (&end),
                           gtk_text_iter_get_line_offset (&end));
          }
      }
#endif
}

static void
ide_source_view__buffer_notify_has_selection_cb (IdeSourceView *self,
                                                 GParamSpec    *pspec,
                                                 IdeBuffer     *buffer)
{
  IdeWorkbench *workbench = ide_widget_get_workbench (GTK_WIDGET (self));

  if (workbench == NULL)
    return;

  if (gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
    ide_workbench_set_selection_owner (workbench, G_OBJECT (self));
  else if (ide_workbench_get_selection_owner (workbench) == G_OBJECT (self))
    ide_workbench_set_selection_owner (workbench, NULL);
}

static void
ide_source_view__buffer_notify_highlight_diagnostics_cb (IdeSourceView *self,
                                                         GParamSpec    *pspec,
                                                         IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (priv->line_diagnostics_renderer != NULL)
    {
      gboolean visible;

      visible = (priv->show_line_diagnostics && ide_buffer_get_highlight_diagnostics (buffer));
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

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  gtk_source_gutter_renderer_queue_draw (priv->line_change_renderer);
  gtk_source_gutter_renderer_queue_draw (priv->line_diagnostics_renderer);

  IDE_EXIT;
}

static void
ide_source_view__buffer_loaded_cb (IdeSourceView *self,
                                   IdeBuffer     *buffer)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkAdjustment *adj;
  GtkTextMark *insert;
  GtkTextIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (priv->completion_blocked)
    {
      GtkSourceCompletion *completion;

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
      gtk_source_completion_unblock_interactive (completion);
      priv->completion_blocked = FALSE;
    }

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));

  /* Store the line column (visual offset) so movements are correct. */
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, insert);
  priv->target_line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self),
                                                                &iter);

  /* Only scroll if the user hasn't started an intermediate scroll */
  adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));
  if (gtk_adjustment_get_value (adj) == gtk_adjustment_get_lower (adj))
    ide_source_view_scroll_to_mark (self, insert, 0.0, TRUE, 0.5, 0.5, TRUE);

  IDE_EXIT;
}

static void
ide_source_view__completion_provider_added (IdeExtensionSetAdapter *adapter,
                                            PeasPluginInfo         *plugin_info,
                                            PeasExtension          *extension,
                                            IdeSourceView          *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceCompletion *completion;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (extension));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));

  gtk_source_completion_add_provider (completion,
                                      GTK_SOURCE_COMPLETION_PROVIDER (extension),
                                      NULL);

  ide_completion_provider_load (IDE_COMPLETION_PROVIDER (extension),
                                ide_buffer_get_context (priv->buffer));
}

static void
ide_source_view__completion_provider_removed (IdeExtensionSetAdapter *adapter,
                                              PeasPluginInfo         *plugin_info,
                                              PeasExtension          *extension,
                                              IdeSourceView          *self)
{
  GtkSourceCompletion *completion;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (plugin_info != NULL);
  g_assert (IDE_IS_COMPLETION_PROVIDER (extension));
  g_assert (IDE_IS_EXTENSION_SET_ADAPTER (adapter));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));

  gtk_source_completion_remove_provider (completion,
                                         GTK_SOURCE_COMPLETION_PROVIDER (extension),
                                         NULL);
}

static void
ide_source_view_set_cursor_from_name (IdeSourceView *self,
                                      const gchar   *cursor_name)
{
  GdkDisplay *display;
  GdkCursor *cursor;
  GdkWindow *window = gtk_text_view_get_window (GTK_TEXT_VIEW (self),
                                                GTK_TEXT_WINDOW_TEXT);

  if (!window)
    return;

  display = gdk_window_get_display (window);
  cursor = gdk_cursor_new_from_name (display, cursor_name);

  gdk_window_set_cursor (window, cursor);
}

static void
ide_source_view_reset_definition_highlight (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->definition_src_location)
    g_clear_pointer (&priv->definition_src_location, ide_source_location_unref);

  if (priv->buffer != NULL)
    {
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (priv->buffer), &begin, &end);
      gtk_text_buffer_remove_tag_by_name (GTK_TEXT_BUFFER (priv->buffer), TAG_DEFINITION, &begin, &end);
    }

  ide_source_view_set_cursor_from_name (self, "text");
}

static void
ide_source_view__buffer__notify_can_redo (IdeSourceView *self,
                                          GParamSpec    *pspec,
                                          IdeBuffer     *buffer)
{
  GActionGroup *group;
  gboolean can_redo;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_object_get (buffer,
                "can-redo", &can_redo,
                NULL);

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "sourceview");
  dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "redo", can_redo);
}

static void
ide_source_view__buffer__notify_can_undo (IdeSourceView *self,
                                          GParamSpec    *pspec,
                                          IdeBuffer     *buffer)
{
  GActionGroup *group;
  gboolean can_undo;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  g_object_get (buffer,
                "can-undo", &can_undo,
                NULL);

  group = gtk_widget_get_action_group (GTK_WIDGET (self), "sourceview");
  dzl_widget_action_group_set_action_enabled (DZL_WIDGET_ACTION_GROUP (group), "undo", can_undo);
}

static void
ide_source_view_bind_buffer (IdeSourceView  *self,
                             IdeBuffer      *buffer,
                             DzlSignalGroup *group)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceSearchSettings *search_settings;
  GtkTextMark *insert;
  GtkTextIter iter;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  priv->buffer = buffer;

  ide_source_view_reset_definition_highlight (self);

  ide_buffer_hold (buffer);

  if (_ide_buffer_get_loading (buffer))
    {
      GtkSourceCompletion *completion;

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
      gtk_source_completion_block_interactive (completion);
      priv->completion_blocked = TRUE;
    }

  context = ide_buffer_get_context (buffer);

  priv->indenter_adapter = ide_extension_adapter_new (context,
                                                      peas_engine_get_default (),
                                                      IDE_TYPE_INDENTER,
                                                      "Indenter-Languages",
                                                      NULL);

  priv->completion_providers = ide_extension_set_adapter_new (context,
                                                              peas_engine_get_default (),
                                                              IDE_TYPE_COMPLETION_PROVIDER,
                                                              "Completion-Provider-Languages",
                                                              NULL);

  dzl_signal_group_set_target (priv->completion_providers_signals,
                               priv->completion_providers);

  ide_extension_set_adapter_foreach (priv->completion_providers,
                                     (IdeExtensionSetAdapterForeachFunc)ide_source_view__completion_provider_added,
                                     self);

  search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                  "wrap-around", TRUE,
                                  "regex-enabled", FALSE,
                                  "case-sensitive", FALSE,
                                  NULL);
  priv->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                       "buffer", buffer,
                                       "highlight", TRUE,
                                       "settings", search_settings,
                                       NULL);

  g_signal_connect_object (search_settings,
                           "notify::search-text",
                           G_CALLBACK (ide_source_view__search_settings_notify_search_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_clear_object (&search_settings);

  priv->cursor = g_object_new (IDE_TYPE_CURSOR,
                               "ide-source-view", self,
                               NULL);

  /* Create scroll mark used by movements and our scrolling helper */
  gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
  priv->scroll_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);

  /* Create rubberband mark used by search rubberbanding */
  priv->rubberband_mark =
    gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);
  priv->rubberband_insert_mark =
    gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);

  /* Marks used for definition highlights */
  priv->definition_highlight_start_mark =
    gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);
  priv->definition_highlight_end_mark =
    gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);

  g_object_ref (priv->definition_highlight_start_mark);
  g_object_ref (priv->definition_highlight_end_mark);

  ide_source_view__buffer_notify_language_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_file_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_highlight_diagnostics_cb (self, NULL, buffer);
  ide_source_view__buffer_notify_style_scheme_cb (self, NULL, buffer);
  ide_source_view__buffer__notify_can_redo (self, NULL, buffer);
  ide_source_view__buffer__notify_can_undo (self, NULL, buffer);
  ide_source_view_reload_word_completion (self);
  ide_source_view_real_set_mode (self, NULL, IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  ide_source_view_scroll_mark_onscreen (self, insert, TRUE, 0.5, 0.5);

  IDE_EXIT;
}

static void
ide_source_view_unbind_buffer (IdeSourceView  *self,
                               DzlSignalGroup *group)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (DZL_IS_SIGNAL_GROUP (group));

  if (priv->buffer == NULL)
    IDE_EXIT;

  priv->scroll_mark = NULL;

  if (priv->completion_blocked)
    {
      GtkSourceCompletion *completion;

      completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
      gtk_source_completion_unblock_interactive (completion);
      priv->completion_blocked = FALSE;
    }

  ide_extension_set_adapter_foreach (priv->completion_providers,
                                     (IdeExtensionSetAdapterForeachFunc)ide_source_view__completion_provider_removed,
                                     self);

  dzl_signal_group_set_target (priv->completion_providers_signals, NULL);

  if (priv->cursor != NULL)
    {
      g_object_run_dispose (G_OBJECT (priv->cursor));
      g_clear_object (&priv->cursor);
    }

  g_clear_object (&priv->search_context);
  g_clear_object (&priv->indenter_adapter);
  g_clear_object (&priv->completion_providers);
  g_clear_object (&priv->definition_highlight_start_mark);
  g_clear_object (&priv->definition_highlight_end_mark);

  ide_buffer_release (priv->buffer);

  IDE_EXIT;
}

static void
ide_source_view_maybe_overwrite (IdeSourceView *self,
                                 GtkTextIter   *iter,
                                 const gchar   *text,
                                 gint           len)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextIter insert;
  GtkTextIter next;
  gunichar ch;
  gunichar next_ch;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (iter != NULL);
  g_assert (text != NULL);
  g_assert (len > 0);

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

  /*
   * Ignore this if it wasn't a single character insertion.
   */
  if (len != 1)
    return;

  /*
   * Short circuit if there is already a selection.
   */
  buffer = gtk_text_iter_get_buffer (iter);
  if (gtk_text_buffer_get_has_selection (buffer))
      return;

  /*
   * @iter is pointing at the location we just inserted text. Since we
   * know we only inserted one character, lets move past it and compare
   * to see if we want to overwrite.
   */
  gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert (buffer));
  ch = g_utf8_get_char (text);
  next_ch = gtk_text_iter_get_char (&insert);

  switch (ch)
    {
    case ')': case ']': case '}': case '"': case '\'': case ';':
      if (ch == next_ch)
        break;
      /* fall through */
    default:
      return;
    }

  next = insert;

  gtk_text_iter_forward_char (&next);
  gtk_text_buffer_delete (buffer, &insert, &next);
  *iter = insert;
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
  gchar ch[2] = { 0 };

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
      ch[0] = '}';
      break;

    case GDK_KEY_parenleft:
      ch[0] = ')';
      break;

    case GDK_KEY_bracketleft:
      ch[0] = ']';
      break;

    case GDK_KEY_quotedbl:
      ch[0] = '"';
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
      if (ch[0] == '"')
        {
          guint count;

          count = count_chars_on_line (self, '"', &iter);
          if ((count > 1) && ((count % 2) == 0))
            return FALSE;
        }

      gtk_text_buffer_insert_at_cursor (buffer, ch, 1);
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
  g_assert (event->keyval == GDK_KEY_BackSpace);

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
    case '<':  match = '>';  break;
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
                           GdkEventKey   *event,
                           IdeIndenter   *indenter)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkWidget *widget = (GtkWidget *)self;
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  g_autofree gchar *indent = NULL;
  GtkTextIter begin;
  GtkTextIter end;
  gint cursor_offset = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (priv->auto_indent == TRUE);
  g_assert (event);
  g_assert (!indenter || IDE_IS_INDENTER (indenter));

  buffer = gtk_text_view_get_buffer (text_view);

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
   * Let the formatter potentially set the replacement text. If we don't have a
   * formatter, use our simple formatter which tries to mimic GtkSourceView.
   */
  indent = ide_indenter_format (indenter, text_view, &begin, &end, &cursor_offset, event);

  if (indent != NULL)
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
      ide_source_view_scroll_mark_onscreen (self, insert, FALSE, 0, 0);

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

static inline gboolean
compare_keys (GdkKeymap       *keymap,
              GdkEventKey     *event,
              GtkBindingEntry *binding_entry,
              guint           *new_keyval,
              GdkModifierType *state_consumed)
{
  gdk_keymap_translate_keyboard_state (keymap,
                                       event->hardware_keycode, event->state, event->group,
                                       new_keyval, NULL, NULL, state_consumed);

  if (g_ascii_isupper (*new_keyval))
    {
      *new_keyval = gdk_keyval_to_lower (*new_keyval);
      *state_consumed &= ~GDK_SHIFT_MASK;
    }

  return (*new_keyval == binding_entry->keyval &&
          (event->state & ~(*state_consumed) & ALL_ACCELS_MASK) == (binding_entry->modifiers & ALL_ACCELS_MASK));
}

static gboolean
is_key_vim_binded (GtkWidget       *widget,
                   GdkEventKey     *event,
                   guint           *new_keyval,
                   GdkModifierType *state_consumed)
{
  GdkKeymap *keymap;
  GtkBindingSet *binding_set;
  GtkBindingEntry *binding_entry;
  GtkStyleContext *context;
  GtkStateFlags state;
  GPtrArray *binding_set_array;


  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (IDE_SOURCE_VIEW (widget));

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->mode));
  keymap = gdk_keymap_get_default ();
  state = gtk_widget_get_state_flags (GTK_WIDGET (priv->mode));

  gtk_style_context_get (context, state, "gtk-key-bindings", &binding_set_array, NULL);
  if (binding_set_array)
    {
      for (guint i = 0; i < binding_set_array->len; i++)
        {
          binding_set = g_ptr_array_index (binding_set_array, i);
          if (g_str_has_prefix (binding_set->set_name, "builder-vim"))
            {
              binding_entry = binding_set->entries;
              while (binding_entry)
                {
                  if (compare_keys (keymap, event, binding_entry, new_keyval, state_consumed))
                    {
                      g_ptr_array_unref (binding_set_array);
                      return TRUE;
                    }

                  binding_entry = binding_entry->set_next;
                }
            }
        }

      g_ptr_array_unref (binding_set_array);
    }

  return FALSE;
}

static void
command_string_append_to (GString         *command_str,
                          guint            keyval,
                          GdkModifierType  state)
{
  if (state & GDK_CONTROL_MASK)
    g_string_append (command_str, "<ctrl>");

  if (state & GDK_SHIFT_MASK)
    g_string_append (command_str, "<shift>");

  if (state & GDK_MOD1_MASK)
    g_string_append (command_str, "<alt>");

  if ((keyval >= '!' && keyval <= '~' ) && keyval != GDK_KEY_bracketleft && keyval != GDK_KEY_bracketright)
    g_string_append_c (command_str, keyval);
  else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
    g_string_append_c (command_str, keyval - GDK_KEY_KP_0 + '0');
  else
    {
      g_string_append_c (command_str, '[');
      g_string_append (command_str, gdk_keyval_name (keyval));
      g_string_append_c (command_str, ']');
    }
}

static gboolean
ide_source_view_do_mode (IdeSourceView *self,
                         GdkEventKey   *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autofree gchar *suggested_default = NULL;
  guint new_keyval;
  GdkModifierType state;
  GdkModifierType state_consumed;
  gboolean ret = FALSE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->mode)
    {
      IdeSourceViewMode *mode;
      gboolean handled;
      gboolean remove = FALSE;

#ifdef IDE_ENABLE_TRACE
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

      if (is_key_vim_binded (GTK_WIDGET (self), event, &new_keyval, &state_consumed))
        {
          state = event->state & ~(state_consumed);
          command_string_append_to (priv->command_str, new_keyval, state);
        }

      /* lookup what this mode thinks our next default should be */
      suggested_default = g_strdup (ide_source_view_mode_get_default_mode (priv->mode));

      handled = _ide_source_view_mode_do_event (priv->mode, event, &remove);

      if (remove)
        {
          /* only remove mode if it is still active */
          if (priv->mode == mode)
            g_clear_object (&priv->mode);
        }

      g_object_unref (mode);

      if (handled)
        ret = TRUE;
    }

  if (priv->mode == NULL)
    ide_source_view_real_set_mode (self, suggested_default, IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);

  g_assert (priv->mode != NULL);

  if (ide_source_view_mode_get_mode_type (priv->mode) == IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT)
    g_string_erase (priv->command_str, 0, -1);

  if (ide_source_view_mode_get_keep_mark_on_char (priv->mode))
    {
      GtkTextBuffer *buffer;
      GtkTextMark *insert;
      GtkTextMark *selection;
      GtkTextIter insert_iter;
      GtkTextIter selection_iter;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
      insert = gtk_text_buffer_get_insert (buffer);
      selection = gtk_text_buffer_get_selection_bound (buffer);

      gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert);
      gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter, selection);

      if (gtk_text_iter_ends_line (&insert_iter) && !gtk_text_iter_starts_line (&insert_iter))
        {
          gtk_text_iter_backward_char (&insert_iter);
          if (gtk_text_buffer_get_has_selection (buffer))
            gtk_text_buffer_select_range (buffer, &insert_iter, &selection_iter);
          else
            gtk_text_buffer_select_range (buffer, &insert_iter, &insert_iter);
        }
    }

  gtk_text_view_reset_cursor_blink (GTK_TEXT_VIEW (self));

  return ret;
}

static gboolean
is_modifier_key (GdkEventKey *event)
{
  static const guint modifier_keyvals[] = {
    GDK_KEY_Shift_L, GDK_KEY_Shift_R, GDK_KEY_Shift_Lock,
    GDK_KEY_Caps_Lock, GDK_KEY_ISO_Lock, GDK_KEY_Control_L,
    GDK_KEY_Control_R, GDK_KEY_Meta_L, GDK_KEY_Meta_R,
    GDK_KEY_Alt_L, GDK_KEY_Alt_R, GDK_KEY_Super_L, GDK_KEY_Super_R,
    GDK_KEY_Hyper_L, GDK_KEY_Hyper_R, GDK_KEY_ISO_Level3_Shift,
    GDK_KEY_ISO_Next_Group, GDK_KEY_ISO_Prev_Group,
    GDK_KEY_ISO_First_Group, GDK_KEY_ISO_Last_Group,
    GDK_KEY_Mode_switch, GDK_KEY_Num_Lock, GDK_KEY_Multi_key,
    GDK_KEY_Scroll_Lock,
    0
  };
  const guint *ac_val;

  ac_val = modifier_keyvals;
  while (*ac_val)
    {
      if (event->keyval == *ac_val++)
        return TRUE;
    }

  return FALSE;
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
   * If we are waiting for input for a modifier key, dispatch it now.
   */
  if (priv->waiting_for_capture)
    {
      if (!is_modifier_key (event))
        {
          guint new_keyval;
          GdkModifierType state_consumed;
          GdkKeymap *keymap = gdk_keymap_get_default ();

          _ide_source_view_set_modifier (self, gdk_keyval_to_unicode (event->keyval));
          gdk_keymap_translate_keyboard_state (keymap,
                                               event->hardware_keycode, event->state, event->group,
                                               &new_keyval, NULL, NULL, &state_consumed);

          command_string_append_to (priv->command_str, new_keyval, event->state & ~(state_consumed));
        }

      return TRUE;
    }

  /*
   * Are we currently recording a macro? If so lets stash the event for later.
   */
  if (priv->recording_macro)
    ide_source_view_capture_record_event (priv->capture, (GdkEvent *)event,
                                          priv->count, priv->modifier);

  /*
   * Check our current change sequence. If the buffer has changed during the
   * key-press handler, we'll refocus our selves at the insert caret.
   */
  change_sequence = priv->change_sequence;

  priv->in_key_press = TRUE;

  /*
   * If we are in a non-default mode, dispatch the event to the mode. This allows custom
   * keybindings like Emacs and Vim to be implemented using gtk-bindings CSS.
   */
  if (ide_source_view_do_mode (self, event))
    {
      ret = TRUE;
      goto cleanup;
    }

  /*
   * Handle movement through the tab stops of the current snippet if needed.
   */
  if (NULL != (snippet = g_queue_peek_head (priv->snippets)))
    {
      switch ((gint) event->keyval)
        {
        case GDK_KEY_Escape:
          ide_source_view_block_handlers (self);
          ide_source_view_pop_snippet (self);
          ide_source_view_scroll_to_insert (self);
          ide_source_view_unblock_handlers (self);
          ret = TRUE;
          goto cleanup;

        case GDK_KEY_KP_Tab:
        case GDK_KEY_Tab:
          if ((event->state & GDK_SHIFT_MASK) == 0)
            {
              ide_source_view_block_handlers (self);
              if (!ide_source_snippet_move_next (snippet))
                ide_source_view_pop_snippet (self);
              ide_source_view_scroll_to_insert (self);
              ide_source_view_unblock_handlers (self);
              ret = TRUE;
              goto cleanup;
            }
          /* Fallthrough */
        case GDK_KEY_ISO_Left_Tab:
          ide_source_view_block_handlers (self);
          ide_source_snippet_move_previous (snippet);
          ide_source_view_scroll_to_insert (self);
          ide_source_view_unblock_handlers (self);
          ret = TRUE;
          goto cleanup;

        default:
          break;
        }
    }

  /*
   * We have stolen ownership of Tab from GtkSourceCompletion so that we can
   * move between snippets at a higher priority than the completion window.
   * If we don't have a snippet active
   */
  if (priv->completion_visible && event->keyval == GDK_KEY_Tab)
    {
      GtkSourceCompletion *completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
      g_signal_emit_by_name (completion, "activate-proposal");
      ret = TRUE;
      goto cleanup;
    }

  /*
   * Avoid conflicts with global <alt>+N perspective movements.
   * We might want to adjust those keybindings at somepoint.
   */
  if (priv->completion_visible && event->state == GDK_MOD1_MASK)
    {
      if ((event->keyval >= GDK_KEY_0 && event->keyval <= GDK_KEY_9) ||
          (event->keyval >= GDK_KEY_KP_0 && event->keyval <= GDK_KEY_KP_9))
        {
          ret = TRUE;
          goto cleanup;
        }
    }

  /*
   * If we are backspacing, and the next character is the matching brace,
   * then we might want to delete it too.
   */
  if ((event->keyval == GDK_KEY_BackSpace) && !gtk_text_buffer_get_has_selection (buffer))
    {
      if (ide_source_view_maybe_delete_match (self, event))
        {
          ret = TRUE;
          goto cleanup;
        }
    }

  /*
   * If we have an auto-indenter and the event is for a trigger key, then we
   * chain up to the parent class to insert the character, and then let the
   * auto-indenter fix things up.
   */
  if (priv->buffer != NULL && priv->auto_indent)
    {
      IdeIndenter *indenter = ide_source_view_get_indenter (self);

      /*
       * Indenter may be NULL and that is okay, the IdeIdenter API
       * knows how to deal with that situation by emulating GtkSourceView
       * indentation style.
       */

      if (ide_indenter_is_trigger (indenter, event))
        {
          ide_source_view_do_indent (self, event, indenter);
          ret = TRUE;
          goto cleanup;
        }
    }

  /*
   * If repeat-with-count is set, we need to repeat the insertion multiple times.
   */
  if (priv->count &&
      priv->mode &&
      ide_source_view_mode_get_repeat_insert_with_count (priv->mode))
    {
      for (gint i = MAX (1, priv->count); i > 0; i--)
        ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);
      priv->count = 0;
    }
  else
    {
      ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->key_press_event (widget, event);
    }

  /*
   * If we just inserted ({["', we might want to insert a matching close.
   */
  if (ret)
    ide_source_view_maybe_insert_match (self, event);

  /*
   * Only scroll to the insert mark if we made a change.
   */
  if (priv->change_sequence != change_sequence)
    ide_source_view_scroll_mark_onscreen (self, insert, FALSE, 0, 0);

cleanup:
  priv->in_key_press = FALSE;

  return ret;
}

static gboolean
ide_source_view_key_release_event (GtkWidget   *widget,
                                   GdkEventKey *event)
{
  IdeSourceView *self = (IdeSourceView *) widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkWidgetClass *klass = GTK_WIDGET_CLASS (ide_source_view_parent_class);
  gboolean ret = klass->key_release_event (widget, event);

  if (priv->definition_src_location)
    {
      ide_source_view_reset_definition_highlight (self);
    }

  return ret;
}

static gboolean
ide_source_view_process_press_on_definition (IdeSourceView  *self,
                                             GdkEventButton *event)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextIter iter;
  GtkTextWindowType window_type;
  gint buffer_x;
  gint buffer_y;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (event != NULL);

  window_type = gtk_text_view_get_window_type (text_view, event->window);
  gtk_text_view_window_to_buffer_coords (text_view,
                                         window_type,
                                         event->x,
                                         event->y,
                                         &buffer_x,
                                         &buffer_y);
  gtk_text_view_get_iter_at_location (text_view,
                                      &iter,
                                      buffer_x,
                                      buffer_y);

  if (priv->definition_src_location != NULL)
    {
      GtkTextIter definition_highlight_start;
      GtkTextIter definition_highlight_end;

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
                                        &definition_highlight_start,
                                        priv->definition_highlight_start_mark);

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
                                        &definition_highlight_end,
                                        priv->definition_highlight_end_mark);

      if (gtk_text_iter_in_range (&iter, &definition_highlight_start, &definition_highlight_end))
        {
          g_autoptr(IdeSourceLocation) src_location = NULL;

          src_location = ide_source_location_ref (priv->definition_src_location);
          ide_source_view_reset_definition_highlight (self);
          g_signal_emit (self, signals [FOCUS_LOCATION], 0, src_location);
        }

      return TRUE;
    }

  return FALSE;
}

static gboolean
ide_source_view_real_button_press_event (GtkWidget      *widget,
                                         GdkEventButton *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)widget;
  gboolean ret;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));

  if (ide_source_view_process_press_on_definition (self, event))
    return TRUE;

  if (event->button == GDK_BUTTON_PRIMARY)
    {
      if (event->state & GDK_CONTROL_MASK)
        {
          if (!ide_cursor_is_enabled (priv->cursor))
            ide_cursor_add_cursor (priv->cursor, IDE_CURSOR_SELECT);
        }
      else if (ide_cursor_is_enabled (priv->cursor))
        {
          ide_cursor_remove_cursors (priv->cursor);
        }
    }

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->button_press_event (widget, event);

  /*
   * Keep mark on the last character if the sourceviewmode dictates such.
   */
  if (gtk_widget_has_focus (widget) &&
      priv->mode &&
      ide_source_view_mode_get_keep_mark_on_char (priv->mode))
    {
      GtkTextBuffer *buffer;
      GtkTextMark *insert;
      GtkTextMark *selection;
      GtkTextIter iter;
      GtkTextIter iter2;

      buffer = gtk_text_view_get_buffer (text_view);
      insert = gtk_text_buffer_get_insert (buffer);
      selection = gtk_text_buffer_get_selection_bound (buffer);

      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter2, selection);

      if (gtk_text_iter_ends_line (&iter) && !gtk_text_iter_starts_line (&iter))
        {
          GtkTextIter prev = iter;

          gtk_text_iter_backward_char (&prev);
          if (gtk_text_iter_equal (&iter, &iter2))
            gtk_text_buffer_select_range (buffer, &prev, &prev);
        }
    }

  /*
   * Update our target column so movements don't cause us to revert
   * to the previous column.
   */
  ide_source_view_save_column (self);

  return ret;
}

static gboolean
ide_source_view_real_button_release_event (GtkWidget      *widget,
                                           GdkEventButton *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean ret;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->button_release_event (widget, event);

  if ((event->button == GDK_BUTTON_PRIMARY) && (event->state & GDK_CONTROL_MASK))
    ide_cursor_add_cursor (priv->cursor, IDE_CURSOR_SELECT);

  return ret;
}

static gboolean
ide_source_get_word_from_iter (const GtkTextIter *iter,
                               GtkTextIter *word_start,
                               GtkTextIter *word_end)
{
  /* Just using forward/backward to word start/end is not enough
   * because _ break words when using those functions while they
   * are commonly used in the same word in code */
  *word_start = *iter;
  *word_end = *iter;

  do
    {
      const gunichar c = gtk_text_iter_get_char (word_end);
      if (!(g_unichar_isalnum (c) || c == '_'))
        break;
    }
  while (gtk_text_iter_forward_char (word_end));

  if (gtk_text_iter_equal (word_start, word_end))
    {
      /* Iter is not inside a word */
      return FALSE;
    }

  while (gtk_text_iter_backward_char (word_start))
    {
      const gunichar c = gtk_text_iter_get_char (word_start);
      if (!(g_unichar_isalnum (c) || c == '_'))
        {
          gtk_text_iter_forward_char (word_start);
          break;
        }
    }

  return (!gtk_text_iter_equal (word_start, word_end));
}

static void
ide_source_view_get_definition_on_mouse_over_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  g_autoptr(DefinitionHighlightData) data = user_data;
  IdeSourceViewPrivate *priv;
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeSymbol) symbol = NULL;
  g_autoptr(GError) error = NULL;
  IdeSourceLocation *srcloc;
  IdeSymbolKind kind;

  IDE_ENTRY;

  g_assert (data != NULL);
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_SOURCE_VIEW (data->self));

  priv = ide_source_view_get_instance_private (data->self);

  symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, &error);

  if (symbol == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("%s", error->message);
      IDE_EXIT;
    }

  /* Short circuit if the async operation completed after we closed */
  if (priv->buffer == NULL)
    IDE_EXIT;

  kind = ide_symbol_get_kind (symbol);

  srcloc = ide_symbol_get_definition_location (symbol);
  if (srcloc != NULL)
    {
      GtkTextIter word_start;
      GtkTextIter word_end;

      if (priv->definition_src_location &&
          (priv->definition_src_location != srcloc))
        g_clear_pointer (&priv->definition_src_location, ide_source_location_unref);

      if (priv->definition_src_location == NULL)
        priv->definition_src_location = ide_source_location_ref (srcloc);

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                        &word_start, data->word_start_mark);
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                        &word_end, data->word_end_mark);

      if (kind == IDE_SYMBOL_HEADER)
        {
          GtkTextIter line_start = word_start;
          GtkTextIter line_end = word_end;
          g_autofree gchar *line_text = NULL;
          g_autoptr (GMatchInfo) matchInfo = NULL;

          gtk_text_iter_set_line_offset (&line_start, 0);
          gtk_text_iter_forward_to_line_end (&line_end);

          line_text = gtk_text_iter_get_visible_text (&line_start,&line_end);

          g_regex_match (priv->include_regex, line_text, 0, &matchInfo);

          if (g_match_info_matches (matchInfo))
            {
              gint start_pos;
              gint end_pos;
              g_match_info_fetch_pos (matchInfo,
                                      0,
                                      &start_pos,
                                      &end_pos);
              word_start = line_start;
              word_end   = line_start;

              gtk_text_iter_set_line_index (&word_start, start_pos);
              gtk_text_iter_set_line_index (&word_end, end_pos);
            }
        }

      gtk_text_buffer_apply_tag_by_name (GTK_TEXT_BUFFER (priv->buffer),
                                         TAG_DEFINITION, &word_start, &word_end);

      if (priv->definition_highlight_start_mark != NULL)
        gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (priv->buffer),
                                   priv->definition_highlight_start_mark,
                                   &word_start);

      if (priv->definition_highlight_end_mark != NULL)
        gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (priv->buffer),
                                   priv->definition_highlight_end_mark,
                                   &word_end);

      ide_source_view_set_cursor_from_name (data->self, "pointer");
    }
  else
    ide_source_view_reset_definition_highlight (data->self);

  IDE_EXIT;
}

static gboolean
ide_source_view_real_motion_notify_event (GtkWidget      *widget,
                                          GdkEventMotion *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextIter iter;
  GtkTextIter start_iter;
  GtkTextIter line_start_iter;
  GtkTextIter end_iter;
  gunichar ch;
  gint buffer_x;
  gint buffer_y;
  GtkTextWindowType window_type;
  DefinitionHighlightData *data;
  gboolean word_found = FALSE;
  gboolean ret;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->motion_notify_event (widget, event);

  if ((event->state & ALL_ACCELS_MASK) != DEFINITION_HIGHLIGHT_MODIFIER)
    {
      if (priv->definition_src_location)
        ide_source_view_reset_definition_highlight (self);

      return ret;
    }

  window_type = gtk_text_view_get_window_type (GTK_TEXT_VIEW (self), event->window);
  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self),
                                         window_type,
                                         event->x,
                                         event->y,
                                         &buffer_x,
                                         &buffer_y);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self),
                                      &iter,
                                      buffer_x,
                                      buffer_y);

  /* Workaround about a Clang bug where <> includes are not correctly reported */
  line_start_iter = iter;
  gtk_text_iter_set_line_offset (&line_start_iter, 0);

  if (gtk_text_iter_ends_line (&line_start_iter))
    goto cleanup;

  while ((ch = gtk_text_iter_get_char (&line_start_iter)) &&
         g_unichar_isspace (ch) &&
         gtk_text_iter_forward_char (&line_start_iter))
    ;

  if (ch == '#')
    {
      g_autofree gchar *str = NULL;
      GtkTextIter sharp_iter = line_start_iter;
      GtkTextIter line_end_iter = iter;

      gtk_text_iter_forward_char (&line_start_iter);
      gtk_text_iter_forward_to_line_end (&line_end_iter);
      str = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (priv->buffer), &line_start_iter, &line_end_iter, FALSE);
      g_strchug (str);
      if (g_str_has_prefix (str, "include"))
        {
          iter = start_iter = sharp_iter;
          end_iter = line_end_iter;
          word_found = TRUE;
        }
    }

  if (!word_found && !ide_source_get_word_from_iter (&iter, &start_iter, &end_iter))
    goto cleanup;

  if (priv->definition_src_location)
    {
      GtkTextIter definition_highlight_start;
      GtkTextIter definition_highlight_end;

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
                                        &definition_highlight_start,
                                        priv->definition_highlight_start_mark);

      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer),
                                        &definition_highlight_end,
                                        priv->definition_highlight_end_mark);

      if (gtk_text_iter_equal (&definition_highlight_start, &start_iter) &&
          gtk_text_iter_equal (&definition_highlight_end, &end_iter))
        return ret;

      ide_source_view_reset_definition_highlight (self);
    }

  data = g_slice_new0 (DefinitionHighlightData);
  data->self = self;
  data->word_start_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (priv->buffer),
                                                       NULL, &start_iter, TRUE);
  data->word_end_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (priv->buffer),
                                                     NULL, &end_iter, TRUE);

  g_object_ref (data->self);
  g_object_ref (data->word_start_mark);
  g_object_ref (data->word_end_mark);

  ide_buffer_get_symbol_at_location_async (priv->buffer,
                                           &iter,
                                           NULL,
                                           ide_source_view_get_definition_on_mouse_over_cb,
                                           data);

  return ret;

cleanup:
  ide_source_view_reset_definition_highlight (self);
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
ide_source_view_real_add_cursor (IdeSourceView *self,
                                 IdeCursorType  type)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ide_cursor_add_cursor (priv->cursor, type);

  IDE_EXIT;
}

static void
ide_source_view_real_remove_cursors (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ide_cursor_remove_cursors (priv->cursor);

  IDE_EXIT;
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
ide_source_view_real_capture_modifier (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  priv->waiting_for_capture = TRUE;
  while ((priv->modifier == 0) && gtk_widget_has_focus (GTK_WIDGET (self)))
    gtk_main_iteration ();
  priv->waiting_for_capture = FALSE;
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
ide_source_view_real_clear_modifier (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  priv->modifier = 0;
}

static void
ide_source_view_real_clear_search (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceSearchSettings *search_settings;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  search_settings = gtk_source_search_context_get_settings (priv->search_context);
  gtk_source_search_settings_set_search_text (search_settings, "");
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
  ide_source_view_scroll_mark_onscreen (self, insert, FALSE, 0, 0);
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
  GtkTextIter begin;
  GtkTextIter end;
  gboolean editable;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));

  buffer = gtk_text_view_get_buffer (text_view);
  editable = gtk_text_view_get_editable (text_view);

  if (!editable)
    return;

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  gtk_text_iter_order (&begin, &end);

  if (gtk_text_iter_is_end (&end) && gtk_text_iter_starts_line (&begin))
    {
      gtk_text_buffer_begin_user_action (buffer);
      gtk_text_iter_backward_char (&begin);
      gtk_text_buffer_delete (buffer, &begin, &end);
      gtk_text_buffer_end_user_action (buffer);
    }
  else
    {
      gtk_text_buffer_delete_selection (buffer, TRUE, editable);
    }

  ide_source_view_save_column (self);
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
  if (priv->count && level)
    level *= (gint)priv->count;

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
ide_source_view_real_insert_modifier (IdeSourceView *self,
                                      gboolean       use_count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  gchar str[8] = { 0 };
  gint count = 1;
  gint len;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (!priv->modifier)
    return;

  if (use_count)
    count = MAX (1, priv->count);

  len = g_unichar_to_utf8 (priv->modifier, str);
  str [len] = '\0';

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  gtk_text_buffer_begin_user_action (buffer);
  for (gint i = 0; i < count; i++)
    gtk_text_buffer_insert_at_cursor (buffer, str, len);
  gtk_text_buffer_end_user_action (buffer);
}

static void
ide_source_view_real_duplicate_entire_line (IdeSourceView *self)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextIter begin, end;
  gboolean selected;
  g_autofree gchar *text = NULL;
  g_autofree gchar *duplicate_line = NULL;
  GtkTextMark *cursor;
  GtkTextBuffer *buffer;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (text_view);
  cursor = gtk_text_buffer_get_insert (buffer);

  gtk_text_buffer_begin_user_action (buffer);

  selected = gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (selected)
    {
      duplicate_line = gtk_text_iter_get_text (&begin, &end);
      gtk_text_buffer_insert (buffer, &begin, duplicate_line, -1);
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &begin, cursor);
      end = begin;

      gtk_text_iter_set_line_offset (&begin, 0);
      gtk_text_iter_forward_to_line_end (&end);

      if (gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&end))
        {
          text = gtk_text_iter_get_text (&begin, &end);
          duplicate_line = g_strconcat (text, "\n", NULL);
          gtk_text_buffer_insert (buffer, &begin, duplicate_line, -1);
        }
    }

  gtk_text_buffer_end_user_action (buffer);
}

static void
ide_source_view_real_join_lines (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  /*
   * We want to leave the cursor inbetween the joined lines, so lets create an
   * insert mark and delete it later after we reposition the cursor.
   */
  mark = gtk_text_buffer_create_mark (buffer, NULL, &end, TRUE);

  /* join lines and restore the insert mark inbetween joined lines. */
  gtk_text_buffer_begin_user_action (buffer);
  gtk_source_buffer_join_lines (GTK_SOURCE_BUFFER (buffer), &begin, &end);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, mark);
  gtk_text_buffer_select_range (buffer, &end, &end);
  gtk_text_buffer_end_user_action (buffer);

  /* Remove our temporary mark. */
  gtk_text_buffer_delete_mark (buffer, mark);
}

static void
ide_source_view_real_jump (IdeSourceView     *self,
                           const GtkTextIter *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeBackForwardItem *item;
  IdeContext *context;
  IdeFile *file;
  IdeUri *uri;
  GtkTextMark *mark;
  GtkTextBuffer *buffer;
  gchar *fragment;
  guint line;
  guint line_column;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (location);

  line = gtk_text_iter_get_line (location);
  line_column = ide_source_view_get_visual_column (self, location);

  IDE_TRACE_MSG ("Jump to %d:%d", line + 1, line_column + 1);

  if (priv->back_forward_list == NULL)
    IDE_EXIT;

  if (priv->buffer == NULL)
    IDE_EXIT;

  context = ide_buffer_get_context (priv->buffer);
  if (context == NULL)
    IDE_EXIT;

  file = ide_buffer_get_file (priv->buffer);
  if (file == NULL)
    IDE_EXIT;

  uri = ide_uri_new_from_file (ide_file_get_file (file));
  fragment = g_strdup_printf ("L%u_%u", line + 1, line_column + 1);
  ide_uri_set_fragment (uri, fragment);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_create_mark (buffer, NULL, location, FALSE);
  item = ide_back_forward_item_new (context, uri, mark);
  ide_back_forward_list_push (priv->back_forward_list, item);

  g_object_unref (item);
  ide_uri_unref (uri);
  g_free (fragment);

  IDE_EXIT;
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
  guint target_line_column;

  /*
   * NOTE:
   *
   * In this function, we try to improve how pasting works in GtkTextView. There are some
   * semantics that make things easier by tracking the paste of an entire line versus small
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
  target_line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self), &iter);

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
          target_line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self),
                                                                  &iter);
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
          if (!gtk_text_iter_ends_line (&iter))
            gtk_text_iter_forward_char (&iter);
          gtk_text_buffer_select_range (buffer, &iter, &iter);
        }

      GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->paste_clipboard (text_view);

      if (!place_cursor_at_original)
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          target_line = gtk_text_iter_get_line (&iter);
          target_line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self),
                                                                  &iter);
        }
    }

  gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, target_line, 0);
  ide_source_view_get_iter_at_visual_column (self, target_line_column, &iter);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  gtk_text_buffer_end_user_action (buffer);
}

static void
ide_source_view_real_selection_theatric (IdeSourceView         *self,
                                         IdeSourceViewTheatric  theatric)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert ((theatric == IDE_SOURCE_VIEW_THEATRIC_EXPAND) ||
            (theatric == IDE_SOURCE_VIEW_THEATRIC_SHRINK));

  if (!ide_source_view_can_animate (self))
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
ide_source_view_save_column (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  priv->target_line_column = ide_source_view_get_visual_column (self, &iter);
}

static void
ide_source_view_update_display_name (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  const gchar *display_name = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->mode != NULL)
    display_name = ide_source_view_mode_get_display_name (priv->mode);

  if (g_strcmp0 (display_name, priv->display_name) != 0)
    {
      g_free (priv->display_name);
      priv->display_name = g_strdup (display_name);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODE_DISPLAY_NAME]);
    }
}

static void
ide_source_view_real_set_mode (IdeSourceView         *self,
                               const gchar           *mode,
                               IdeSourceViewModeType  type)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autofree gchar *suggested_default = NULL;
  gboolean overwrite;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (!priv->buffer)
    IDE_EXIT;

#ifdef IDE_ENABLE_TRACE
  {
    const gchar *old_mode = "null";

    if (priv->mode)
      old_mode = ide_source_view_mode_get_name (priv->mode);
    IDE_TRACE_MSG ("transition from mode (%s) to (%s)", old_mode, mode ?: "<default>");
  }
#endif

  ide_source_view_save_column (self);

  if (priv->mode)
    {
      IdeSourceViewMode *old_mode = g_object_ref (priv->mode);
      const gchar *str;

      /* see if this mode suggested a default next mode */
      str = ide_source_view_mode_get_default_mode (old_mode);
      suggested_default = g_strdup (str);

      g_clear_object (&priv->mode);
      g_object_unref (old_mode);
    }

  if (mode == NULL)
    {
      mode = suggested_default ?: "default";
      type = IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT;
    }

  /* reset the count when switching to permanent mode */
  if (type == IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT)
    priv->count = 0;

  priv->mode = _ide_source_view_mode_new (GTK_WIDGET (self), mode, type);

  overwrite = ide_source_view_mode_get_block_cursor (priv->mode);
  if (overwrite != gtk_text_view_get_overwrite (GTK_TEXT_VIEW (self)))
    gtk_text_view_set_overwrite (GTK_TEXT_VIEW (self), overwrite);
  g_object_notify (G_OBJECT (self), "overwrite");

  ide_source_view_update_auto_indent_override (self);

  ide_source_view_update_display_name (self);

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
  gint count = -1;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (apply_count)
    count = priv->count;

  if (priv->scrolling_to_scroll_mark)
    priv->scrolling_to_scroll_mark = FALSE;

  _ide_source_view_apply_movement (self,
                                   movement,
                                   extend_selection,
                                   exclusive,
                                   count,
                                   priv->command_str,
                                   priv->command,
                                   priv->modifier,
                                   priv->search_char,
                                   &priv->target_line_column);
}

static void
ide_source_view__search_forward_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  GtkSourceSearchContext *search_context = (GtkSourceSearchContext *)object;
  IdeSourceViewPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped;
  g_autoptr(SearchMovement) mv = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  priv = ide_source_view_get_instance_private (mv->self);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  insert = gtk_text_buffer_get_insert (buffer);

  if (!gtk_source_search_context_forward_finish2 (search_context,
                                                  result,
                                                  &begin,
                                                  &end,
                                                  &has_wrapped,
                                                  &error))
    {
      /*
       * If we didn't find a match, scroll back to the position when the search
       * started.
       */
      if (priv->rubberband_search)
        ide_source_view_rollback_search (mv->self);
      return;
    }

  mv->count--;

  gtk_text_iter_order (&begin, &end);

  /*
   * If we still need to move further back in the document, let's search again.
   */
  if (mv->count > 0)
    {
      gtk_source_search_context_forward_async (search_context,
                                               &end,
                                               NULL,
                                               ide_source_view__search_forward_cb,
                                               search_movement_ref (mv));
      return;
    }

  if (!mv->exclusive && !mv->select_match)
    gtk_text_iter_forward_char (&begin);

  if (mv->extend_selection)
    gtk_text_buffer_move_mark (buffer, insert, &begin);
  else if (mv->select_match)
    gtk_text_buffer_select_range (buffer, &begin, &end);
  else
    gtk_text_buffer_select_range (buffer, &begin, &begin);

  /* if we arent focused, update the saved position marker */
  if (!gtk_widget_has_focus (GTK_WIDGET (mv->self)))
    ide_source_view_real_save_insert_mark (mv->self);

  ide_source_view_scroll_mark_onscreen (mv->self, insert, TRUE, 0.5, 0.5);
}

static void
ide_source_view__search_backward_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GtkSourceSearchContext *search_context = (GtkSourceSearchContext *)object;
  IdeSourceViewPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped;
  g_autoptr(SearchMovement) mv = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_assert (mv);
  g_assert (IDE_IS_SOURCE_VIEW (mv->self));

  priv = ide_source_view_get_instance_private (mv->self);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (mv->self));
  insert = gtk_text_buffer_get_insert (buffer);

  if (!gtk_source_search_context_backward_finish2 (search_context,
                                                   result,
                                                   &begin,
                                                   &end,
                                                   &has_wrapped,
                                                   &error))
    {
      /*
       * If we didn't find a match, scroll back to the position when the search
       * started.
       */
      if (priv->rubberband_search)
        ide_source_view_rollback_search (mv->self);
      return;
    }

  mv->count--;

  gtk_text_iter_order (&begin, &end);

  /*
   * If we still need to move further back in the document, let's search again.
   */
  if (mv->count > 0)
    {
      gtk_source_search_context_backward_async (search_context,
                                                &begin,
                                                NULL,
                                                ide_source_view__search_backward_cb,
                                                search_movement_ref (mv));
      return;
    }

  if (mv->exclusive && !mv->select_match)
    gtk_text_iter_forward_char (&begin);

  if (mv->extend_selection)
    gtk_text_buffer_move_mark (buffer, insert, &begin);
  else if (mv->select_match)
    gtk_text_buffer_select_range (buffer, &begin, &end);
  else
    gtk_text_buffer_select_range (buffer, &begin, &begin);

  /* if we arent focused, update the saved position marker */
  if (!gtk_widget_has_focus (GTK_WIDGET (mv->self)))
    ide_source_view_real_save_insert_mark (mv->self);

  ide_source_view_scroll_mark_onscreen (mv->self, insert, TRUE, 0.5, 0.5);
}

static void
ide_source_view_real_move_search (IdeSourceView    *self,
                                  GtkDirectionType  dir,
                                  gboolean          extend_selection,
                                  gboolean          select_match,
                                  gboolean          exclusive,
                                  gboolean          apply_count,
                                  gint              word_boundaries)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  g_autoptr(SearchMovement) mv = NULL;
  GtkTextBuffer *buffer;
  GtkTextMark *insert_mark;
  GtkTextIter insert_iter;
  GtkSourceSearchSettings *settings;
  const gchar *search_text;
  gboolean is_forward;

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (!priv->search_context)
    return;

  if (dir == GTK_DIR_TAB_BACKWARD)
    {
      switch (priv->search_direction)
        {
        case GTK_DIR_LEFT:
          dir = GTK_DIR_RIGHT;
          break;
        case GTK_DIR_RIGHT:
          dir = GTK_DIR_LEFT;
          break;
        case GTK_DIR_UP:
          dir = GTK_DIR_DOWN;
          break;
        case GTK_DIR_DOWN:
          dir = GTK_DIR_UP;
          break;
        case GTK_DIR_TAB_FORWARD:
        case GTK_DIR_TAB_BACKWARD:
        default:
          g_return_if_reached ();
        }
    }
  else if (dir == GTK_DIR_TAB_FORWARD)
    {
      dir = priv->search_direction;
    }
  else
    {
      priv->search_direction = dir;
    }

  gtk_source_search_context_set_highlight (priv->search_context, TRUE);

  settings = gtk_source_search_context_get_settings (priv->search_context);

  /*
   * A word_boundaries value other than 0 or 1 means that we don't modify
   * the word_boundaries search setting.
   */
  if (word_boundaries == 0)
    gtk_source_search_settings_set_at_word_boundaries (settings, FALSE);
  else if (word_boundaries == 1)
    gtk_source_search_settings_set_at_word_boundaries (settings, TRUE);

  search_text = gtk_source_search_settings_get_search_text (settings);

  if (search_text == NULL || search_text[0] == '\0')
    {
      if (priv->saved_search_text == NULL)
        return;
      gtk_source_search_settings_set_search_text (settings, priv->saved_search_text);
    }

  buffer = gtk_text_view_get_buffer (text_view);
  insert_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert_mark);

  is_forward = (dir == GTK_DIR_DOWN) || (dir == GTK_DIR_RIGHT);

  mv = search_movement_new (self, is_forward, extend_selection, select_match,
                            exclusive, apply_count);

  if (is_forward)
    {
      gtk_text_iter_forward_char (&insert_iter);
      gtk_source_search_context_forward_async (priv->search_context,
                                               &insert_iter,
                                               NULL,
                                               ide_source_view__search_forward_cb,
                                               search_movement_ref (mv));
    }
  else
    {
      gtk_source_search_context_backward_async (priv->search_context,
                                                &insert_iter,
                                                NULL,
                                                ide_source_view__search_backward_cb,
                                                search_movement_ref (mv));
    }
}

static void
ide_source_view_real_move_error (IdeSourceView    *self,
                                 GtkDirectionType  dir)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  gboolean wrap_around = TRUE;
  gboolean (*movement) (GtkTextIter *) = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (!priv->buffer)
    return;

  if (dir == GTK_DIR_RIGHT)
    dir = GTK_DIR_DOWN;
  else if (dir == GTK_DIR_LEFT)
    dir = GTK_DIR_UP;

  /*
   * TODO: This is not particularly very efficient. But I didn't feel like
   *       plumbing access to the diagnostics set and duplicating most of
   *       the code for getting a diagnostic at a line. Once the diagnostics
   *       get support for fast lookups (bloom filter or something) then
   *       we should change to that.
   */

  if (dir == GTK_DIR_DOWN)
    movement = gtk_text_iter_forward_line;
  else
    movement = gtk_text_iter_backward_line;

  buffer = GTK_TEXT_BUFFER (priv->buffer);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

wrapped:
  while (movement (&iter))
    {
      IdeDiagnostic *diag;

      diag = ide_buffer_get_diagnostic_at_iter (priv->buffer, &iter);

      if (diag)
        {
          IdeSourceLocation *location;

          location = ide_diagnostic_get_location (diag);

          if (location)
            {
              guint line_offset;

              line_offset = ide_source_location_get_line_offset (location);
              gtk_text_iter_set_line_offset (&iter, 0);
              for (; line_offset; line_offset--)
                if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
                  break;

              gtk_text_buffer_select_range (buffer, &iter, &iter);
              ide_source_view_scroll_mark_onscreen (self, insert, TRUE, 0.5, 0.5);
              return;
            }

          break;
        }
    }

  if (wrap_around)
    {
      if (dir == GTK_DIR_DOWN)
        gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (priv->buffer), &iter);
      else
        gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (priv->buffer), &iter);
      wrap_around = FALSE;
      goto wrapped;
    }
}

static void
ide_source_view_real_restore_insert_mark_full (IdeSourceView *self,
                                               gboolean       move_mark)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->insert_mark_cleared)
    {
      priv->insert_mark_cleared = FALSE;
      return;
    }

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, priv->saved_line, 0);
  ide_source_view_get_iter_at_visual_column (self, priv->saved_line_column, &iter);
  gtk_text_buffer_get_iter_at_line_offset (buffer,
                                           &selection,
                                           priv->saved_selection_line,
                                           0);
  ide_source_view_get_iter_at_visual_column (self,
                                             priv->saved_selection_line_column,
                                             &selection);

  gtk_text_buffer_select_range (buffer, &iter, &selection);

  if (move_mark)
    {
      GtkTextMark *insert;

      insert = gtk_text_buffer_get_insert (buffer);
      ide_source_view_scroll_mark_onscreen (self, insert, FALSE, 0, 0);
    }
}

static void
ide_source_view_real_restore_insert_mark (IdeSourceView *self)
{
  ide_source_view_real_restore_insert_mark_full (self, TRUE);
}

static void
ide_source_view_real_save_insert_mark (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection_bound;
  GtkTextIter iter;
  GtkTextIter selection;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  priv->insert_mark_cleared = FALSE;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);
  selection_bound = gtk_text_buffer_get_selection_bound (buffer);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection, selection_bound);

  priv->saved_line = gtk_text_iter_get_line (&iter);
  priv->saved_line_column = ide_source_view_get_visual_column (self, &iter);
  priv->saved_selection_line = gtk_text_iter_get_line (&selection);
  priv->saved_selection_line_column = ide_source_view_get_visual_column (self, &selection);

  priv->target_line_column = priv->saved_line_column;
}

static void
ide_source_view_real_save_command (IdeSourceView *self)
{
  GdkEvent *event;
  guint keyval;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  event = gtk_get_current_event ();
  if (event && gdk_event_get_keyval (event, &keyval))
    priv->command = (gunichar)keyval;
}

static void
ide_source_view_real_save_search_char (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->modifier)
    priv->search_char = priv->modifier;
}

/* In string mode, the search act only on the current line,
 * search a string to the right if we are not already in one,
 * and only inner_left is used ( inner_right is set to it )
 */
static void
ide_source_view_real_select_inner (IdeSourceView *self,
                                   const gchar   *inner_left,
                                   const gchar   *inner_right,
                                   gboolean       exclusive,
                                   gboolean       string_mode)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gunichar unichar_inner_left;
  gunichar unichar_inner_right;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  unichar_inner_left = g_utf8_get_char (inner_left);
  unichar_inner_right = g_utf8_get_char (inner_right);

  _ide_source_view_select_inner (self,
                                 unichar_inner_left,
                                 unichar_inner_right,
                                 priv->count,
                                 exclusive,
                                 string_mode);
}

static void
ide_source_view_real_select_tag (IdeSourceView *self,
                                 gboolean       exclusive)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  _ide_source_view_select_tag (self, priv->count, exclusive);
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
                                   const GtkTextIter       *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeContext *ide_context;
  IdeSourceSnippetContext *context;
  IdeFile *file;
  GFile *gfile;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));
  g_assert (location != NULL);

  context = ide_source_snippet_get_context (snippet);

  if (priv->buffer != NULL)
    {
      if ((file = ide_buffer_get_file (priv->buffer)) &&
          (gfile = ide_file_get_file (file)))
        {
          g_autofree gchar *name = NULL;

          name = g_file_get_basename (gfile);
          ide_source_snippet_context_add_variable (context, "filename", name);
        }

      if ((ide_context = ide_buffer_get_context (priv->buffer)))
        {
          IdeVcs *vcs;
          IdeVcsConfig *vcs_config;

          vcs = ide_context_get_vcs (ide_context);
          if ((vcs_config = ide_vcs_get_config (vcs)))
            {
              GValue value = G_VALUE_INIT;

              g_value_init (&value, G_TYPE_STRING);

              ide_vcs_config_get_config (vcs_config, IDE_VCS_CONFIG_FULL_NAME, &value);

              if (!ide_str_empty0 (g_value_get_string (&value)))
                {
                  ide_source_snippet_context_add_shared_variable (context, "author", g_value_get_string (&value));
                  ide_source_snippet_context_add_shared_variable (context, "fullname", g_value_get_string (&value));
                  ide_source_snippet_context_add_shared_variable (context, "username", g_value_get_string (&value));
                }

              g_value_reset (&value);

              ide_vcs_config_get_config (vcs_config, IDE_VCS_CONFIG_EMAIL, &value);

              if (!ide_str_empty0 (g_value_get_string (&value)))
                ide_source_snippet_context_add_shared_variable (context, "email", g_value_get_string (&value));

              g_value_unset (&value);
              g_object_unref (vcs_config);
            }
        }
    }
}

static void
ide_source_view_real_set_search_text (IdeSourceView *self,
                                      const gchar   *search_text,
                                      gboolean       from_selection)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autofree gchar *str = NULL;
  GtkSourceSearchSettings *settings;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (!priv->search_context)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (from_selection)
    {
      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
      str = gtk_text_iter_get_slice (&begin, &end);
      search_text = str;
    }

  ide_source_view_sync_rubberband_mark (self);

  settings = gtk_source_search_context_get_settings (priv->search_context);
  gtk_source_search_settings_set_search_text (settings, search_text);
}

static void
ide_source_view_real_reindent (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  IdeIndenter *indenter;
  GtkTextIter begin;
  GtkTextIter end;
  GdkWindow *window;
  GtkTextIter iter;
  guint i;
  guint first_line;
  g_autoptr(GPtrArray) lines = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->buffer == NULL)
    return;

  /* indenter may be NULL and that is okay */
  indenter = ide_source_view_get_indenter (self);

  buffer = GTK_TEXT_BUFFER (priv->buffer);
  window = gtk_text_view_get_window (GTK_TEXT_VIEW (self), GTK_TEXT_WINDOW_TEXT);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  gtk_text_iter_set_line_offset (&begin, 0);
  first_line = gtk_text_iter_get_line (&begin);

  /* if the end position is at index 0 of the next line (common with
   * line mode in vim), then move it back to the end of the previous
   * line, since we don't really care about that next line.
   */
  if (gtk_text_iter_starts_line (&end) &&
      gtk_text_iter_get_line (&begin) != gtk_text_iter_get_line (&end))
    gtk_text_iter_backward_char (&end);

  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  lines = g_ptr_array_new_with_free_func (g_free);

  if (gtk_text_iter_compare (&begin, &end) == 0)
    g_ptr_array_add (lines, g_strdup (""));
  else
    for (iter = begin;
         gtk_text_iter_compare (&iter, &end) < 0;
         gtk_text_iter_forward_line (&iter))
      {
        GtkTextIter line_end = iter;
        gchar *line;

        if (!gtk_text_iter_ends_line (&line_end))
          gtk_text_iter_forward_to_line_end (&line_end);

        line = gtk_text_iter_get_slice (&iter, &line_end);
        g_ptr_array_add (lines, g_strstrip (line));
      }

  gtk_text_buffer_begin_user_action (buffer);

  gtk_text_buffer_delete (buffer, &begin, &end);

  for (i = 0; i < lines->len; i++)
    {
      g_autofree gchar *indent = NULL;
      const gchar *line;
      GdkEventKey *event;
      gint cursor_offset;

      line = g_ptr_array_index (lines, i);
      event = dzl_gdk_synthesize_event_key (window, '\n');
      indent = ide_indenter_format (indenter, GTK_TEXT_VIEW (self), &begin, &end, &cursor_offset, event);
      gdk_event_free ((GdkEvent *)event);

      if (indent != NULL)
        {
          if (!gtk_text_iter_equal (&begin, &end))
            gtk_text_buffer_delete (buffer, &begin, &end);

          gtk_text_buffer_insert (buffer, &begin, indent, -1);
          gtk_text_buffer_insert (buffer, &begin, line, -1);

          if (i != lines->len - 1)
            gtk_text_buffer_insert (buffer, &begin, "\n", -1);
        }

      end = begin;
    }

  gtk_text_buffer_end_user_action (buffer);

  /* Advance to first non-whitespace */
  gtk_text_iter_set_line (&begin, first_line);
  while (!gtk_text_iter_ends_line (&begin) &&
         g_unichar_isspace (gtk_text_iter_get_char (&begin)))
    gtk_text_iter_forward_char (&begin);

  gtk_text_buffer_select_range (buffer, &begin, &begin);
}

static void
ide_source_view_set_overscroll_num_lines (IdeSourceView *self,
                                          gint           num_lines)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gint height = gtk_widget_get_allocated_height (GTK_WIDGET (self));
  gint new_margin;

  priv->overscroll_num_lines = num_lines;
  new_margin = priv->overscroll_num_lines * priv->cached_char_height;

  if (new_margin < 0)
    new_margin = height + new_margin;

  new_margin = CLAMP (new_margin, 0, height);

  g_object_set (self, "bottom-margin", new_margin, NULL);
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

  _ide_source_view_init_shortcuts (self);

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
                                             "show-line-deletions", TRUE,
                                             "size", 2,
                                             "visible", priv->show_line_changes,
                                             "xpad", 3,
                                             NULL);
  g_object_ref (priv->line_change_renderer);
  gtk_source_gutter_insert (gutter, priv->line_change_renderer, 0);

  visible = ((priv->buffer != NULL) &&
             priv->show_line_diagnostics &&
             ide_buffer_get_highlight_diagnostics (priv->buffer));
  priv->line_diagnostics_renderer = g_object_new (IDE_TYPE_LINE_DIAGNOSTICS_GUTTER_RENDERER,
                                                  "size", 16,
                                                  "visible", visible,
                                                  "xpad", 2,
                                                  NULL);
  g_object_ref (priv->line_diagnostics_renderer);
  gtk_source_gutter_insert (gutter, priv->line_diagnostics_renderer, -100);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_DIAGNOSTICS]);

  priv->definition_src_location = NULL;
  ide_source_view_reset_definition_highlight (self);
}

static void
ide_source_view_real_insert_at_cursor (GtkTextView *text_view,
                                       const gchar *str)
{
  IdeSourceView *self = (IdeSourceView *)text_view;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (str);

  GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->insert_at_cursor (text_view, str);

  buffer = gtk_text_view_get_buffer (text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  ide_source_view_scroll_mark_onscreen (self, insert, FALSE, 0, 0);
}

static void
ide_source_view_real_sort (IdeSourceView *self,
                           gboolean       ignore_case,
                           gboolean       reverse)
{
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkSourceSortFlags sort_flags = GTK_SOURCE_SORT_FLAGS_NONE;

  g_assert (GTK_TEXT_VIEW (self));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    gtk_text_buffer_get_bounds (buffer, &begin, &end);

  if (!ignore_case)
    sort_flags |= GTK_SOURCE_SORT_FLAGS_CASE_SENSITIVE;

  if (reverse)
    sort_flags |= GTK_SOURCE_SORT_FLAGS_REVERSE_ORDER;

  gtk_source_buffer_sort_lines (GTK_SOURCE_BUFFER (buffer),
                                &begin,
                                &end,
                                sort_flags,
                                0);
}

static void
ide_source_view_draw_snippet_background (IdeSourceView    *self,
                                         cairo_t          *cr,
                                         IdeSourceSnippet *snippet,
                                         gint              width)
{
  GtkTextBuffer *buffer;
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextMark *mark_begin;
  GtkTextMark *mark_end;
  GdkRectangle r;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (cr);
  g_assert (IDE_IS_SOURCE_SNIPPET (snippet));

  buffer = gtk_text_view_get_buffer (text_view);

  mark_begin = ide_source_snippet_get_mark_begin (snippet);
  mark_end = ide_source_snippet_get_mark_end (snippet);

  if (!mark_begin || !mark_end)
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, &begin, mark_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &end, mark_end);

  get_rect_for_iters (text_view, &begin, &end, &r, GTK_TEXT_WINDOW_TEXT);

  gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_TEXT, r.x, r.y, &r.x, &r.y);

  dzl_cairo_rounded_rectangle (cr, &r, 5, 5);

  cairo_fill (cr);
}

static void
ide_source_view_draw_snippets_background (IdeSourceView *self,
                                          cairo_t       *cr)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = GTK_TEXT_VIEW (self);
  GdkWindow *window;
  gint width;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (cr);

  window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT);
  width = gdk_window_get_width (window);

  gdk_cairo_set_source_rgba (cr, &priv->snippet_area_background_rgba);

  cairo_save (cr);

  for (guint i = 0; i < priv->snippets->length; i++)
    {
      IdeSourceSnippet *snippet = g_queue_peek_nth (priv->snippets, i);

      ide_source_view_draw_snippet_background (self,
                                               cr,
                                               snippet,
                                               width - ((priv->snippets->length - i) * 10));
    }

  cairo_restore (cr);
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
  dzl_cairo_rounded_rectangle (cr, &r, radius, radius);
  cairo_fill (cr);
}

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
   * TODO: Add support for multi-line matches. When @begin and @end are not
   *       on the same line, we need to add the match region to @region so
   *       ide_source_view_draw_search_bubbles() can draw search bubbles
   *       around it.
   */
}

static guint
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
  gboolean has_wrapped;
  guint count = 1;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (region);
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_assert (begin);
  g_assert (end);

  if (!gtk_source_search_context_forward2 (search_context,
                                           begin,
                                           &first_begin,
                                           &match_end,
                                           &has_wrapped))
    return 0;

  add_match (text_view, region, &first_begin, &match_end);

  for (;; )
    {
      gtk_text_iter_assign (&new_begin, &match_end);

      if (gtk_source_search_context_forward2 (search_context,
                                              &new_begin,
                                              &match_begin,
                                              &match_end,
                                              &has_wrapped) &&
          (gtk_text_iter_compare (&match_begin, end) < 0) &&
          (gtk_text_iter_compare (&first_begin, &match_begin) != 0))
        {
          add_match (text_view, region, &match_begin, &match_end);
          count++;
          continue;
        }

      break;
    }

  return count;
}

void
ide_source_view_draw_search_bubbles (IdeSourceView *self,
                                     cairo_t       *cr)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  cairo_region_t *clip_region;
  cairo_region_t *match_region;
  GdkRectangle area;
  GtkTextIter begin;
  GtkTextIter end;
  cairo_rectangle_int_t r;
  guint count;
  gint buffer_x = 0;
  gint buffer_y = 0;
  gint n;
  gint i;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (cr);

  if (!priv->search_context || !gtk_source_search_context_get_highlight (priv->search_context))
    return;

  if (!gdk_cairo_get_clip_rectangle (cr, &area))
    gtk_widget_get_allocation (GTK_WIDGET (self), &area);

  gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_TEXT,
                                         area.x, area.y, &buffer_x, &buffer_y);
  gtk_text_view_get_iter_at_location (text_view, &begin, buffer_x, buffer_y);
  gtk_text_view_get_iter_at_location (text_view, &end,
                                      buffer_x + area.width,
                                      buffer_y + area.height);

  clip_region = cairo_region_create_rectangle (&area);
  match_region = cairo_region_create ();
  count = add_matches (text_view, match_region, priv->search_context, &begin, &end);

  cairo_region_subtract (clip_region, match_region);

  if (priv->show_search_shadow &&
      ((count > 0) || gtk_source_search_context_get_occurrences_count (priv->search_context) > 0))
    {
      gdk_cairo_region (cr, clip_region);
      gdk_cairo_set_source_rgba (cr, &priv->search_shadow_rgba);
      cairo_fill (cr);
    }

  gdk_cairo_region (cr, clip_region);
  cairo_clip (cr);

  n = cairo_region_num_rectangles (match_region);

  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (match_region, i, &r);
      draw_bezel (cr, &r, 3, &priv->bubble_color1);
      draw_bezel (cr, &r, 2, &priv->bubble_color2);
    }

  cairo_region_destroy (clip_region);
  cairo_region_destroy (match_region);
}

static void
ide_source_view_real_draw_layer (GtkTextView      *text_view,
                                 GtkTextViewLayer  layer,
                                 cairo_t          *cr)
{
  IdeSourceView *self = (IdeSourceView *)text_view;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (cr);

  GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->draw_layer (text_view, layer, cr);

  if (layer == GTK_TEXT_VIEW_LAYER_BELOW_TEXT)
    {
      if (priv->snippets->length)
        ide_source_view_draw_snippets_background (self, cr);
    }

  if (layer == GTK_TEXT_VIEW_LAYER_ABOVE)
    {
      if (priv->show_search_bubbles)
        {
          cairo_save (cr);
          ide_source_view_draw_search_bubbles (self, cr);
          cairo_restore (cr);
        }
    }
}

static gboolean
ide_source_view_real_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
  GtkTextView *text_view = (GtkTextView *)widget;
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv =  ide_source_view_get_instance_private (self);
  gboolean ret;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (cr);

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->draw (widget, cr);

  if (priv->show_search_shadow &&
      priv->search_context &&
      (gtk_source_search_context_get_occurrences_count (priv->search_context) > 0))
    {
      GdkWindow *window;
      GdkRectangle rect;

      window = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_LEFT);

      gdk_window_get_position (window, &rect.x, &rect.y);
      rect.width = gdk_window_get_width (window);
      rect.height = gdk_window_get_height (window);

      cairo_save (cr);
      gdk_cairo_rectangle (cr, &rect);
      gdk_cairo_set_source_rgba (cr, &priv->search_shadow_rgba);
      cairo_fill (cr);
      cairo_restore (cr);
    }

  return ret;
}

static gboolean
ide_source_view_focus_in_event (GtkWidget     *widget,
                                GdkEventFocus *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceCompletion *completion;
  IdeWorkbench *workbench;
  gboolean ret;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * Restore the completion window now that we have regained focus.
   */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_unblock_interactive (completion);

  /*
   * Restore the insert mark, but ignore selections (since we cant ensure they
   * will stay looking selected, as the other frame could be a view into our
   * own buffer).
   */
  workbench = ide_widget_get_workbench (GTK_WIDGET (widget));
  if (!workbench || ide_workbench_get_selection_owner (workbench) != G_OBJECT (self))
    {
      priv->saved_selection_line = priv->saved_line;
      priv->saved_selection_line_column = priv->saved_line_column;
    }

  ide_source_view_real_restore_insert_mark_full (self, FALSE);

  /* restore line highlight if enabled */
  if (priv->highlight_current_line)
    gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self), TRUE);

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->focus_in_event (widget, event);

  return ret;
}

static gboolean
ide_source_view_focus_out_event (GtkWidget     *widget,
                                 GdkEventFocus *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  GtkSourceCompletion *completion;
  gboolean ret;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /* save our insert mark for when we focus back in. it could have moved if
   * another view into the same buffer has caused the insert mark to jump.
   */
  ide_source_view_real_save_insert_mark (self);
  ide_source_view_sync_rubberband_mark (self);

  ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->focus_out_event (widget, event);

  /*
   * Block the completion window while we are not focused. It confuses text
   * insertion and such.
   */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_block_interactive (completion);

  /* We don't want highlight-current-line unless the widget is in focus, so
   * disable it until we get re-focused.
   */
  gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self), FALSE);

  return ret;
}

static void
ide_source_view_real_begin_macro (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceViewModeType mode_type;
  GdkEvent *event;
  const gchar *mode_name;
  gunichar modifier;
  gint count;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->in_replay_macro)
    IDE_GOTO (in_replay);

  priv->recording_macro = TRUE;

  mode_type = ide_source_view_mode_get_mode_type (priv->mode);
  mode_name = ide_source_view_mode_get_name (priv->mode);
  modifier = priv->modifier;
  count = priv->count;
  event = gtk_get_current_event ();

  g_clear_object (&priv->capture);

  priv->capture = ide_source_view_capture_new (self, mode_name, mode_type, count, modifier);
  ide_source_view_capture_record_event (priv->capture, event, count, modifier);
  gdk_event_free (event);

in_replay:
  IDE_EXIT;
}

static void
ide_source_view_real_end_macro (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->in_replay_macro)
    IDE_GOTO (in_replay);

  priv->recording_macro = FALSE;

in_replay:
  IDE_EXIT;
}

static void
ide_source_view_goto_definition_symbol_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  g_autoptr(IdeSourceView) self = user_data;
  g_autoptr(IdeSymbol) symbol = NULL;
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(GError) error = NULL;
  IdeSourceLocation *srcloc;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  symbol = ide_buffer_get_symbol_at_location_finish (buffer, result, &error);

  if (symbol == NULL)
    {
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  srcloc = ide_symbol_get_definition_location (symbol);

  if (srcloc != NULL)
    {
      guint line = ide_source_location_get_line (srcloc);
      guint line_offset = ide_source_location_get_line_offset (srcloc);
      IdeFile *file = ide_source_location_get_file (srcloc);
      IdeFile *our_file = ide_buffer_get_file (buffer);

#ifdef IDE_ENABLE_TRACE
      const gchar *filename = ide_file_get_path (file);

      IDE_TRACE_MSG ("%s => %s +%u:%u",
                     ide_symbol_get_name (symbol),
                     filename, line+1, line_offset+1);
#endif

      /* Stash our current position for jump-back */
      ide_source_view_jump (self, NULL);

      /*
       * If we are navigating within this file, just stay captive instead of
       * potentially allowing jumping to the file in another editor.
       */
      if (ide_file_equal (file, our_file))
        {
          GtkTextIter iter;

          gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                                   &iter, line, line_offset);
          gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
          ide_source_view_scroll_to_insert (self);
          IDE_EXIT;
        }

      g_signal_emit (self, signals [FOCUS_LOCATION], 0, srcloc);
    }

  IDE_EXIT;
}

static void
ide_source_view_real_goto_definition (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->buffer != NULL)
    {
      GtkTextMark *insert;
      GtkTextIter iter;

      insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (priv->buffer));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &iter, insert);

      ide_buffer_get_symbol_at_location_async (priv->buffer,
                                               &iter,
                                               NULL,
                                               ide_source_view_goto_definition_symbol_cb,
                                               g_object_ref (self));
    }
}

static void
ide_source_view_real_hide_completion (IdeSourceView *self)
{
  GtkSourceCompletion *completion;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_hide (completion);
}

static void
ide_source_view_real_replay_macro (IdeSourceView *self,
                                   gboolean       use_count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceViewCapture *capture;
  gint count = 1;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->recording_macro)
    {
      g_warning ("Cannot playback macro while recording.");
      IDE_EXIT;
    }

  if (priv->in_replay_macro)
    {
      g_warning ("Cannot playback macro while playing back macro.");
      IDE_EXIT;
    }

  if (priv->capture == NULL)
    return;

  if (use_count)
    count = MAX (1, priv->count);

  IDE_TRACE_MSG ("Replaying capture %d times.", count);

  priv->in_replay_macro = TRUE;
  capture = priv->capture, priv->capture = NULL;
  for (gint i = 0; i < count; i++)
    ide_source_view_capture_replay (capture);
  g_clear_object (&priv->capture);
  priv->capture = capture, capture = NULL;
  priv->in_replay_macro = FALSE;

  IDE_EXIT;
}

static void
ide_source_view_begin_user_action (IdeSourceView *self)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_begin_user_action (buffer);
}

static void
ide_source_view_end_user_action (IdeSourceView *self)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_end_user_action (buffer);
}

gboolean
ide_source_view_get_overwrite (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  if (gtk_text_view_get_overwrite (GTK_TEXT_VIEW (self)))
    {
      if (!priv->mode || !ide_source_view_mode_get_block_cursor (priv->mode))
        return TRUE;
    }

  return FALSE;
}

static gchar *
ide_source_view_get_fixit_label (IdeSourceView *self,
                                 IdeFixit      *fixit)
{
  IdeSourceLocation *begin_loc;
  IdeSourceLocation *end_loc;
  IdeSourceRange *range;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gchar *old_text = NULL;
  gchar *new_text = NULL;
  gchar *tmp;
  gchar *ret = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (fixit != NULL);

  range = ide_fixit_get_range (fixit);
  if (range == NULL)
    goto cleanup;

  new_text = g_strdup (ide_fixit_get_text (fixit));
  if (new_text == NULL)
    goto cleanup;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  if (!IDE_IS_BUFFER (buffer))
    goto cleanup;

  begin_loc = ide_source_range_get_begin (range);
  end_loc = ide_source_range_get_end (range);

  ide_buffer_get_iter_at_source_location (IDE_BUFFER (buffer), &begin, begin_loc);
  ide_buffer_get_iter_at_source_location (IDE_BUFFER (buffer), &end, end_loc);

  old_text = gtk_text_iter_get_slice (&begin, &end);

  if (strlen (old_text) > FIXIT_LABEL_LEN_MAX)
    {
      tmp = old_text;
      old_text = g_strndup (tmp, FIXIT_LABEL_LEN_MAX);
      g_free (tmp);
    }

  if (strlen (new_text) > FIXIT_LABEL_LEN_MAX)
    {
      tmp = new_text;
      new_text = g_strndup (tmp, FIXIT_LABEL_LEN_MAX);
      g_free (tmp);
    }

  tmp = old_text;
  old_text = g_markup_escape_text (old_text, -1);
  g_free (tmp);

  tmp = new_text;
  new_text = g_markup_escape_text (new_text, -1);
  g_free (tmp);

  if (old_text [0] == '\0')
    ret = g_strdup_printf (_("Insert “%s”"), new_text);
  else
    ret = g_strdup_printf (_("Replace “%s” with “%s”"), old_text, new_text);

cleanup:
  g_free (old_text);
  g_free (new_text);

  return ret;
}

static void
ide_source_view__fixit_activate (IdeSourceView *self,
                                 GtkMenuItem   *menu_item)
{
  IdeFixit *fixit;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_MENU_ITEM (menu_item));

  fixit = g_object_get_data (G_OBJECT (menu_item), "IDE_FIXIT");

  if (fixit != NULL)
    {
      IdeSourceLocation *srcloc;
      IdeSourceRange *range;
      GtkTextBuffer *buffer;
      const gchar *text;
      GtkTextIter begin;
      GtkTextIter end;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
      if (!IDE_IS_BUFFER (buffer))
        return;

      text = ide_fixit_get_text (fixit);
      range = ide_fixit_get_range (fixit);

      srcloc = ide_source_range_get_begin (range);
      ide_buffer_get_iter_at_source_location (IDE_BUFFER (buffer), &begin, srcloc);

      srcloc = ide_source_range_get_end (range);
      ide_buffer_get_iter_at_source_location (IDE_BUFFER (buffer), &end, srcloc);

      gtk_text_buffer_begin_user_action (buffer);
      gtk_text_buffer_delete (buffer, &begin, &end);
      gtk_text_buffer_insert (buffer, &begin, text, -1);
      gtk_text_buffer_end_user_action (buffer);
    }
}

static void
ide_source_view_real_populate_popup (GtkTextView *text_view,
                                     GtkWidget   *popup)
{
  IdeSourceView *self = (IdeSourceView *)text_view;
  GtkSeparatorMenuItem *sep;
  GtkTextBuffer *buffer;
  GtkMenuItem *menu_item;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;
  IdeDiagnostic *diagnostic;
  GMenu *model;

  g_assert (GTK_IS_TEXT_VIEW (text_view));
  g_assert (GTK_IS_WIDGET (popup));

  GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->populate_popup (text_view, popup);

  if (!GTK_IS_MENU (popup))
    return;

  buffer = gtk_text_view_get_buffer (text_view);
  if (!IDE_IS_BUFFER (buffer))
    return;

  model = dzl_application_get_menu_by_id (DZL_APPLICATION_DEFAULT, "ide-source-view-popup-menu");
  gtk_menu_shell_bind_model (GTK_MENU_SHELL (popup), G_MENU_MODEL (model), NULL, TRUE);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  /*
   * TODO: I'm pretty sure we don't want to use the insert mark, but the
   *       location of the button-press-event (if there was one).
   */
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  /*
   * Check if we have a diagnostic at this position and if there are fixits associated with it.
   * If so, display the "Apply Fixit" menu item with available fixits.
   */
  diagnostic = ide_buffer_get_diagnostic_at_iter (IDE_BUFFER (buffer), &iter);

  if (diagnostic != NULL)
    {
      guint num_fixits;

      num_fixits = ide_diagnostic_get_num_fixits (diagnostic);

      if (num_fixits > 0)
        {
          GtkWidget *parent;
          GtkWidget *submenu;
          guint i;

          sep = g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM,
                              "visible", TRUE,
                              NULL);
          gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), GTK_WIDGET (sep));

          submenu = gtk_menu_new ();

          parent = g_object_new (GTK_TYPE_MENU_ITEM,
                                 "label", _("Apply Fix-It"),
                                 "submenu", submenu,
                                 "visible", TRUE,
                                 NULL);
          gtk_menu_shell_prepend (GTK_MENU_SHELL (popup), parent);

          for (i = 0; i < num_fixits; i++)
            {
              IdeFixit *fixit;
              gchar *label;

              fixit = ide_diagnostic_get_fixit (diagnostic, i);
              label = ide_source_view_get_fixit_label (self, fixit);

              menu_item = g_object_new (GTK_TYPE_MENU_ITEM,
                                        "label", label,
                                        "visible", TRUE,
                                        NULL);
              gtk_menu_shell_append (GTK_MENU_SHELL (submenu), GTK_WIDGET (menu_item));

              g_object_set_data_full (G_OBJECT (menu_item),
                                      "IDE_FIXIT",
                                      ide_fixit_ref (fixit),
                                      (GDestroyNotify)ide_fixit_unref);

              g_signal_connect_object (menu_item,
                                       "activate",
                                       G_CALLBACK (ide_source_view__fixit_activate),
                                       self,
                                       G_CONNECT_SWAPPED);
            }
        }
    }
}

static void
ide_source_view_real_rebuild_highlight (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->buffer != NULL)
    ide_buffer_rehighlight (priv->buffer);

  IDE_EXIT;
}

static gboolean
ignore_invalid_buffers (GBinding     *binding,
                        const GValue *from_value,
                        GValue       *to_value,
                        gpointer      user_data)
{
  if (G_VALUE_HOLDS (from_value, GTK_TYPE_TEXT_BUFFER))
    {
      GtkTextBuffer *buffer;

      buffer = g_value_get_object (from_value);

      if (IDE_IS_BUFFER (buffer))
        {
          g_value_set_object (to_value, buffer);
          return TRUE;
        }
    }

  g_value_set_object (to_value, NULL);

  return TRUE;
}

static void
ide_source_view_set_indent_style (IdeSourceView  *self,
                                  IdeIndentStyle  indent_style)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert ((indent_style == IDE_INDENT_STYLE_SPACES) ||
            (indent_style == IDE_INDENT_STYLE_TABS));

  if (indent_style == IDE_INDENT_STYLE_SPACES)
    gtk_source_view_set_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self), TRUE);
  else
    gtk_source_view_set_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self), FALSE);
}

static gboolean
ide_source_view_do_size_allocate_hack_cb (gpointer data)
{
  IdeSourceView *self = data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkAllocation alloc = priv->delay_size_allocation;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  priv->delay_size_allocate_chainup = 0;

  GTK_WIDGET_CLASS (ide_source_view_parent_class)->size_allocate (GTK_WIDGET (self), &alloc);

  return G_SOURCE_REMOVE;
}

/*
 * HACK:
 *
 * We really want the panels in Builder to be as smooth as possible when
 * animating in and out of the scene. However, since these are not floating
 * panels, we have the challenge of trying to go through the entire relayout,
 * pixelcache, draw cycle many times per-second. Most systems are simply not
 * up to the task.
 *
 * We can, however, take a shortcut when shrinking the allocation. We can
 * simply defer the allocation request that would normally be chained up
 * to GtkTextView and finish that work after the animation has completed.
 * We use a simple heuristic to determine this, simply "missing" a size
 * allocation from the typical frame clock cycle.
 */
static gboolean
ide_source_view_do_size_allocate_hack (IdeSourceView *self,
                                       GtkAllocation *allocation)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkWidget *widget = (GtkWidget *)self;
  GtkAllocation old;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (allocation != NULL);

  /*
   * If we are shrinking the allocation, we can go forward with the hack.
   * If not, we will abort our request and do the normal chainup cycle.
   */
  gtk_widget_get_allocation (widget, &old);
  if ((old.width < allocation->width) || (old.height < allocation->height))
    return FALSE;

  /*
   * Save the allocation for later. We'll need it to apply after our timeout
   * which will occur just after the last frame (or sooner if we stall the
   * drawing pipeline).
   */
  priv->delay_size_allocation = *allocation;

  /*
   * Register our timeout to occur just after a normal frame interval.
   * If we are animating at 60 FPS, we should get another size-allocate within
   * the frame cycle, typically 17 msec.
   */
  if (priv->delay_size_allocate_chainup)
    g_source_remove (priv->delay_size_allocate_chainup);
  priv->delay_size_allocate_chainup = g_timeout_add (30,
                                                     ide_source_view_do_size_allocate_hack_cb,
                                                     self);

  return TRUE;
}

static void
ide_source_view_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (allocation != NULL);

  if (!ide_source_view_do_size_allocate_hack (self, allocation))
    GTK_WIDGET_CLASS (ide_source_view_parent_class)->size_allocate (GTK_WIDGET (self), allocation);

  ide_source_view_set_overscroll_num_lines (self, priv->overscroll_num_lines);
}

static gboolean
ide_source_view_scroll_event (GtkWidget      *widget,
                              GdkEventScroll *event)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  gboolean ret = GDK_EVENT_PROPAGATE;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * If the user started a manual scroll while we were attempting to scroll to
   * the target position, just abort our delayed scroll.
   */
  priv->scrolling_to_scroll_mark = FALSE;

  /*
   * Be forward-portable against changes underneath us.
   */
  if (GTK_WIDGET_CLASS (ide_source_view_parent_class)->scroll_event)
    ret = GTK_WIDGET_CLASS (ide_source_view_parent_class)->scroll_event (widget, event);

  return ret;
}

static void
ide_source_view_real_reset_font_size (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->font_scale != FONT_SCALE_NORMAL)
    {
      priv->font_scale = FONT_SCALE_NORMAL;
      ide_source_view_rebuild_css (self);
    }
}

static void
ide_source_view_real_increase_font_size (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->font_scale < LAST_FONT_SCALE - 1)
    {
      priv->font_scale++;
      ide_source_view_rebuild_css (self);
    }
}

static void
ide_source_view_real_decrease_font_size (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (priv->font_scale > 0)
    {
      priv->font_scale--;
      ide_source_view_rebuild_css (self);
    }
}

static void
ide_source_view_real_delete_from_cursor (GtkTextView   *text_view,
                                         GtkDeleteType  delete_type,
                                         gint           count)
{
  if (delete_type == GTK_DELETE_PARAGRAPHS)
    ide_text_util_delete_line (text_view, count);
  else
    GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->delete_from_cursor (text_view,
                                                                            delete_type,
                                                                            count);
}

static void
ide_source_view_real_select_all (IdeSourceView *self,
                                 gboolean       select_)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_signal_chain_from_overridden_handler (self, select_);

  priv->insert_mark_cleared = TRUE;
}

static void
ide_source_view_rename_changed (IdeSourceView    *self,
                                DzlSimplePopover *popover)
{
  const gchar *text;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (DZL_IS_SIMPLE_POPOVER (popover));

  text = dzl_simple_popover_get_text (popover);
  dzl_simple_popover_set_ready (popover, text != NULL);

  IDE_EXIT;
}

static void
ide_source_view_rename_edits_applied (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  g_autoptr(IdeSourceView) self = user_data;
  IdeBufferManager *buffer_manager = (IdeBufferManager *)object;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_MANAGER (buffer_manager));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * The completion window can sometimes popup when performing the replacements
   * so we manually hide that window here.
   */
  ide_source_view_real_hide_completion (self);

  IDE_EXIT;
}

static void
ide_source_view_rename_edits_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeRenameProvider *provider = (IdeRenameProvider *)object;
  g_autoptr(IdeSourceView) self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(GPtrArray) edits = NULL;
  g_autoptr(GError) error = NULL;
  IdeBufferManager *buffer_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_RENAME_PROVIDER (provider));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (!ide_rename_provider_rename_finish (provider, result, &edits, &error))
    {
      /* TODO: Propagate error to UI */
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  g_assert (edits != NULL);

  context = ide_buffer_get_context (priv->buffer);
  buffer_manager = ide_context_get_buffer_manager (context);

  ide_buffer_manager_apply_edits_async (buffer_manager,
                                        g_steal_pointer (&edits),
                                        NULL,
                                        ide_source_view_rename_edits_applied,
                                        g_steal_pointer (&self));

  IDE_EXIT;
}

static void
ide_source_view_rename_activate (IdeSourceView    *self,
                                 const gchar      *text,
                                 DzlSimplePopover *popover)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(IdeSourceLocation) location = NULL;
  IdeRenameProvider *provider;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (text != NULL);
  g_assert (DZL_IS_SIMPLE_POPOVER (popover));

  if (NULL == (provider = ide_buffer_get_rename_provider (priv->buffer)))
    IDE_EXIT;

  location = ide_buffer_get_insert_location (priv->buffer);

  ide_rename_provider_rename_async (provider,
                                    location,
                                    text,
                                    NULL,
                                    ide_source_view_rename_edits_cb,
                                    g_object_ref (self));

  /*
   * TODO: We should probably lock all buffers so that we can ensure
   *       that our edit points are correct by time we get called back.
   */

  gtk_popover_popdown (GTK_POPOVER (popover));

  IDE_EXIT;
}

static void
ide_source_view_real_begin_rename (IdeSourceView *self)
{
  IdeRenameProvider *provider;
  DzlSimplePopover *popover;
  g_autofree gchar *uri = NULL;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GdkRectangle loc;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  provider = ide_buffer_get_rename_provider (IDE_BUFFER (buffer));

  if (provider == NULL)
    {
      g_message ("Cannot rename, operation requires an IdeRenameProvider");
      return;
    }

  insert = gtk_text_buffer_get_insert (buffer);
  uri = ide_buffer_get_uri (IDE_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  IDE_TRACE_MSG ("Renaming symbol found at %s: %d:%d",
                 uri,
                 gtk_text_iter_get_line (&iter) + 1,
                 gtk_text_iter_get_line_offset (&iter) + 1);

  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &loc);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (self),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         loc.x, loc.y, &loc.x, &loc.y);

  popover = g_object_new (DZL_TYPE_SIMPLE_POPOVER,
                          "title", _("Rename symbol"),
                          "button-text", _("Rename"),
                          "relative-to", self,
                          "pointing-to", &loc,
                          NULL);

  g_signal_connect_object (popover,
                           "changed",
                           G_CALLBACK (ide_source_view_rename_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (popover,
                           "activate",
                           G_CALLBACK (ide_source_view_rename_activate),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_popover_popup (GTK_POPOVER (popover));

  IDE_EXIT;
}

static void
ide_source_view_format_selection_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;
  g_autoptr(IdeSourceView) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_buffer_format_selection_finish (buffer, result, &error))
    g_warning ("%s", error->message);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (self), TRUE);

  IDE_EXIT;
}

static void
ide_source_view_real_format_selection (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(IdeFormatterOptions) options = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  options = ide_formatter_options_new ();
  ide_formatter_options_set_tab_width (options,
    gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self)));
  ide_formatter_options_set_insert_spaces (options,
    gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self)));

  gtk_text_view_set_editable (GTK_TEXT_VIEW (self), FALSE);
  ide_buffer_format_selection_async (priv->buffer,
                                     options,
                                     NULL,
                                     ide_source_view_format_selection_cb,
                                     g_object_ref (self));

  IDE_EXIT;
}

static void
ide_source_view_real_find_references_jump (IdeSourceView *self,
                                           GtkListBoxRow *row,
                                           GtkListBox    *list_box)
{
  IdeSourceLocation *location;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (GTK_IS_LIST_BOX (list_box));

  location = g_object_get_data (G_OBJECT (row), "IDE_SOURCE_LOCATION");

  if (location != NULL)
    g_signal_emit (self, signals [FOCUS_LOCATION], 0, location);

  IDE_EXIT;
}

static gboolean
insert_mark_within_range (IdeBuffer      *buffer,
                          IdeSourceRange *range)
{
  GtkTextMark *insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  IdeSourceLocation *begin = ide_source_range_get_begin (range);
  IdeSourceLocation *end = ide_source_range_get_end (range);
  GtkTextIter iter;
  GtkTextIter begin_iter;
  GtkTextIter end_iter;

  if (!begin || !end)
    return FALSE;

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, insert);
  ide_buffer_get_iter_at_source_location (buffer, &begin_iter, begin);
  ide_buffer_get_iter_at_source_location (buffer, &end_iter, end);

  return gtk_text_iter_compare (&begin_iter, &iter) <= 0 &&
         gtk_text_iter_compare (&end_iter, &iter) >= 0;

}

static void
ide_source_view_find_references_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  IdeSymbolResolver *resolver = (IdeSymbolResolver *)object;
  g_autoptr(IdeSourceView) self = user_data;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(GPtrArray) references = NULL;
  g_autoptr(GError) error = NULL;
  GtkScrolledWindow *scroller;
  GtkPopover *popover;
  GtkListBox *list_box;
  GtkTextMark *insert;
  GtkTextIter iter;
  GdkRectangle loc;

  IDE_ENTRY;

  g_assert (IDE_IS_SYMBOL_RESOLVER (resolver));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  references = ide_symbol_resolver_find_references_finish (resolver, result, &error);

  if (error != NULL)
    g_debug ("%s", error->message);

  /* Ignore popover if we are no longer visible or not top-most */
  if (!gtk_widget_get_visible (GTK_WIDGET (self)) ||
      !gtk_widget_get_child_visible (GTK_WIDGET (self)))
    IDE_EXIT;

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (priv->buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &iter, insert);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->buffer), &iter, &iter);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &loc);
  gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (self),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         loc.x, loc.y, &loc.x, &loc.y);

  popover = g_object_new (GTK_TYPE_POPOVER,
                          "modal", TRUE,
                          "position", GTK_POS_TOP,
                          "relative-to", self,
                          "pointing-to", &loc,
                          NULL);

  scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                           "min-content-height", 35,
                           "max-content-height", 200,
                           "propagate-natural-height", TRUE,
                           "propagate-natural-width", TRUE,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (popover), GTK_WIDGET (scroller));

  list_box = g_object_new (GTK_TYPE_LIST_BOX,
                           "visible", TRUE,
                           NULL);
  gtk_container_add (GTK_CONTAINER (scroller), GTK_WIDGET (list_box));

  if (references != NULL && references->len > 0)
    {
      IdeContext *context = ide_buffer_get_context (priv->buffer);
      IdeVcs *vcs = ide_context_get_vcs (context);
      GFile *workdir = ide_vcs_get_working_directory (vcs);

      for (guint i = 0; i < references->len; i++)
        {
          IdeSourceRange *range = g_ptr_array_index (references, i);
          IdeSourceLocation *begin = ide_source_range_get_begin (range);
          IdeFile *file = ide_source_location_get_file (begin);
          GFile *gfile = ide_file_get_file (file);
          guint line = ide_source_location_get_line (begin);
          guint line_offset = ide_source_location_get_line_offset (begin);
          g_autofree gchar *name = NULL;
          g_autofree gchar *text = NULL;
          GtkListBoxRow *row;
          GtkLabel *label;

          if (g_file_has_prefix (gfile, workdir))
            name = g_file_get_relative_path (workdir, gfile);
          else if (g_file_is_native (gfile))
            name = g_file_get_path (gfile);
          else
            name = g_file_get_uri (gfile);

          /* translators: %s is the filename, then line number, column number. <> are pango markup */
          text = g_strdup_printf (_("<b>%s</b> — <small>Line %u, Column %u</small>"),
                                  name, line + 1, line_offset + 1);

          label = g_object_new (GTK_TYPE_LABEL,
                                "xalign", 0.0f,
                                "label", text,
                                "use-markup", TRUE,
                                "visible", TRUE,
                                NULL);
          row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                              "child", label,
                              "visible", TRUE,
                              NULL);
          g_object_set_data_full (G_OBJECT (row),
                                  "IDE_SOURCE_LOCATION",
                                  ide_source_location_ref (begin),
                                  (GDestroyNotify)ide_source_location_unref);
          gtk_container_add (GTK_CONTAINER (list_box), GTK_WIDGET (row));

          if (insert_mark_within_range (priv->buffer, range))
            gtk_list_box_select_row (list_box, row);
        }
    }
  else
    {
      GtkLabel *label = g_object_new (GTK_TYPE_LABEL,
                                      "label", _("No references were found"),
                                      "visible", TRUE,
                                      NULL);
      gtk_container_add (GTK_CONTAINER (list_box), GTK_WIDGET (label));
    }

  g_signal_connect_object (list_box,
                           "row-activated",
                           G_CALLBACK (ide_source_view_real_find_references_jump),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_popover_popup (popover);

  g_signal_connect (popover, "hide", G_CALLBACK (gtk_widget_destroy), NULL);

  IDE_EXIT;
}

static void
ide_source_view_real_find_references (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  g_autoptr(IdeSourceLocation) location = NULL;
  IdeSymbolResolver *resolver;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  resolver = ide_buffer_get_symbol_resolver (priv->buffer);

  if (resolver == NULL)
    {
      g_debug ("No symbol resolver is available");
      IDE_EXIT;
    }

  location = ide_buffer_get_insert_location (priv->buffer);

  ide_symbol_resolver_find_references_async (resolver,
                                             location,
                                             NULL,
                                             ide_source_view_find_references_cb,
                                             g_object_ref (self));

  IDE_EXIT;
}

static void
ide_source_view_real_request_documentation (IdeSourceView *self)
{
  g_autofree gchar *word = NULL;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (!gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
    {
      gtk_text_iter_order (&begin, &end);

      if (!_ide_source_iter_starts_extra_natural_word (&begin))
        {
          _ide_source_iter_backward_extra_natural_word_start (&begin);
          end = begin;
        }

      _ide_source_iter_forward_extra_natural_word_end (&end);
    }

  word = gtk_text_iter_get_slice (&begin, &end);

  g_signal_emit (self, signals [DOCUMENTATION_REQUESTED], 0, word);
}

static void
ide_source_view_real_reset (IdeSourceView *self)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  g_signal_emit (self, signals [CLEAR_SEARCH], 0);
  g_signal_emit (self, signals [CLEAR_MODIFIER], 0);
  g_signal_emit (self, signals [CLEAR_SELECTION], 0);
  g_signal_emit (self, signals [CLEAR_COUNT], 0);
  g_signal_emit (self, signals [CLEAR_SNIPPETS], 0);
  g_signal_emit (self, signals [HIDE_COMPLETION], 0);
  g_signal_emit (self, signals [REMOVE_CURSORS], 0);
  g_signal_emit (self, signals [SET_MODE], 0, NULL, IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  if (priv->hadj_animation)
    {
      dzl_animation_stop (priv->hadj_animation);
      ide_clear_weak_pointer (&priv->hadj_animation);
    }

  if (priv->vadj_animation)
    {
      dzl_animation_stop (priv->vadj_animation);
      ide_clear_weak_pointer (&priv->vadj_animation);
    }

  ide_source_view_clear_snippets (self);

  if (priv->delay_size_allocate_chainup)
    {
      g_source_remove (priv->delay_size_allocate_chainup);
      priv->delay_size_allocate_chainup = 0;
    }

  g_clear_object (&priv->capture);
  g_clear_object (&priv->indenter_adapter);
  g_clear_object (&priv->line_change_renderer);
  g_clear_object (&priv->line_diagnostics_renderer);
  g_clear_object (&priv->snippets_provider);
  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->mode);
  g_clear_object (&priv->buffer_signals);
  g_clear_object (&priv->file_setting_bindings);

  if (priv->command_str != NULL)
    {
      g_string_free (priv->command_str, TRUE);
      priv->command_str = NULL;
    }

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);
}

static void
ide_source_view_finalize (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_clear_object (&priv->completion_providers_signals);
  g_clear_pointer (&priv->display_name, g_free);
  g_clear_pointer (&priv->font_desc, pango_font_description_free);
  g_clear_pointer (&priv->selections, g_queue_free);
  g_clear_pointer (&priv->snippets, g_queue_free);
  g_clear_pointer (&priv->include_regex, g_regex_unref);
  g_clear_pointer (&priv->saved_search_text, g_free);

  DZL_COUNTER_DEC (instances);

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

    case PROP_BACK_FORWARD_LIST:
      g_value_set_object (value, ide_source_view_get_back_forward_list (self));
      break;

    case PROP_COUNT:
      g_value_set_int (value, ide_source_view_get_count (self));
      break;

    case PROP_ENABLE_WORD_COMPLETION:
      g_value_set_boolean (value, ide_source_view_get_enable_word_completion (self));
      break;

    case PROP_FILE_SETTINGS:
      g_value_set_object (value, ide_source_view_get_file_settings (self));
      break;

    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      g_value_set_boolean (value, ide_source_view_get_highlight_current_line (self));
      break;

    case PROP_INDENTER:
      g_value_set_object (value, ide_source_view_get_indenter (self));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      g_value_set_boolean (value, ide_source_view_get_insert_matching_brace (self));
      break;

    case PROP_MODE_DISPLAY_NAME:
      g_value_set_string (value, ide_source_view_get_mode_display_name (self));
      break;

    case PROP_OVERWRITE:
      g_value_set_boolean (value, ide_source_view_get_overwrite (self));
      break;

    case PROP_OVERWRITE_BRACES:
      g_value_set_boolean (value, ide_source_view_get_overwrite_braces (self));
      break;

    case PROP_RUBBERBAND_SEARCH:
      g_value_set_boolean (value, ide_source_view_get_rubberband_search (self));
      break;

    case PROP_SCROLL_OFFSET:
      g_value_set_uint (value, ide_source_view_get_scroll_offset (self));
      break;

    case PROP_SEARCH_CONTEXT:
      g_value_set_object (value, ide_source_view_get_search_context (self));
      break;

    case PROP_SEARCH_DIRECTION:
      g_value_set_enum (value, ide_source_view_get_search_direction (self));
      break;

    case PROP_SHOW_GRID_LINES:
      g_value_set_boolean (value, ide_source_view_get_show_grid_lines (self));
      break;

    case PROP_SHOW_LINE_CHANGES:
      g_value_set_boolean (value, ide_source_view_get_show_line_changes (self));
      break;

    case PROP_SHOW_LINE_DIAGNOSTICS:
      g_value_set_boolean (value, ide_source_view_get_show_line_diagnostics (self));
      break;

    case PROP_SHOW_SEARCH_BUBBLES:
      g_value_set_boolean (value, ide_source_view_get_show_search_bubbles (self));
      break;

    case PROP_SHOW_SEARCH_SHADOW:
      g_value_set_boolean (value, ide_source_view_get_show_search_shadow (self));
      break;

    case PROP_SNIPPET_COMPLETION:
      g_value_set_boolean (value, ide_source_view_get_snippet_completion (self));
      break;

    case PROP_OVERSCROLL:
      g_value_set_int (value, priv->overscroll_num_lines);
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
      ide_source_view_update_auto_indent_override (self);
      break;

    case PROP_BACK_FORWARD_LIST:
      ide_source_view_set_back_forward_list (self, g_value_get_object (value));
      break;

    case PROP_COUNT:
      ide_source_view_set_count (self, g_value_get_int (value));
      break;

    case PROP_ENABLE_WORD_COMPLETION:
      ide_source_view_set_enable_word_completion (self, g_value_get_boolean (value));
      break;

    case PROP_FONT_NAME:
      ide_source_view_set_font_name (self, g_value_get_string (value));
      break;

    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      ide_source_view_set_highlight_current_line (self, g_value_get_boolean (value));
      break;

    case PROP_INDENT_STYLE:
      ide_source_view_set_indent_style (self, g_value_get_enum (value));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      ide_source_view_set_insert_matching_brace (self, g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE:
      gtk_text_view_set_overwrite (GTK_TEXT_VIEW (self), g_value_get_boolean (value));
      break;

    case PROP_OVERWRITE_BRACES:
      ide_source_view_set_overwrite_braces (self, g_value_get_boolean (value));
      break;

    case PROP_RUBBERBAND_SEARCH:
      ide_source_view_set_rubberband_search (self, g_value_get_boolean (value));
      break;

    case PROP_SCROLL_OFFSET:
      ide_source_view_set_scroll_offset (self, g_value_get_uint (value));
      break;

    case PROP_SEARCH_DIRECTION:
      ide_source_view_set_search_direction (self, g_value_get_enum (value));
      break;

    case PROP_SHOW_GRID_LINES:
      ide_source_view_set_show_grid_lines (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_CHANGES:
      ide_source_view_set_show_line_changes (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_LINE_DIAGNOSTICS:
      ide_source_view_set_show_line_diagnostics (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SEARCH_BUBBLES:
      ide_source_view_set_show_search_bubbles (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_SEARCH_SHADOW:
      ide_source_view_set_show_search_shadow (self, g_value_get_boolean (value));
      break;

    case PROP_SNIPPET_COMPLETION:
      ide_source_view_set_snippet_completion (self, g_value_get_boolean (value));
      break;

    case PROP_OVERSCROLL:
      ide_source_view_set_overscroll_num_lines (self, g_value_get_int (value));
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
  GtkBindingSet *binding_set;
  GTypeClass *completion_class;

  object_class->constructed = ide_source_view_constructed;
  object_class->dispose = ide_source_view_dispose;
  object_class->finalize = ide_source_view_finalize;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  widget_class->button_press_event = ide_source_view_real_button_press_event;
  widget_class->button_release_event = ide_source_view_real_button_release_event;
  widget_class->motion_notify_event = ide_source_view_real_motion_notify_event;
  widget_class->draw = ide_source_view_real_draw;
  widget_class->focus_in_event = ide_source_view_focus_in_event;
  widget_class->focus_out_event = ide_source_view_focus_out_event;
  widget_class->key_press_event = ide_source_view_key_press_event;
  widget_class->key_release_event = ide_source_view_key_release_event;
  widget_class->query_tooltip = ide_source_view_query_tooltip;
  widget_class->scroll_event = ide_source_view_scroll_event;
  widget_class->size_allocate = ide_source_view_size_allocate;
  widget_class->style_updated = ide_source_view_real_style_updated;

  text_view_class->delete_from_cursor = ide_source_view_real_delete_from_cursor;
  text_view_class->draw_layer = ide_source_view_real_draw_layer;
  text_view_class->insert_at_cursor = ide_source_view_real_insert_at_cursor;
  text_view_class->populate_popup = ide_source_view_real_populate_popup;

  klass->add_cursor = ide_source_view_real_add_cursor;
  klass->remove_cursors = ide_source_view_real_remove_cursors;
  klass->append_to_count = ide_source_view_real_append_to_count;
  klass->begin_macro = ide_source_view_real_begin_macro;
  klass->begin_rename = ide_source_view_real_begin_rename;
  klass->capture_modifier = ide_source_view_real_capture_modifier;
  klass->clear_count = ide_source_view_real_clear_count;
  klass->clear_modifier = ide_source_view_real_clear_modifier;
  klass->clear_search = ide_source_view_real_clear_search;
  klass->clear_selection = ide_source_view_real_clear_selection;
  klass->clear_snippets = ide_source_view_clear_snippets;
  klass->cycle_completion = ide_source_view_real_cycle_completion;
  klass->decrease_font_size = ide_source_view_real_decrease_font_size;
  klass->delete_selection = ide_source_view_real_delete_selection;
  klass->end_macro = ide_source_view_real_end_macro;
  klass->goto_definition = ide_source_view_real_goto_definition;
  klass->hide_completion = ide_source_view_real_hide_completion;
  klass->increase_font_size = ide_source_view_real_increase_font_size;
  klass->indent_selection = ide_source_view_real_indent_selection;
  klass->insert_modifier = ide_source_view_real_insert_modifier;
  klass->jump = ide_source_view_real_jump;
  klass->move_error = ide_source_view_real_move_error;
  klass->move_search = ide_source_view_real_move_search;
  klass->movement = ide_source_view_real_movement;
  klass->paste_clipboard_extended = ide_source_view_real_paste_clipboard_extended;
  klass->pop_selection = ide_source_view_real_pop_selection;
  klass->push_selection = ide_source_view_real_push_selection;
  klass->rebuild_highlight = ide_source_view_real_rebuild_highlight;
  klass->replay_macro = ide_source_view_real_replay_macro;
  klass->request_documentation = ide_source_view_real_request_documentation;
  klass->reset_font_size = ide_source_view_real_reset_font_size;
  klass->restore_insert_mark = ide_source_view_real_restore_insert_mark;
  klass->save_command = ide_source_view_real_save_command;
  klass->save_insert_mark = ide_source_view_real_save_insert_mark;
  klass->save_search_char = ide_source_view_real_save_search_char;
  klass->select_inner = ide_source_view_real_select_inner;
  klass->select_tag = ide_source_view_real_select_tag;
  klass->selection_theatric = ide_source_view_real_selection_theatric;
  klass->set_mode = ide_source_view_real_set_mode;
  klass->set_overwrite = ide_source_view_real_set_overwrite;
  klass->set_search_text = ide_source_view_real_set_search_text;
  klass->sort = ide_source_view_real_sort;
  klass->swap_selection_bounds = ide_source_view_real_swap_selection_bounds;

  g_object_class_override_property (object_class, PROP_AUTO_INDENT, "auto-indent");

  properties [PROP_BACK_FORWARD_LIST] =
    g_param_spec_object ("back-forward-list",
                         "Back Forward List",
                         "The back-forward list to track jumps.",
                         IDE_TYPE_BACK_FORWARD_LIST,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_COUNT] =
    g_param_spec_int ("count",
                      "Count",
                      "The count for movements.",
                      -1,
                      G_MAXINT,
                      0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FILE_SETTINGS] =
    g_param_spec_object ("file-settings",
                         "File Settings",
                         "The file settings that have been loaded for the file.",
                         IDE_TYPE_FILE_SETTINGS,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                        "Font Description",
                        "The Pango font description to use for rendering source.",
                        PANGO_TYPE_FONT_DESCRIPTION,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_ENABLE_WORD_COMPLETION] =
    g_param_spec_boolean ("enable-word-completion",
                          "Enable Word Completion",
                          "If words from all buffers can be used to autocomplete.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_NAME] =
    g_param_spec_string ("font-name",
                         "Font Name",
                         "The Pango font name to use for rendering source.",
                         "Monospace",
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  g_object_class_override_property (object_class,
                                    PROP_HIGHLIGHT_CURRENT_LINE,
                                    "highlight-current-line");

  properties [PROP_INDENTER] =
    g_param_spec_object ("indenter",
                         "Indenter",
                         "Indenter",
                         IDE_TYPE_INDENTER,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_INDENT_STYLE] =
    g_param_spec_enum ("indent-style",
                       "Indent Style",
                       "Indent Style",
                       IDE_TYPE_INDENT_STYLE,
                       IDE_INDENT_STYLE_TABS,
                       (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_INSERT_MATCHING_BRACE] =
    g_param_spec_boolean ("insert-matching-brace",
                          "Insert Matching Brace",
                          "Insert a matching brace/bracket/quotation/parenthesis.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_override_property (object_class, PROP_OVERWRITE, "overwrite");

  properties [PROP_MODE_DISPLAY_NAME] =
    g_param_spec_string ("mode-display-name",
                         "Mode Display Name",
                         "The display name of the keybinding mode.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_OVERWRITE_BRACES] =
    g_param_spec_boolean ("overwrite-braces",
                          "Overwrite Braces",
                          "Overwrite a matching brace/bracket/quotation/parenthesis.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUBBERBAND_SEARCH] =
    g_param_spec_boolean ("rubberband-search",
                          "Rubberband Search",
                          "Auto scroll to next search result without moving insertion caret.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCROLL_OFFSET] =
    g_param_spec_uint ("scroll-offset",
                       "Scroll Offset",
                       "The number of lines between the insertion cursor and screen boundary.",
                       0,
                       G_MAXUINT,
                       0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_CONTEXT] =
    g_param_spec_object ("search-context",
                         "Search Context",
                         "The search context for the view.",
                         GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SEARCH_DIRECTION] =
    g_param_spec_enum ("search-direction",
                       "Search Direction",
                       "The direction searches go for the view.",
                       GTK_TYPE_DIRECTION_TYPE,
                       GTK_DIR_DOWN,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_GRID_LINES] =
    g_param_spec_boolean ("show-grid-lines",
                          "Show Grid Lines",
                          "If the background grid should be shown.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_LINE_CHANGES] =
    g_param_spec_boolean ("show-line-changes",
                          "Show Line Changes",
                          "If line changes should be shown in the left gutter.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeSourceView:show-line-diagnostics:
   *
   * If the diagnostics gutter should be visible.
   *
   * This also requires that IdeBuffer:highlight-diagnostics is set to %TRUE
   * to generate diagnostics.
   */
  properties [PROP_SHOW_LINE_DIAGNOSTICS] =
    g_param_spec_boolean ("show-line-diagnostics",
                          "Show Line Diagnostics",
                          "If line changes diagnostics should be shown in the left gutter.",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_SEARCH_BUBBLES] =
    g_param_spec_boolean ("show-search-bubbles",
                          "Show Search Bubbles",
                          "If search bubbles should be rendered.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_SEARCH_SHADOW] =
    g_param_spec_boolean ("show-search-shadow",
                          "Show Search Shadow",
                          "If the shadow should be drawn when performing searches.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SNIPPET_COMPLETION] =
    g_param_spec_boolean ("snippet-completion",
                          "Snippet Completion",
                          "If snippet expansion should be enabled via the completion window.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_OVERSCROLL] =
    g_param_spec_int ("overscroll",
                      "Overscroll",
                      "The number of lines to scroll beyond the end of the "
                      "buffer. A negative number of lines will scroll until "
                      "only that number of lines is visible",
                      G_MININT,
                      G_MAXINT,
                      DEFAULT_OVERSCROLL_NUM_LINES,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals [ACTION] =
    g_signal_new_class_handler ("action",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (dzl_gtk_widget_action_with_string),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                3,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING);

  signals [APPEND_TO_COUNT] =
    g_signal_new ("append-to-count",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, append_to_count),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  /**
   * IdeSourceView::begin-macro:
   *
   * This signal will begin recording input to the #IdeSourceView. This includes the current
   * #IdeSourceViewMode, #IdeSourceView:count and #IdeSourceView:modifier which will be used
   * to replay the sequence starting from the correct state.
   *
   * Pair this with an emission of #IdeSourceView::end-macro to complete the sequence.
   */
  signals [BEGIN_MACRO] =
    g_signal_new ("begin-macro",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, begin_macro),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::begin-rename:
   *
   * This signal is emitted when the source view should begin a rename
   * operation using the #IdeRenameProvider from the underlying buffer. The
   * cursor position will be used as the location when sending the request to
   * the provider.
   */
  signals [BEGIN_RENAME] =
    g_signal_new ("begin-rename",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, begin_rename),
                  NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [BEGIN_USER_ACTION] =
    g_signal_new_class_handler ("begin-user-action",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_begin_user_action),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  signals [SAVE_COMMAND] =
    g_signal_new ("save-command",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, save_command),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  signals [SAVE_SEARCH_CHAR] =
    g_signal_new ("save-search-char",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, save_search_char),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::capture-modifier:
   *
   * This signal will block the main loop in a similar fashion to how
   * gtk_dialog_run() performs until a key-press has occurred that can be
   * captured for use in movements.
   *
   * Pressing Escape or unfocusing the widget will break from this loop.
   *
   * Use of this signal is not recommended except in very specific cases.
   */
  signals [CAPTURE_MODIFIER] =
    g_signal_new ("capture-modifier",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, capture_modifier),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  g_signal_override_class_handler ("change-case",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_CALLBACK (ide_source_view_real_change_case));

  signals [CLEAR_COUNT] =
    g_signal_new ("clear-count",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_count),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [CLEAR_MODIFIER] =
    g_signal_new ("clear-modifier",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_modifier),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [CLEAR_SEARCH] =
    g_signal_new ("clear-search",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_search),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [CLEAR_SELECTION] =
    g_signal_new ("clear-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_selection),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [CLEAR_SNIPPETS] =
    g_signal_new ("clear-snippets",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, clear_snippets),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [CYCLE_COMPLETION] =
    g_signal_new ("cycle-completion",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, cycle_completion),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * IdeSourceView:documentation-requested:
   * @self: A #IdeSourceView
   * @word: the word that was requested
   *
   * This is emitted by the default request-documentation handler to
   * locate the documentation for the currently selected word.
   */
  signals [DOCUMENTATION_REQUESTED] =
    g_signal_new ("documentation-requested",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  signals [DECREASE_FONT_SIZE] =
    g_signal_new ("decrease-font-size",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, decrease_font_size),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [DELETE_SELECTION] =
    g_signal_new ("delete-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, delete_selection),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::end-macro:
   *
   * You should call #IdeSourceView::begin-macro before emitting this signal.
   *
   * Complete a macro recording sequence. This may be called more times than is necessary,
   * since #IdeSourceView will only keep the most recent macro recording. This can be
   * helpful when implementing recording sequences such as in Vim.
   */
  signals [END_MACRO] =
    g_signal_new ("end-macro",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, end_macro),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [END_USER_ACTION] =
    g_signal_new_class_handler ("end-user-action",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_end_user_action),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  signals [FIND_REFERENCES] =
    g_signal_new_class_handler ("find-references",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_real_find_references),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [FOCUS_LOCATION] =
    g_signal_new ("focus-location",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSourceViewClass, focus_location),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SOURCE_LOCATION);

  signals [FORMAT_SELECTION] =
    g_signal_new_class_handler ("format-selection",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_real_format_selection),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [GOTO_DEFINITION] =
    g_signal_new ("goto-definition",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, goto_definition),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [HIDE_COMPLETION] =
    g_signal_new ("hide-completion",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, hide_completion),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [INCREASE_FONT_SIZE] =
    g_signal_new ("increase-font-size",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, increase_font_size),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [INDENT_SELECTION] =
    g_signal_new ("indent-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, indent_selection),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_INT);

  /**
   * IdeSourceView::insert-modifier:
   * @self: An #IdeSourceView
   * @use_count: If the count property should be used to repeat.
   *
   * Inserts the current modifier character at the insert mark in the buffer.
   * If @use_count is %TRUE, then the character will be inserted
   * #IdeSourceView:count times.
   */
  signals [INSERT_MODIFIER] =
    g_signal_new ("insert-modifier",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, insert_modifier),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  g_signal_override_class_handler ("join-lines",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_CALLBACK (ide_source_view_real_join_lines));

  signals [JUMP] =
    g_signal_new ("jump",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeSourceViewClass, jump),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_ITER);

  signals [MOVEMENT] =
    g_signal_new ("movement",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, movement),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  4,
                  IDE_TYPE_SOURCE_VIEW_MOVEMENT,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  /**
   * IdeSourceView::move-error:
   * @self: An #IdeSourceView.
   * @dir: The direction to move.
   *
   * Moves to the next search result either forwards or backwards.
   */
  signals [MOVE_ERROR] =
    g_signal_new ("move-error",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, move_error),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  signals [MOVE_SEARCH] =
    g_signal_new ("move-search",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, move_search),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  6,
                  GTK_TYPE_DIRECTION_TYPE,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN,
                  G_TYPE_INT);

  signals [PASTE_CLIPBOARD_EXTENDED] =
    g_signal_new ("paste-clipboard-extended",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, paste_clipboard_extended),
                  NULL, NULL, NULL,
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
  signals [POP_SELECTION] =
    g_signal_new ("pop-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, pop_selection),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::pop-snippet:
   * @self: An #IdeSourceView
   * @snippet: An #IdeSourceSnippet.
   *
   * Pops the current snippet from the sourceview if there is one.
   */
  signals [POP_SNIPPET] =
    g_signal_new_class_handler ("pop-snippet",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  /**
   * IdeSourceView::push-selection:
   *
   * Saves the current selection away to be restored by a call to
   * IdeSourceView::pop-selection. You must pop the selection to keep
   * the selection stack in consistent order.
   */
  signals [PUSH_SELECTION] =
    g_signal_new ("push-selection",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, push_selection),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::push-snippet:
   * @self: An #IdeSourceView
   * @snippet: An #IdeSourceSnippet.
   * @iter: (allow-none): The location for the snippet, or %NULL.
   *
   * Pushes @snippet onto the snippet stack at either @iter or the insertion
   * mark if @iter is not provided.
   */
  signals [PUSH_SNIPPET] =
    g_signal_new_class_handler ("push-snippet",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_source_view_real_push_snippet),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                2,
                                IDE_TYPE_SOURCE_SNIPPET,
                                GTK_TYPE_TEXT_ITER);

  signals [REBUILD_HIGHLIGHT] =
    g_signal_new ("rebuild-highlight",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, rebuild_highlight),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [DUPLICATE_ENTIRE_LINE] =
    g_signal_new_class_handler ("duplicate-entire-line",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_real_duplicate_entire_line),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  signals [REINDENT] =
    g_signal_new_class_handler ("reindent",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_real_reindent),
                                NULL, NULL, NULL,
                                G_TYPE_NONE,
                                0);

  /**
   * IdeSourceView:replay-macro:
   * @self: an #IdeSourceView.
   *
   * Replays the last series of captured events that were captured between calls
   * to #IdeSourceView::begin-macro and #IdeSourceView::end-macro.
   */
  signals [REPLAY_MACRO] =
    g_signal_new ("replay-macro",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, replay_macro),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  signals [REQUEST_DOCUMENTATION] =
    g_signal_new ("request-documentation",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, request_documentation),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * IdeSourceView::reset:
   *
   * This is a helper signal that will try to reset keyboard input
   * and various stateful settings of the sourceview. This is a good
   * signal to map to the "Escape" key.
   *
   * Since: 3.26
   */
  signals [RESET] =
    g_signal_new_class_handler ("reset",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (ide_source_view_real_reset),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  signals [RESET_FONT_SIZE] =
    g_signal_new ("reset-font-size",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, reset_font_size),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [RESTORE_INSERT_MARK] =
    g_signal_new ("restore-insert-mark",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, restore_insert_mark),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [SAVE_INSERT_MARK] =
    g_signal_new ("save-insert-mark",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, save_insert_mark),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  g_signal_override_class_handler ("select-all",
                                   G_TYPE_FROM_CLASS (klass),
                                   G_CALLBACK (ide_source_view_real_select_all));

  signals [SELECT_INNER] =
    g_signal_new ("select-inner",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, select_inner),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  signals [SELECT_TAG] =
    g_signal_new ("select-tag",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, select_tag),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  signals [SELECTION_THEATRIC] =
    g_signal_new ("selection-theatric",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, selection_theatric),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_SOURCE_VIEW_THEATRIC);

  signals [SET_MODE] =
    g_signal_new ("set-mode",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_mode),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
                  IDE_TYPE_SOURCE_VIEW_MODE_TYPE);

  signals [SET_OVERWRITE] =
    g_signal_new ("set-overwrite",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_overwrite),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  signals [SET_SEARCH_TEXT] =
    g_signal_new ("set-search-text",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, set_search_text),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_STRING,
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
  signals [SORT] =
    g_signal_new ("sort",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, sort),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  signals [SWAP_SELECTION_BOUNDS] =
    g_signal_new ("swap-selection-bounds",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, swap_selection_bounds),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  signals [ADD_CURSOR] =
    g_signal_new ("add-cursor",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, add_cursor),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  IDE_TYPE_CURSOR_TYPE);

  signals [REMOVE_CURSORS] =
    g_signal_new ("remove-cursors",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (IdeSourceViewClass, remove_cursors),
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_r,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "begin-rename", 0);

  gtk_binding_entry_add_signal (binding_set,
                                GDK_KEY_space,
                                GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "find-references", 0);

  /*
   * Escape is wired up by the GtkSourceCompletion by default. However, some
   * keybindings may want to control that manually (such as Vim). Vim needs to
   * go back to normal mode upon Escape to more closely match the traditional
   * environment.
   *
   * We remove the Tab activation from the completion class so that we can
   * activate it ourselves. Otherwise, it might fire before we have a chance
   * to steal it to move to the next completion item.
   */
  completion_class = g_type_class_ref (GTK_SOURCE_TYPE_COMPLETION);
  binding_set = gtk_binding_set_by_class (completion_class);
  gtk_binding_entry_remove (binding_set, GDK_KEY_Escape, 0);
  gtk_binding_entry_remove (binding_set, GDK_KEY_Tab, 0);
  g_type_class_unref (completion_class);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceCompletion *completion;
  GtkTargetList *target_list;

  priv->include_regex = g_regex_new (INCLUDE_STATEMENTS,
                                     G_REGEX_OPTIMIZE,
                                     0,
                                     NULL);

  DZL_COUNTER_INC (instances);

  priv->target_line_column = 0;
  priv->snippets = g_queue_new ();
  priv->selections = g_queue_new ();
  priv->show_line_diagnostics = TRUE;
  priv->font_scale = FONT_SCALE_NORMAL;
  priv->search_direction = GTK_DIR_DOWN;
  priv->command_str = g_string_sized_new (32);
  priv->overscroll_num_lines = DEFAULT_OVERSCROLL_NUM_LINES;

  priv->completion_providers_signals = dzl_signal_group_new (IDE_TYPE_EXTENSION_SET_ADAPTER);

  dzl_signal_group_connect_object (priv->completion_providers_signals,
                                   "extension-added",
                                   G_CALLBACK (ide_source_view__completion_provider_added),
                                   self,
                                   0);

  dzl_signal_group_connect_object (priv->completion_providers_signals,
                                   "extension-removed",
                                   G_CALLBACK (ide_source_view__completion_provider_removed),
                                   self,
                                   0);

  priv->file_setting_bindings = dzl_binding_group_new ();
  dzl_binding_group_bind (priv->file_setting_bindings, "indent-width",
                          self, "indent-width", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "tab-width",
                          self, "tab-width", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "right-margin-position",
                          self, "right-margin-position", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "indent-style",
                          self, "indent-style", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "show-right-margin",
                          self, "show-right-margin", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "insert-matching-brace",
                          self, "insert-matching-brace", G_BINDING_SYNC_CREATE);
  dzl_binding_group_bind (priv->file_setting_bindings, "overwrite-braces",
                          self, "overwrite-braces", G_BINDING_SYNC_CREATE);

  priv->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "changed",
                                   G_CALLBACK (ide_source_view__buffer_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "line-flags-changed",
                                   G_CALLBACK (ide_source_view__buffer_line_flags_changed_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::can-redo",
                                   G_CALLBACK (ide_source_view__buffer__notify_can_redo),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::can-undo",
                                   G_CALLBACK (ide_source_view__buffer__notify_can_undo),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::highlight-diagnostics",
                                   G_CALLBACK (ide_source_view__buffer_notify_highlight_diagnostics_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::file",
                                   G_CALLBACK (ide_source_view__buffer_notify_file_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::language",
                                   G_CALLBACK (ide_source_view__buffer_notify_language_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::style-scheme",
                                   G_CALLBACK (ide_source_view__buffer_notify_style_scheme_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "insert-text",
                                   G_CALLBACK (ide_source_view__buffer_insert_text_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "insert-text",
                                   G_CALLBACK (ide_source_view__buffer_insert_text_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "delete-range",
                                   G_CALLBACK (ide_source_view__buffer_delete_range_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "delete-range",
                                   G_CALLBACK (ide_source_view__buffer_delete_range_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "mark-set",
                                   G_CALLBACK (ide_source_view__buffer_mark_set_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "loaded",
                                   G_CALLBACK (ide_source_view__buffer_loaded_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (priv->buffer_signals,
                                   "notify::has-selection",
                                   G_CALLBACK (ide_source_view__buffer_notify_has_selection_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->buffer_signals,
                           "bind",
                           G_CALLBACK (ide_source_view_bind_buffer),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->buffer_signals,
                           "unbind",
                           G_CALLBACK (ide_source_view_unbind_buffer),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property_full (self, "buffer", priv->buffer_signals, "target", 0,
                               ignore_invalid_buffers, NULL, NULL, NULL);

  /*
   * We block completion when we are not focused so that two SourceViews
   * viewing the same GtkTextBuffer do not both show completion
   * windows.
   */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self));
  gtk_source_completion_block_interactive (completion);

  /*
   * Drag and drop support
   */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (self));
  if (target_list)
    gtk_target_list_add_uri_targets (target_list, TARGET_URI_LIST);

  dzl_widget_action_group_attach (self, "sourceview");
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

      priv->font_scale = FONT_SCALE_NORMAL;

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
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_CHANGES]);
    }
}

gboolean
ide_source_view_get_show_line_diagnostics (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_line_diagnostics;
}

void
ide_source_view_set_show_line_diagnostics (IdeSourceView *self,
                                           gboolean       show_line_diagnostics)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_line_diagnostics = !!show_line_diagnostics;

  if (show_line_diagnostics != priv->show_line_diagnostics)
    {
      gboolean visible;

      priv->show_line_diagnostics = show_line_diagnostics;

      if ((priv->buffer != NULL) && (priv->line_diagnostics_renderer != NULL))
        {
          visible = (priv->show_line_diagnostics &&
                     ide_buffer_get_highlight_diagnostics (priv->buffer));
          gtk_source_gutter_renderer_set_visible (priv->line_diagnostics_renderer, visible);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_LINE_CHANGES]);
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
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_GRID_LINES]);
    }
}

gboolean
ide_source_view_get_insert_matching_brace (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->insert_matching_brace;
}

void
ide_source_view_get_iter_at_visual_column (IdeSourceView *self,
                                           guint column,
                                           GtkTextIter *location)
{
  gunichar tab_char;
  guint visual_col = 0;
  guint tab_width;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  tab_char = g_utf8_get_char ("\t");
  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self));
  gtk_text_iter_set_line_offset (location, 0);

  while (!gtk_text_iter_ends_line (location))
    {
      if (gtk_text_iter_get_char (location) == tab_char)
        visual_col += (tab_width - (visual_col % tab_width));
      else
        ++visual_col;

      if (visual_col > column)
        break;

      /* FIXME: this does not handle invisible text correctly, but
       *       * gtk_text_iter_forward_visible_cursor_position is too
       *       slow */
      if (!gtk_text_iter_forward_char (location))
        break;
    }
}

const gchar *
ide_source_view_get_mode_name (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  if (priv->mode)
    return ide_source_view_mode_get_name (priv->mode);

  return NULL;
}

const gchar *
ide_source_view_get_mode_display_name (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->display_name;
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
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INSERT_MATCHING_BRACE]);
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
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OVERWRITE_BRACES]);
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
      g_signal_emit (self, signals [POP_SNIPPET], 0, snippet);
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

/**
 * ide_source_view_push_snippet:
 * @self: An #IdeSourceView
 * @snippet: An #IdeSourceSnippet.
 * @location: (allow-none): A location for the snippet or %NULL.
 *
 * Pushes a new snippet onto the source view.
 */
void
ide_source_view_push_snippet (IdeSourceView     *self,
                              IdeSourceSnippet  *snippet,
                              const GtkTextIter *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  IdeSourceSnippetContext *context;
  IdeSourceSnippet *previous;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gboolean has_more_tab_stops;
  gboolean insert_spaces;
  gchar *line_prefix;
  guint tab_width;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (IDE_IS_SOURCE_SNIPPET (snippet));
  g_return_if_fail (!location ||
                    (gtk_text_iter_get_buffer (location) == (void*)priv->buffer));

  if ((previous = g_queue_peek_head (priv->snippets)))
    ide_source_snippet_pause (previous);

  g_queue_push_head (priv->snippets, g_object_ref (snippet));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (location != NULL)
    iter = *location;
  else
    gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));

  context = ide_source_snippet_get_context (snippet);

  insert_spaces = gtk_source_view_get_insert_spaces_instead_of_tabs (GTK_SOURCE_VIEW (self));
  ide_source_snippet_context_set_use_spaces (context, insert_spaces);

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self));
  ide_source_snippet_context_set_tab_width (context, tab_width);

  line_prefix = text_iter_get_line_prefix (&iter);
  ide_source_snippet_context_set_line_prefix (context, line_prefix);
  g_free (line_prefix);

  g_signal_emit (self, signals [PUSH_SNIPPET], 0, snippet, &iter);

  gtk_text_buffer_begin_user_action (buffer);
  ide_source_view_block_handlers (self);
  has_more_tab_stops = ide_source_snippet_begin (snippet, buffer, &iter);
  ide_source_view_scroll_to_insert (self);
  ide_source_view_unblock_handlers (self);
  gtk_text_buffer_end_user_action (buffer);

  if (!ide_source_view_can_animate (self))
    {
      GtkTextMark *mark_begin;
      GtkTextMark *mark_end;

      mark_begin = ide_source_snippet_get_mark_begin (snippet);
      mark_end = ide_source_snippet_get_mark_end (snippet);

      if (mark_begin != NULL && mark_end != NULL)
        {
          GtkTextIter begin;
          GtkTextIter end;

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

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SNIPPET_COMPLETION]);
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
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BACK_FORWARD_LIST]);
}

void
ide_source_view_jump (IdeSourceView     *self,
                      const GtkTextIter *location)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextIter iter;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (location == NULL)
    {
      GtkTextMark *mark;

      mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (priv->buffer));
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (priv->buffer), &iter, mark);
      location = &iter;
    }

  if (priv->buffer && !_ide_buffer_get_loading (priv->buffer))
    g_signal_emit (self, signals [JUMP], 0, location);

  IDE_EXIT;
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
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SCROLL_OFFSET]);
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
      scroll_offset = MIN ((gint)priv->scroll_offset, max_scroll_offset);
      scroll_offset_height = priv->cached_char_height * scroll_offset;

      area.y += scroll_offset_height;
      area.height -= (2 * scroll_offset_height);

      /*
       * If we have an even number of visible lines and scrolloffset is less than our
       * desired scrolloffset, we need to remove an extra line so we don't have two
       * visible lines.
       */
      if ((scroll_offset < (gint)priv->scroll_offset) && (visible_lines & 1) == 0)
        area.height -= priv->cached_char_height;

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
                                      GtkTextMark   *mark,
                                      gboolean       use_align,
                                      gdouble        alignx,
                                      gdouble        aligny)
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
    ide_source_view_scroll_to_mark (self, mark, 0.0, use_align, alignx, aligny, TRUE);

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

static gboolean
ide_source_view_mark_is_onscreen (IdeSourceView *self,
                                  GtkTextMark   *mark)
{
  GtkTextBuffer *buffer;
  GdkRectangle visible_rect;
  GdkRectangle mark_rect;
  GtkTextIter iter;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_TEXT_MARK (mark));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ide_source_view_get_visible_rect (self, &visible_rect);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &mark_rect);

  return (_GDK_RECTANGLE_CONTAINS (&visible_rect, &mark_rect));
}

static void
ide_source_view__vadj_animation_completed (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  /*
   * If the mark we were scrolling to is not yet on screen, then just wait for another size
   * allocate so that we can continue making progress.
   */
  if (!ide_source_view_mark_is_onscreen (self, priv->scroll_mark))
    IDE_EXIT;

  priv->scrolling_to_scroll_mark = FALSE;

  IDE_EXIT;
}

/*
 * Many parts of this function were taken from gtk_text_view_scroll_to_iter ()
 * https://developer.gnome.org/gtk3/stable/GtkTextView.html#gtk-text-view-scroll-to-iter
 */
void
ide_source_view_scroll_to_iter (IdeSourceView     *self,
                                const GtkTextIter *iter,
                                gdouble            within_margin,
                                gboolean           use_align,
                                gdouble            xalign,
                                gdouble            yalign,
                                gboolean           animate_scroll)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextView *text_view = (GtkTextView *)self;
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GdkRectangle screen;
  gint xvalue = 0;
  gint yvalue = 0;
  gint scroll_dest;
  gint screen_bottom;
  gint screen_right;
  gint screen_xoffset;
  gint screen_yoffset;
  gint current_x_scroll;
  gint current_y_scroll;
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (within_margin >= 0.0 && within_margin <= 0.5);
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  if (!ide_source_view_can_animate (self))
    animate_scroll = FALSE;

  buffer = gtk_text_view_get_buffer (text_view);
  gtk_text_buffer_move_mark (buffer, priv->scroll_mark, iter);

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (self));
  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self));

  gtk_text_view_get_iter_location (text_view,
                                   iter,
                                   &rect);

  gtk_text_view_get_visible_rect (text_view, &screen);

  current_x_scroll = screen.x;
  current_y_scroll = screen.y;

  screen_xoffset = screen.width * within_margin;
  screen_yoffset = screen.height * within_margin;

  screen.x += screen_xoffset;
  screen.y += screen_yoffset;
  screen.width -= screen_xoffset * 2;
  screen.height -= screen_yoffset * 2;


  /* paranoia check */
  if (screen.width < 1)
    screen.width = 1;
  if (screen.height < 1)
    screen.height = 1;

  /* The -1 here ensures that we leave enough space to draw the cursor
   * when this function is used for horizontal scrolling.
   */
  screen_right = screen.x + screen.width - 1;
  screen_bottom = screen.y + screen.height;


  /* The alignment affects the point in the target character that we
   * choose to align. If we're doing right/bottom alignment, we align
   * the right/bottom edge of the character the mark is at; if we're
   * doing left/top we align the left/top edge of the character; if
   * we're doing center alignment we align the center of the
   * character.
   */

  /* Vertical alignment */
  scroll_dest = current_y_scroll;
  if (use_align)
    {
      scroll_dest = rect.y + (rect.height * yalign) - (screen.height * yalign);

      /* if scroll_dest < screen.y, we move a negative increment (up),
       * else a positive increment (down)
       */
      yvalue = scroll_dest - screen.y + screen_yoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.y < screen.y)
        {
          scroll_dest = rect.y;
          yvalue = scroll_dest - screen.y - screen_yoffset;
        }
      else if ((rect.y + rect.height) > screen_bottom)
        {
          scroll_dest = rect.y + rect.height;
          yvalue = scroll_dest - screen_bottom + screen_yoffset;
        }
    }
  yvalue += current_y_scroll;

  /* Scroll offset adjustment */
  if (priv->cached_char_height)
    {
      gint max_scroll_offset;
      gint visible_lines;
      gint scroll_offset;
      gint scroll_offset_height;

      visible_lines = screen.height / priv->cached_char_height;
      max_scroll_offset = (visible_lines - 1) / 2;
      scroll_offset = MIN ((gint)priv->scroll_offset, max_scroll_offset);
      scroll_offset_height = priv->cached_char_height * scroll_offset;

      if (scroll_offset_height > 0)
        {
          if (rect.y - scroll_offset_height < yvalue)
            yvalue -= (scroll_offset_height - (rect.y - yvalue));
          else if (_GDK_RECTANGLE_Y2 (&rect) + scroll_offset_height > yvalue + screen.height)
            yvalue += (_GDK_RECTANGLE_Y2 (&rect) + scroll_offset_height) - (yvalue + screen.height);
        }
    }

  /* Horizontal alignment */
  scroll_dest = current_x_scroll;
  if (use_align)
    {
      scroll_dest = rect.x + (rect.width * xalign) - (screen.width * xalign);

      /* if scroll_dest < screen.y, we move a negative increment (left),
       * else a positive increment (right)
       */
      xvalue = scroll_dest - screen.x + screen_xoffset;
    }
  else
    {
      /* move minimum to get onscreen */
      if (rect.x < screen.x)
        {
          scroll_dest = rect.x;
          xvalue = scroll_dest - screen.x - screen_xoffset;
        }
      else if ((rect.x + rect.width) > screen_right)
        {
          scroll_dest = rect.x + rect.width;
          xvalue = scroll_dest - screen_right + screen_xoffset;
        }
    }
  xvalue += current_x_scroll;

  if (animate_scroll)
    {
      GdkFrameClock *frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self));
      guint duration_msec = LARGE_SCROLL_DURATION_MSEC;
      gdouble difference;
      gdouble page_size;
      gdouble current;

      current = gtk_adjustment_get_value (vadj);
      page_size = gtk_adjustment_get_page_size (vadj);
      difference = ABS (current - yvalue);

      /*
       * Ignore animations if we are scrolling less than two full lines.  This
       * helps when pressing up/down for key repeat.  Also, if it's a partial
       * page scroll (less than page size), use less time to animate, so it
       * isn't so annoying.
       */
      if (difference < (priv->cached_char_height * 2))
        goto ignore_animation;
      else if (difference <= page_size)
        duration_msec = SMALL_SCROLL_DURATION_MSEC;

      priv->scrolling_to_scroll_mark = TRUE;

      if (priv->hadj_animation != NULL)
        {
          dzl_animation_stop (priv->hadj_animation);
          ide_clear_weak_pointer (&priv->hadj_animation);
        }

      priv->hadj_animation =
        dzl_object_animate (hadj,
                            DZL_ANIMATION_EASE_OUT_CUBIC,
                            duration_msec,
                            frame_clock,
                            "value", (double)xvalue,
                            NULL);
      g_object_add_weak_pointer (G_OBJECT (priv->hadj_animation),
                                 (gpointer *)&priv->hadj_animation);

      if (priv->vadj_animation != NULL)
        {
          dzl_animation_stop (priv->vadj_animation);
          ide_clear_weak_pointer (&priv->vadj_animation);
        }

      priv->vadj_animation =
        dzl_object_animate_full (vadj,
                                 DZL_ANIMATION_EASE_OUT_CUBIC,
                                 duration_msec,
                                 frame_clock,
                                 (GDestroyNotify)ide_source_view__vadj_animation_completed,
                                 self,
                                 "value", (double)yvalue,
                                 NULL);
      g_object_add_weak_pointer (G_OBJECT (priv->vadj_animation),
                                 (gpointer *)&priv->vadj_animation);
    }
  else
    {
ignore_animation:
      gtk_adjustment_set_value (hadj, xvalue);
      gtk_adjustment_set_value (vadj, yvalue);
    }

  IDE_EXIT;
}

void
ide_source_view_scroll_to_mark (IdeSourceView *self,
                                GtkTextMark   *mark,
                                gdouble        within_margin,
                                gboolean       use_align,
                                gdouble        xalign,
                                gdouble        yalign,
                                gboolean       animate_scroll)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (xalign >= 0.0);
  g_return_if_fail (xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0);
  g_return_if_fail (yalign <= 1.0);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  ide_source_view_scroll_to_iter (self, &iter, within_margin, use_align, xalign, yalign,
                                  animate_scroll);

#ifdef IDE_ENABLE_TRACE
  {
    const gchar *name = gtk_text_mark_get_name (mark);
    IDE_TRACE_MSG ("Scrolling to mark \"%s\" at %d:%d",
                   name ? name : "unnamed",
                   gtk_text_iter_get_line (&iter),
                   gtk_text_iter_get_line_offset (&iter));
  }
#endif

  IDE_EXIT;
}

gboolean
ide_source_view_place_cursor_onscreen (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  insert = gtk_text_buffer_get_insert (buffer);

  ret = ide_source_view_move_mark_onscreen (self, insert);

  IDE_RETURN (ret);
}

gboolean
ide_source_view_get_enable_word_completion (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->enable_word_completion;
}

void
ide_source_view_set_enable_word_completion (IdeSourceView *self,
                                            gboolean       enable_word_completion)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  enable_word_completion = !!enable_word_completion;

  if (priv->enable_word_completion != enable_word_completion)
    {
      priv->enable_word_completion = enable_word_completion;
      ide_source_view_reload_word_completion (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ENABLE_WORD_COMPLETION]);
    }
}

/**
 * ide_source_view_get_search_context:
 * @self: An #IdeSourceView.
 *
 * Returns the #GtkSourceSearchContext for the source view if there is one.
 *
 * Returns: (transfer none) (nullable): A #GtkSourceSearchContext or %NULL.
 */
GtkSourceSearchContext *
ide_source_view_get_search_context (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->search_context;
}

/**
 * ide_source_view_get_search_direction:
 * @self: An #IdeSourceView.
 *
 * Gets the current search direction.
 *
 * Returns: A #GtkDirectionType
 */
GtkDirectionType
ide_source_view_get_search_direction (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), GTK_DIR_DOWN);

  return priv->search_direction;
}

/**
 * ide_source_view_set_search_direction:
 * @self: An #IdeSourceView.
 * @direction: the direction
 *
 * Sets the search direction.
 *
 * This can be used to invert the normal search direction so that a forward
 * movement is towards the beginning of the document.
 */
void
ide_source_view_set_search_direction (IdeSourceView    *self,
                                      GtkDirectionType  direction)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (direction != GTK_DIR_TAB_BACKWARD && direction != GTK_DIR_TAB_FORWARD);

  if (direction != priv->search_direction)
    {
      priv->search_direction = direction;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SEARCH_DIRECTION]);
    }
}

/**
 * ide_source_view_get_show_search_bubbles:
 * @self: An #IdeSourceView.
 *
 * Gets the #IdeSourceView:show-search-bubbles property.
 *
 * If this is set to %TRUE, a bubble will be drawn around search results to
 * make them stand out.
 *
 * The default is %FALSE.
 */
gboolean
ide_source_view_get_show_search_bubbles (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_search_bubbles;
}

void
ide_source_view_set_show_search_bubbles (IdeSourceView *self,
                                         gboolean       show_search_bubbles)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_search_bubbles = !!show_search_bubbles;

  if (show_search_bubbles != priv->show_search_bubbles)
    {
      priv->show_search_bubbles = show_search_bubbles;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_SEARCH_BUBBLES]);
      ide_source_view_invalidate_window (self);
    }
}

/**
 * ide_source_view_get_show_search_shadow:
 * @self: An #IdeSourceView.
 *
 * Gets the #IdeSourceView:show-search-shadow property.
 *
 * If this property is %TRUE, then when searching, a shadow will be drawn over
 * the portion of the visible region that does not contain a match. This can
 * be used to help bring focus to the matches.
 *
 * The default is %FALSE.
 */
gboolean
ide_source_view_get_show_search_shadow (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->show_search_shadow;
}

void
ide_source_view_set_show_search_shadow (IdeSourceView *self,
                                        gboolean       show_search_shadow)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  show_search_shadow = !!show_search_shadow;

  if (show_search_shadow != priv->show_search_shadow)
    {
      priv->show_search_shadow = show_search_shadow;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_SEARCH_SHADOW]);
      ide_source_view_invalidate_window (self);
    }
}

/**
 * ide_source_view_get_file_settings:
 * @self: A #IdeSourceView.
 *
 * Gets the #IdeSourceView:file-settings property. This contains various
 * settings for how the file should be rendered in the view, and preferences
 * such as spaces vs tabs.
 *
 * Returns: (transfer none) (nullable): An #IdeFileSettings or %NULL.
 */
IdeFileSettings *
ide_source_view_get_file_settings (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return (IdeFileSettings *)dzl_binding_group_get_source (priv->file_setting_bindings);
}

gboolean
ide_source_view_get_highlight_current_line (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->highlight_current_line;
}

void
ide_source_view_set_highlight_current_line (IdeSourceView *self,
                                            gboolean       highlight_current_line)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  /*
   * This overrides the default GtkSourceView::highlight-current-line so that
   * we can turn off the line highlight when the IdeSourceView is not in focus.
   * See ide_source_view_real_focus_in_event() and
   * ide_source_view_real_focus_out_event() for the machinery.
   */

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  highlight_current_line = !!highlight_current_line;

  if (highlight_current_line != priv->highlight_current_line)
    {
      priv->highlight_current_line = highlight_current_line;
      g_object_notify (G_OBJECT (self), "highlight-current-line");
    }
}

guint
ide_source_view_get_visual_column (IdeSourceView *self,
                                   const GtkTextIter *location)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), 0);

  return gtk_source_view_get_visual_column(GTK_SOURCE_VIEW (self), location);
}

void
ide_source_view_get_visual_position (IdeSourceView *self,
                                     guint         *line,
                                     guint         *line_column)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  if (!gtk_widget_has_focus (GTK_WIDGET (self)))
    {
      gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, priv->saved_line, 0);
      ide_source_view_get_iter_at_visual_column (self, priv->saved_line_column, &iter);
    }
  else
    {
      GtkTextMark *mark;

      mark = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
    }

  if (line)
    *line = gtk_text_iter_get_line (&iter);

  if (line_column)
    *line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self), &iter);
}

void
ide_source_view_clear_search (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);
  GtkSourceSearchSettings *search_settings;
  const gchar *search_text;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  search_settings = gtk_source_search_context_get_settings (priv->search_context);
  search_text = gtk_source_search_settings_get_search_text (search_settings);

  if ((search_text != NULL) &&
      (search_text [0] != 0) &&
      (0 != g_strcmp0 (priv->saved_search_text, search_text)))
    {
      g_free (priv->saved_search_text);
      priv->saved_search_text = g_strdup (search_text);
    }

  gtk_source_search_settings_set_search_text (search_settings, "");
}

gint
ide_source_view_get_count (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), 0);

  return priv->count;
}

void
ide_source_view_set_count (IdeSourceView *self,
                           gint           count)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (count < 0)
    count = 0;

  if (count != priv->count)
    {
      priv->count = count;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_COUNT]);
    }
}

gboolean
ide_source_view_get_rubberband_search (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return priv->rubberband_search;
}

void
ide_source_view_set_rubberband_search (IdeSourceView *self,
                                       gboolean       rubberband_search)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  rubberband_search = !!rubberband_search;

  if (rubberband_search != priv->rubberband_search)
    {
      priv->rubberband_search = rubberband_search;

      if (priv->rubberband_search && (priv->rubberband_mark != NULL))
        {
          GtkTextBuffer *buffer;
          GtkTextMark *insert;
          GtkTextIter iter;
          GdkRectangle rect;

          /*
           * The rubberband_mark is the top-left position of the sourceview
           * currently (for the beginning of the search). We use this so that
           * we can restore the sourceview vadjustment to the proper position
           * when rubberbanding back to the original position. The
           * rubberband_insert_mark is the position after the current insert
           * mark so that we will begin incremental searches after the current
           * cursor.
           */

          buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
          insert = gtk_text_buffer_get_insert (buffer);

          gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self), &rect);
          gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self), &iter, rect.x+1, rect.y+1);
          gtk_text_buffer_move_mark (buffer, priv->rubberband_mark, &iter);

          gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
          gtk_text_iter_forward_char (&iter);
          gtk_text_buffer_move_mark (buffer, priv->rubberband_insert_mark, &iter);
        }

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_RUBBERBAND_SEARCH]);
    }
}

void
ide_source_view_rollback_search (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  ide_source_view_scroll_mark_onscreen (self, priv->rubberband_mark, TRUE, 0.5, 0.5);
}

GtkTextMark *
_ide_source_view_get_scroll_mark (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return priv->scroll_mark;
}

/**
 * ide_source_view_get_current_snippet:
 *
 * Gets the current snippet if there is one, otherwise %NULL.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceSnippet or %NULL.
 */
IdeSourceSnippet *
ide_source_view_get_current_snippet (IdeSourceView *self)
{
  IdeSourceViewPrivate *priv = ide_source_view_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return g_queue_peek_head (priv->snippets);
}
