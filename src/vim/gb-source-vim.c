/* gb-source-vim.c
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

#define G_LOG_DOMAIN "vim"
#define SCROLL_OFF 3

#include <errno.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>

#include "gb-source-vim.h"

#ifndef GB_SOURCE_VIM_EXTERNAL
# include "gb-source-view.h"
#endif

/*
 *  I can't possibly know all of VIM features. So this doesn't implement
 *  all of them. Just the main ones I know about. File bugs if you like.
 *
 * TODO:
 *
 *  - Registers
 *  - Multi-character verb/noun/modifiers.
 *  - Marks
 *  - Jumps
 */

/**
 * GbSourceVimCommandFunc:
 * @vim: The #GbSourceVim instance.
 * @count: The number modifier for the command.
 * @modifier: A potential trailing modifer character.
 *
 * This is a function prototype for commands to implement themselves. They
 * can potentially use the count to perform the operation multiple times.
 *
 * However, not all commands support this or will use it.
 */
typedef void (*GbSourceVimCommandFunc) (GbSourceVim        *vim,
                                        guint               count,
                                        gchar               modifier);

/**
 * GbSourceVimOperation:
 * @command_text: text command to execute.
 *
 * This is a function declaration for functions that can process an operation.
 * Operations are things that are entered into the command mode entry.
 *
 * Unfortunately, we already have a command abstraction that should possibly
 * be renamed. But such is life!
 */
typedef void (*GbSourceVimOperation) (GbSourceVim *vim,
                                      const gchar *command_text);

struct _GbSourceVimPrivate
{
  GtkTextView             *text_view;
  GString                 *phrase;
  GtkTextMark             *selection_anchor_begin;
  GtkTextMark             *selection_anchor_end;
  GtkSourceSearchContext  *search_context;
  GtkSourceSearchSettings *search_settings;
  GbSourceVimMode          mode;
  gulong                   key_press_event_handler;
  gulong                   focus_in_event_handler;
  gulong                   mark_set_handler;
  gulong                   delete_range_handler;
  guint                    target_line_offset;
  guint                    stash_line;
  guint                    stash_line_offset;
  guint                    anim_timeout;
  guint                    enabled : 1;
  guint                    connected : 1;
};

typedef enum
{
  GB_SOURCE_VIM_PAGE_UP,
  GB_SOURCE_VIM_PAGE_DOWN,
  GB_SOURCE_VIM_HALF_PAGE_UP,
  GB_SOURCE_VIM_HALF_PAGE_DOWN,
} GbSourceVimPageDirectionType;

typedef enum
{
  GB_SOURCE_VIM_COMMAND_NOOP,
  GB_SOURCE_VIM_COMMAND_MOVEMENT,
  GB_SOURCE_VIM_COMMAND_CHANGE,
  GB_SOURCE_VIM_COMMAND_JUMP,
} GbSourceVimCommandType;

typedef enum
{
  GB_SOURCE_VIM_COMMAND_FLAG_NONE,
  GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER = 1 << 0,
  GB_SOURCE_VIM_COMMAND_FLAG_VISUAL            = 1 << 1,
  GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE  = 1 << 2,
  GB_SOURCE_VIM_COMMAND_FLAG_MOTION_LINEWISE   = 1 << 3,
} GbSourceVimCommandFlags;

/**
 * GbSourceVimCommand:
 *
 * This structure encapsulates what we need to know about a command before
 * we can dispatch it. GB_EDiTOR_VIM_COMMAND_FLAG_REQUIRES_MODIFIER in flags
 * means there needs to be a supplimental character provided after the key.
 * Such an example would be "dd", "dw", "yy", or "gg".
 */
typedef struct
{
  GbSourceVimCommandFunc  func;
  GbSourceVimCommandType  type;
  gchar                   key;
  GbSourceVimCommandFlags flags;
} GbSourceVimCommand;

typedef enum
{
  GB_SOURCE_VIM_PHRASE_FAILED,
  GB_SOURCE_VIM_PHRASE_SUCCESS,
  GB_SOURCE_VIM_PHRASE_NEED_MORE,
} GbSourceVimPhraseStatus;

typedef struct
{
  guint count;
  gchar key;
  gchar modifier;
} GbSourceVimPhrase;

typedef struct
{
  gunichar jump_to;
  gunichar jump_from;
  guint    depth;
} MatchingBracketState;

enum
{
  PROP_0,
  PROP_ENABLED,
  PROP_MODE,
  PROP_PHRASE,
  PROP_TEXT_VIEW,
  LAST_PROP
};

enum
{
  BEGIN_SEARCH,
  COMMAND_VISIBILITY_TOGGLED,
  JUMP_TO_DOC,
  LAST_SIGNAL
};

enum
{
  CLASS_0,
  CLASS_SPACE,
  CLASS_SPECIAL,
  CLASS_WORD,
};

G_DEFINE_TYPE_WITH_PRIVATE (GbSourceVim, gb_source_vim, G_TYPE_OBJECT)

static GHashTable *gCommands;
static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void text_iter_swap (GtkTextIter *a,
                            GtkTextIter *b);
static void gb_source_vim_select_range (GbSourceVim *vim,
                                        GtkTextIter *insert_iter,
                                        GtkTextIter *selection_iter);
static void gb_source_vim_cmd_select_line (GbSourceVim *vim,
                                           guint        count,
                                           gchar        modifier);
static void gb_source_vim_cmd_delete (GbSourceVim *vim,
                                      guint        count,
                                      gchar        modifier);
static void gb_source_vim_cmd_delete_to_end (GbSourceVim *vim,
                                             guint        count,
                                             gchar        modifier);
static void gb_source_vim_cmd_insert_before_line (GbSourceVim *vim,
                                                  guint        count,
                                                  gchar        modifier);

GbSourceVim *
gb_source_vim_new (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return g_object_new (GB_TYPE_SOURCE_VIM,
                       "text-view", text_view,
                       NULL);
}

static int
gb_source_vim_classify (gunichar ch)
{
  switch (ch)
    {
    case ' ':
    case '\t':
    case '\n':
      return CLASS_SPACE;

    case '"': case '\'':
    case '(': case ')':
    case '{': case '}':
    case '[': case ']':
    case '<': case '>':
    case '-': case '+': case '*': case '/':
    case '!': case '@': case '#': case '$': case '%':
    case '^': case '&': case ':': case ';': case '?':
    case '|': case '=': case '\\': case '.': case ',':
      return CLASS_SPECIAL;

    case '_':
    default:
      return CLASS_WORD;
    }
}

static guint
gb_source_vim_get_line_offset (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  return gtk_text_iter_get_line_offset (&iter);
}

static void
gb_source_vim_save_position (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &iter, &selection);

  vim->priv->stash_line = gtk_text_iter_get_line (&iter);
  vim->priv->stash_line_offset = gtk_text_iter_get_line_offset (&iter);
}

static void
gb_source_vim_restore_position (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  guint offset;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_iter_at_line (buffer, &iter, vim->priv->stash_line);

  for (offset = vim->priv->stash_line_offset; offset; offset--)
    if (!gtk_text_iter_forward_char (&iter))
      break;

  gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static void
gb_source_vim_set_selection_anchor (GbSourceVim       *vim,
                                    const GtkTextIter *begin,
                                    const GtkTextIter *end)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextIter left_anchor;
  GtkTextIter right_anchor;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (begin);
  g_assert (end);

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  if (gtk_text_iter_compare (begin, end) < 0)
    {
      gtk_text_iter_assign (&left_anchor, begin);
      gtk_text_iter_assign (&right_anchor, end);
    }
  else
    {
      gtk_text_iter_assign (&left_anchor, end);
      gtk_text_iter_assign (&right_anchor, begin);
    }

  if (!priv->selection_anchor_begin)
    priv->selection_anchor_begin =
      gtk_text_buffer_create_mark (buffer,
                                   "selection-anchor-begin",
                                   &left_anchor,
                                   TRUE);
  else
    gtk_text_buffer_move_mark (buffer,
                               priv->selection_anchor_begin,
                               &left_anchor);

  if (!priv->selection_anchor_end)
    priv->selection_anchor_end =
      gtk_text_buffer_create_mark (buffer,
                                   "selection-anchor-end",
                                   &right_anchor,
                                   FALSE);
  else
    gtk_text_buffer_move_mark (buffer,
                               priv->selection_anchor_end,
                               &right_anchor);
}

static void
gb_source_vim_ensure_anchor_selected (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *selection_mark;
  GtkTextMark *insert_mark;
  GtkTextIter anchor_begin;
  GtkTextIter anchor_end;
  GtkTextIter insert_iter;
  GtkTextIter selection_iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  if (!priv->selection_anchor_begin || !priv->selection_anchor_end)
    return;

  buffer = gtk_text_view_get_buffer (priv->text_view);

  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_begin,
                                    priv->selection_anchor_begin);
  gtk_text_buffer_get_iter_at_mark (buffer, &anchor_end,
                                    priv->selection_anchor_end);

  insert_mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter, insert_mark);

  selection_mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &selection_iter, selection_mark);

  if ((gtk_text_iter_compare (&selection_iter, &anchor_end) < 0) &&
      (gtk_text_iter_compare (&insert_iter, &anchor_end) < 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        gb_source_vim_select_range (vim, &insert_iter, &anchor_end);
      else
        gb_source_vim_select_range (vim, &anchor_end, &selection_iter);
    }
  else if ((gtk_text_iter_compare (&selection_iter, &anchor_begin) > 0) &&
           (gtk_text_iter_compare (&insert_iter, &anchor_begin) > 0))
    {
      if (gtk_text_iter_compare (&insert_iter, &selection_iter) < 0)
        gb_source_vim_select_range (vim, &anchor_begin, &selection_iter);
      else
        gb_source_vim_select_range (vim, &insert_iter, &anchor_begin);
    }
}

static void
gb_source_vim_clear_selection (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

  if (priv->selection_anchor_begin)
    {
      GtkTextMark *mark;

      mark = priv->selection_anchor_begin;
      priv->selection_anchor_begin = NULL;

      gtk_text_buffer_delete_mark (buffer, mark);
    }

  if (priv->selection_anchor_end)
    {
      GtkTextMark *mark;

      mark = priv->selection_anchor_end;
      priv->selection_anchor_end = NULL;

      gtk_text_buffer_delete_mark (buffer, mark);
    }

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

GbSourceVimMode
gb_source_vim_get_mode (GbSourceVim *vim)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), 0);

  return vim->priv->mode;
}

const gchar *
gb_source_vim_get_phrase (GbSourceVim *vim)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), NULL);

  return vim->priv->phrase->str;
}

static void
gb_source_vim_clear_phrase (GbSourceVim *vim)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  g_string_truncate (vim->priv->phrase, 0);
  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_PHRASE]);
}

void
gb_source_vim_set_mode (GbSourceVim     *vim,
                        GbSourceVimMode  mode)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  /*
   * Ignore if we are already in this mode.
   */
  if (mode == vim->priv->mode)
    return;

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  /*
   * If we are starting insert mode, let's try to coalesce all changes
   * into one undo stack item like VIM.
   */
  if (mode == GB_SOURCE_VIM_INSERT)
    gtk_text_buffer_begin_user_action (buffer);
  else if (vim->priv->mode == GB_SOURCE_VIM_INSERT)
    gtk_text_buffer_end_user_action (buffer);

  vim->priv->mode = mode;

  /*
   * Switch to the "block mode" cursor for non-insert mode. We are totally
   * abusing "overwrite" here simply to look more like VIM.
   */
  gtk_text_view_set_overwrite (vim->priv->text_view,
                               (mode != GB_SOURCE_VIM_INSERT));

  /*
   * Clear any in flight phrases.
   */
  gb_source_vim_clear_phrase (vim);

  /*
   * If we are going back to navigation mode, stash our current buffer
   * position for use in commands like j and k.
   */
  if (mode == GB_SOURCE_VIM_NORMAL)
    vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  /*
   * Clear the current selection too.
   */
  if (mode != GB_SOURCE_VIM_COMMAND)
    gb_source_vim_clear_selection (vim);

  /*
   * Make the command entry visible if necessary.
   */
  g_signal_emit (vim, gSignals [COMMAND_VISIBILITY_TOGGLED], 0,
                 (mode == GB_SOURCE_VIM_COMMAND));

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_MODE]);
}

static void
gb_source_vim_maybe_auto_indent (GbSourceVim *vim)
{
#ifndef GB_SOURCE_VIM_EXTERNAL
  GbSourceAutoIndenter *auto_indenter;
  GbSourceVimPrivate *priv;
  GbSourceView *source_view;
  GdkEvent fake_event = { 0 };

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  if (!GB_IS_SOURCE_VIEW (priv->text_view))
    return;

  source_view = GB_SOURCE_VIEW (priv->text_view);

  auto_indenter = gb_source_view_get_auto_indenter (source_view);
  if (!auto_indenter)
    return;

  fake_event.key.type = GDK_KEY_PRESS;
  fake_event.key.window = gtk_text_view_get_window (priv->text_view,
                                                    GTK_TEXT_WINDOW_TEXT);
  fake_event.key.send_event = FALSE;
  fake_event.key.time = GDK_CURRENT_TIME;
  fake_event.key.state = 0;
  fake_event.key.keyval = GDK_KEY_Return;
  fake_event.key.length = 1;
  fake_event.key.string = (char *)"";
  fake_event.key.hardware_keycode = 0;
  fake_event.key.group = 0;
  fake_event.key.is_modifier = 0;

  if (gb_source_auto_indenter_is_trigger (auto_indenter, &fake_event.key))
    {
      GtkTextBuffer *buffer;
      GtkTextMark *insert;
      GtkTextIter begin;
      GtkTextIter end;
      gint cursor_offset = 0;
      gchar *indent;

      buffer = gtk_text_view_get_buffer (priv->text_view);
      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
      gtk_text_iter_assign (&end, &begin);

      indent = gb_source_auto_indenter_format (auto_indenter, priv->text_view,
                                               buffer, &begin, &end,
                                               &cursor_offset, &fake_event.key);

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
           * Place the cursor, as it could be somewhere within our indent text.
           */
          gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
          if (cursor_offset > 0)
            gtk_text_iter_forward_chars (&begin, cursor_offset);
          else if (cursor_offset < 0)
            gtk_text_iter_backward_chars (&begin, ABS (cursor_offset));
          gtk_text_buffer_select_range (buffer, &begin, &begin);
        }

      g_free (indent);
    }
#endif
}

static gboolean
gb_source_vim_get_selection_bounds (GbSourceVim *vim,
                                    GtkTextIter *insert_iter,
                                    GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  if (insert_iter)
    gtk_text_buffer_get_iter_at_mark (buffer, insert_iter, insert);

  if (selection_iter)
    gtk_text_buffer_get_iter_at_mark (buffer, selection_iter, selection);

  return gtk_text_buffer_get_has_selection (buffer);
}

static void
gb_source_vim_select_range (GbSourceVim *vim,
                            GtkTextIter *insert_iter,
                            GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;
  gint insert_off;
  gint selection_off;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (insert_iter);
  g_assert (selection_iter);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  /*
   * If the caller is requesting that we select a single character, we will
   * keep the iter before that character. This more closely matches the visual
   * mode in VIM.
   */
  insert_off = gtk_text_iter_get_offset (insert_iter);
  selection_off = gtk_text_iter_get_offset (selection_iter);
  if ((insert_off - selection_off) == 1)
    text_iter_swap (insert_iter, selection_iter);

  gtk_text_buffer_move_mark (buffer, insert, insert_iter);
  gtk_text_buffer_move_mark (buffer, selection, selection_iter);
}

static void
gb_source_vim_move_line0 (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (vim->priv->text_view));
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  gtk_text_iter_set_line_offset (&iter, 0);

  if (has_selection)
    {
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static void
gb_source_vim_move_line_start (GbSourceVim *vim,
                               gboolean     can_move_forward)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter original;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (vim->priv->text_view));
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);
  gtk_text_iter_assign (&original, &iter);

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  while (!gtk_text_iter_ends_line (&iter) &&
         g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    if (!gtk_text_iter_forward_char (&iter))
      break;

  /*
   * If we failed to find a non-whitespace character or ended up at the
   * same place we already were, just use the 0 index position.
   */
  if (!can_move_forward)
    {
      if (g_unichar_isspace (gtk_text_iter_get_char (&iter)) ||
          gtk_text_iter_equal (&iter, &original))
        {
          gb_source_vim_move_line0 (vim);
          return;
        }
    }

  if (has_selection)
    {
      if (gtk_text_iter_compare (&iter, &selection) > 0)
        gtk_text_iter_forward_char (&iter);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static void
gb_source_vim_move_line_end (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  while (!gtk_text_iter_ends_line (&iter))
    if (!gtk_text_iter_forward_char (&iter))
      break;

  if (has_selection)
    {
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_backward (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);

  if (gtk_text_iter_backward_char (&iter) &&
      (line == gtk_text_iter_get_line (&iter)))
    {
      if (has_selection)
        {
          if (gtk_text_iter_equal (&iter, &selection))
            {
              gtk_text_iter_backward_char (&iter);
              gtk_text_iter_forward_char (&selection);
            }
          gb_source_vim_select_range (vim, &iter, &selection);
          gb_source_vim_ensure_anchor_selected (vim);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);

      vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
    }
}

static gboolean
text_iter_backward_vim_word (GtkTextIter *iter)
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  if (!gtk_text_iter_backward_char (iter))
    return FALSE;

  /*
   * If we are on space, walk until we get to non-whitespace. Then work our way
   * back to the beginning of the word.
   */
  ch = gtk_text_iter_get_char (iter);
  if (gb_source_vim_classify (ch) == CLASS_SPACE)
    {
      for (;;)
        {
          if (!gtk_text_iter_backward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          if (gb_source_vim_classify (ch) != CLASS_SPACE)
            break;
        }

      ch = gtk_text_iter_get_char (iter);
      begin_class = gb_source_vim_classify (ch);

      for (;;)
        {
          if (!gtk_text_iter_backward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          cur_class = gb_source_vim_classify (ch);

          if (cur_class != begin_class)
            {
              gtk_text_iter_forward_char (iter);
              return TRUE;
            }
        }

      return FALSE;
    }

  ch = gtk_text_iter_get_char (iter);
  begin_class = gb_source_vim_classify (ch);

  for (;;)
    {
      if (!gtk_text_iter_backward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = gb_source_vim_classify (ch);

      if (cur_class != begin_class)
        {
          gtk_text_iter_forward_char (iter);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gb_source_vim_move_backward_word (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  GtkTextMark *insert;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (!text_iter_backward_vim_word (&iter))
    gtk_text_buffer_get_start_iter (buffer, &iter);

  if (has_selection)
    {
      if (gtk_text_iter_equal (&iter, &selection))
        gtk_text_iter_backward_word_start (&iter);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_forward (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);

  if (!gtk_text_iter_forward_char (&iter))
    gtk_text_buffer_get_end_iter (buffer, &iter);

  if (line == gtk_text_iter_get_line (&iter))
    {
      if (has_selection)
        {
          if (gtk_text_iter_equal (&iter, &selection))
            {
              gtk_text_iter_forward_char (&iter);
              gtk_text_iter_backward_char (&selection);
              gb_source_vim_ensure_anchor_selected (vim);
            }
          gb_source_vim_select_range (vim, &iter, &selection);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);

      vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
    }
}

static gboolean
text_iter_forward_vim_word (GtkTextIter *iter)
{
  gint begin_class;
  gint cur_class;
  gunichar ch;

  g_assert (iter);

  ch = gtk_text_iter_get_char (iter);
  begin_class = gb_source_vim_classify (ch);

  /* Move to the first non-whitespace character if necessary. */
  if (begin_class == CLASS_SPACE)
    {
      for (;;)
        {
          if (!gtk_text_iter_forward_char (iter))
            return FALSE;

          ch = gtk_text_iter_get_char (iter);
          cur_class = gb_source_vim_classify (ch);
          if (cur_class != CLASS_SPACE)
            return TRUE;
        }
    }

  /* move to first character not at same class level. */
  while (gtk_text_iter_forward_char (iter))
    {
      ch = gtk_text_iter_get_char (iter);
      cur_class = gb_source_vim_classify (ch);

      if (cur_class == CLASS_SPACE)
        {
          begin_class = CLASS_0;
          continue;
        }

      if (cur_class != begin_class)
        return TRUE;
    }

  return FALSE;
}

static gboolean
text_iter_forward_vim_word_end (GtkTextIter *iter)
{
  gunichar ch;
  gint begin_class;
  gint cur_class;

  g_assert (iter);

  if (!gtk_text_iter_forward_char (iter))
    return FALSE;

  /* If we are on space, walk to the start of the next word. */
  ch = gtk_text_iter_get_char (iter);
  if (gb_source_vim_classify (ch) == CLASS_SPACE)
    if (!text_iter_forward_vim_word (iter))
      return FALSE;

  ch = gtk_text_iter_get_char (iter);
  begin_class = gb_source_vim_classify (ch);

  for (;;)
    {
      if (!gtk_text_iter_forward_char (iter))
        return FALSE;

      ch = gtk_text_iter_get_char (iter);
      cur_class = gb_source_vim_classify (ch);

      if (cur_class != begin_class)
        {
          gtk_text_iter_backward_char (iter);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gb_source_vim_move_forward_word (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  /*
   * TODO: VIM will jump to an empty line before going to the next word.
   */

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (!text_iter_forward_vim_word (&iter))
    gtk_text_buffer_get_end_iter (buffer, &iter);

  if (has_selection)
    {
      if (!gtk_text_iter_forward_char (&iter))
        gtk_text_buffer_get_end_iter (buffer, &iter);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_forward_word_end (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (!text_iter_forward_vim_word_end (&iter))
    gtk_text_buffer_get_end_iter (buffer, &iter);

  if (has_selection)
    {
      if (!gtk_text_iter_forward_char (&iter))
        gtk_text_buffer_get_end_iter (buffer, &iter);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static gboolean
bracket_predicate (gunichar ch,
                   gpointer user_data)
{
  MatchingBracketState *state = user_data;

  if (ch == state->jump_from)
    state->depth++;
  else if (ch == state->jump_to)
    state->depth--;

  return (state->depth == 0);
}

static void
gb_source_vim_move_matching_bracket (GbSourceVim *vim)
{
  MatchingBracketState state;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  gboolean is_forward;
  gboolean ret;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  state.depth = 1;
  state.jump_from = gtk_text_iter_get_char (&iter);

  switch (state.jump_from)
    {
    case '{':
      state.jump_to = '}';
      is_forward = TRUE;
      break;

    case '[':
      state.jump_to = ']';
      is_forward = TRUE;
      break;

    case '(':
      state.jump_to = ')';
      is_forward = TRUE;
      break;

    case '}':
      state.jump_to = '{';
      is_forward = FALSE;
      break;

    case ']':
      state.jump_to = '[';
      is_forward = FALSE;
      break;

    case ')':
      state.jump_to = '(';
      is_forward = FALSE;
      break;

    default:
      return;
    }

  if (is_forward)
    ret = gtk_text_iter_forward_find_char (&iter, bracket_predicate, &state,
                                           NULL);
  else
    ret = gtk_text_iter_backward_find_char (&iter, bracket_predicate, &state,
                                            NULL);

  if (ret)
    {
      if (has_selection)
        {
          gb_source_vim_select_range (vim, &iter, &selection);
          gb_source_vim_ensure_anchor_selected (vim);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);

      insert = gtk_text_buffer_get_insert (buffer);
      gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
    }
}

static gboolean
is_single_line_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  if (gtk_text_iter_compare (begin, end) < 0)
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (begin) + 1) ==
             gtk_text_iter_get_line (end)));
  else
    return ((gtk_text_iter_get_line_offset (begin) == 0) &&
            (gtk_text_iter_get_line_offset (end) == 0) &&
            ((gtk_text_iter_get_line (end) + 1) ==
             gtk_text_iter_get_line (begin)));
}

static gboolean
is_single_char_selection (const GtkTextIter *begin,
                          const GtkTextIter *end)
{
  GtkTextIter tmp;

  g_assert (begin);
  g_assert (end);

  gtk_text_iter_assign (&tmp, begin);
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, end))
    return TRUE;

  gtk_text_iter_assign (&tmp, end);
  if (gtk_text_iter_forward_char (&tmp) && gtk_text_iter_equal (&tmp, begin))
    return TRUE;

  return FALSE;
}

static void
text_iter_swap (GtkTextIter *a,
                GtkTextIter *b)
{
  GtkTextIter tmp;

  gtk_text_iter_assign (&tmp, a);
  gtk_text_iter_assign (a, b);
  gtk_text_iter_assign (b, &tmp);
}

static void
gb_source_vim_move_forward_paragraph (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter, selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  /* Move down to the first non-blank line */
  while (gtk_text_iter_starts_line (&iter) &&
         gtk_text_iter_ends_line (&iter))
    if (!gtk_text_iter_forward_line (&iter))
      break;

  /* Find the next blank line */
  while (gtk_text_iter_forward_line (&iter))
    if (gtk_text_iter_starts_line (&iter) &&
        gtk_text_iter_ends_line (&iter))
      break;

  if (has_selection)
    {
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_backward_paragraph (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter, selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  /* Move up to the first non-blank line */
  while (gtk_text_iter_starts_line (&iter) &&
         gtk_text_iter_ends_line (&iter))
    if (!gtk_text_iter_backward_line (&iter))
      break;

  /* Find the next blank line */
  while (gtk_text_iter_backward_line (&iter))
    if (gtk_text_iter_starts_line (&iter) &&
        gtk_text_iter_ends_line (&iter))
      break;

  if (has_selection)
    {
      if (gtk_text_iter_equal (&iter, &selection))
        gtk_text_iter_forward_char (&selection);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_down (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;
  guint offset;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);
  offset = vim->priv->target_line_offset;

  /*
   * If we have a whole line selected (from say `V`), then we need to swap
   * the cursor and selection. This feels to me like a slight bit of a hack.
   * There may be cause to actually have a selection mode and know the type
   * of selection (line vs individual characters).
   */
  if (is_single_line_selection (&iter, &selection))
    {
      guint target_line;

      if (gtk_text_iter_compare (&iter, &selection) < 0)
        text_iter_swap (&iter, &selection);

      target_line = gtk_text_iter_get_line (&iter) + 1;
      gtk_text_iter_set_line (&iter, target_line);

      if (target_line != gtk_text_iter_get_line (&iter))
        goto select_to_end;

      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
      goto move_mark;
    }

  if (is_single_char_selection (&iter, &selection))
    {
      if (gtk_text_iter_compare (&iter, &selection) < 0)
        priv->target_line_offset = ++offset;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line + 1);
  if ((line + 1) == gtk_text_iter_get_line (&iter))
    {
      for (; offset; offset--)
        if (!gtk_text_iter_ends_line (&iter))
          if (!gtk_text_iter_forward_char (&iter))
            break;
      if (has_selection)
        {
          gb_source_vim_select_range (vim, &iter, &selection);
          gb_source_vim_ensure_anchor_selected (vim);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);
    }
  else
    {
select_to_end:
      gtk_text_buffer_get_end_iter (buffer, &iter);
      if (has_selection)
        {
          gb_source_vim_select_range (vim, &iter, &selection);
          gb_source_vim_ensure_anchor_selected (vim);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

move_mark:
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_move_up (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;
  guint offset;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);
  offset = vim->priv->target_line_offset;

  if (line == 0)
    return;

  if (is_single_line_selection (&iter, &selection))
    {
      if (gtk_text_iter_compare (&iter, &selection) > 0)
        text_iter_swap (&iter, &selection);
      gtk_text_iter_set_line (&iter, gtk_text_iter_get_line (&iter) - 1);
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
      goto move_mark;
    }

  if (is_single_char_selection (&iter, &selection))
    {
      if (gtk_text_iter_compare (&iter, &selection) > 0)
        priv->target_line_offset = --offset;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line - 1);
  if ((line - 1) == gtk_text_iter_get_line (&iter))
    {
      for (; offset; offset--)
        if (!gtk_text_iter_ends_line (&iter))
          if (!gtk_text_iter_forward_char (&iter))
            break;

      if (has_selection)
        {
          if (gtk_text_iter_equal (&iter, &selection))
            gtk_text_iter_backward_char (&iter);
          gb_source_vim_select_range (vim, &iter, &selection);
          gb_source_vim_ensure_anchor_selected (vim);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

move_mark:
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_toggle_case (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter cur;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_selection;
  gboolean place_at_end = FALSE;
  GString *str;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) > 0)
    {
      text_iter_swap (&begin, &end);
      place_at_end = TRUE;
    }

  if (!has_selection)
    {
      if (!gtk_text_iter_forward_char (&end))
        return;
    }

  str = g_string_new (NULL);

  gtk_text_iter_assign (&cur, &begin);

  while (gtk_text_iter_compare (&cur, &end) < 0)
    {
      gunichar ch;

      ch = gtk_text_iter_get_char (&cur);

      if (g_unichar_isupper (ch))
        g_string_append_unichar (str, g_unichar_tolower (ch));
      else
        g_string_append_unichar (str, g_unichar_toupper (ch));

      if (!gtk_text_iter_forward_char (&cur))
        break;
    }

  if (!str->len)
    goto cleanup;

  gtk_text_buffer_begin_user_action (buffer);

  gb_source_vim_save_position (vim);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, str->str, str->len);
  gb_source_vim_restore_position (vim);

  if (!has_selection)
    gb_source_vim_select_range (vim, &begin, &begin);
  else if (place_at_end)
    {
      if (gtk_text_iter_backward_char (&begin))
        gb_source_vim_select_range (vim, &begin, &begin);
    }

  gtk_text_buffer_end_user_action (buffer);

cleanup:
  g_string_free (str, TRUE);
}

static void
gb_source_vim_delete_selection (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkClipboard *clipboard;
  gchar *text;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  /*
   * If there is no selection to delete, try to remove the next character
   * in the line. If there is no next character, delete the last character
   * in the line. It might look like there is no selection if the line
   * was empty.
   */
  if (gtk_text_iter_equal (&begin, &end))
    {
      if (gtk_text_iter_starts_line (&begin) &&
          gtk_text_iter_ends_line (&end) &&
          (0 == gtk_text_iter_get_line_offset (&end)))
        return;
      else if (!gtk_text_iter_ends_line (&end))
        {
          if (!gtk_text_iter_forward_char (&end))
            gtk_text_buffer_get_end_iter (buffer, &end);
        }
      else if (!gtk_text_iter_starts_line (&begin))
        {
          if (!gtk_text_iter_backward_char (&begin))
            return;
        }
      else
        return;
    }

  /*
   * Yank the selection text and apply it to the clipboard.
   */
  text = gtk_text_iter_get_slice (&begin, &end);
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (vim->priv->text_view),
                                        GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);
  g_free (text);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_end_user_action (buffer);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_select_line (GbSourceVim *vim)
{
  GbSourceVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  /*
   * Move to the start iter to the beginning of the line.
   */
  gtk_text_iter_assign (&begin, &iter);
  while (!gtk_text_iter_starts_line (&begin))
    if (!gtk_text_iter_backward_char (&begin))
      break;

  /*
   * Move to the end cursor to the end of the line.
   */
  gtk_text_iter_assign (&end, &iter);
  while (!gtk_text_iter_ends_line (&end))
    {
      if (!gtk_text_iter_forward_char (&end))
        {
          /*
           * This is the last line in the buffer, so we need to select the
           * newline before the line instead of the newline after the line.
           */
          gtk_text_iter_backward_char (&begin);
          break;
        }
    }

  /*
   * We actually want to select the \n before the line.
   */
  if (gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_char (&end);

  gtk_text_buffer_select_range (buffer, &begin, &end);

  gb_source_vim_set_selection_anchor (vim, &begin, &end);

  vim->priv->target_line_offset = 0;
}

static void
gb_source_vim_select_char (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  GtkTextIter *target;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);
  target = has_selection ? &iter : &selection;

  if (!gtk_text_iter_forward_char (target))
    gtk_text_buffer_get_end_iter (buffer, target);

  gb_source_vim_select_range (vim, &iter, &selection);
  gb_source_vim_set_selection_anchor (vim, &iter, &selection);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_apply_motion (GbSourceVim *vim,
                            char         motion,
                            guint        count)
{
  GbSourceVimCommand *cmd;

  cmd = g_hash_table_lookup (gCommands, GINT_TO_POINTER (motion));
  if (!cmd || (cmd->type != GB_SOURCE_VIM_COMMAND_MOVEMENT))
    return;

  if ((cmd->flags & GB_SOURCE_VIM_COMMAND_FLAG_MOTION_LINEWISE))
    gb_source_vim_select_line (vim);
  else
    gb_source_vim_select_char (vim);

  cmd->func (vim, count, '\0');

  if ((cmd->flags & GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE))
    {
      GtkTextIter iter, selection;

      gb_source_vim_get_selection_bounds (vim, &iter, &selection);
      if (gtk_text_iter_compare (&iter, &selection) < 0)
        text_iter_swap (&iter, &selection);

      /* From the docs:
       * "If the motion is exclusive and the end of the motion is in column 1,
       *  the end of the motion is moved to the end of the previous line and
       *  the motion becomes inclusive."
       */
      if (gtk_text_iter_get_line_offset (&iter) == 0)
        {
          GtkTextIter tmp;
          guint line;

          gtk_text_iter_backward_char (&iter);

          /* More docs:
           * "If [as above] and the start of the motion was at or before
           *  the first non-blank in the line, the motion becomes linewise."
           */
           tmp = selection;
           line = gtk_text_iter_get_line (&selection);

           gtk_text_iter_backward_word_start (&tmp);
           if (gtk_text_iter_is_start (&tmp) ||
               gtk_text_iter_get_line (&tmp) < line)
             {
               while (!gtk_text_iter_starts_line (&selection))
                 gtk_text_iter_backward_char (&selection);
               while (!gtk_text_iter_starts_line (&iter))
                 gtk_text_iter_forward_char (&iter);
             }
        }
      else
        {
          gtk_text_iter_backward_char (&iter);
        }
      gb_source_vim_select_range (vim, &iter, &selection);
    }
}

static void
gb_source_vim_undo (GbSourceVim *vim)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  /*
   * We only support GtkSourceView for now.
   */
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  undo = gtk_source_buffer_get_undo_manager (GTK_SOURCE_BUFFER (buffer));
  if (gtk_source_undo_manager_can_undo (undo))
    gtk_source_undo_manager_undo (undo);

  /*
   * GtkSourceView might preserve the selection. So let's go ahead and
   * clear it manually to the selection-bound mark position.
   */
  if (gb_source_vim_get_selection_bounds (vim, NULL, &iter))
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_redo (GbSourceVim *vim)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  /*
   * We only support GtkSourceView for now.
   */
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  undo = gtk_source_buffer_get_undo_manager (GTK_SOURCE_BUFFER (buffer));
  if (gtk_source_undo_manager_can_redo (undo))
    gtk_source_undo_manager_redo (undo);

  /*
   * GtkSourceView might preserve the selection. So let's go ahead and
   * clear it manually to the insert mark position.
   */
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_join (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  GString *str;
  gchar **parts;
  gchar *slice;
  guint i;
  guint offset;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (!has_selection)
    {
      guint line;

      /*
       * If there is no selection, we move the selection position to the end
       * of the following line.
       */
      line = gtk_text_iter_get_line (&iter) + 1;
      gtk_text_buffer_get_iter_at_line (buffer, &selection, line);
      if (gtk_text_iter_get_line (&selection) != line)
        return;

      while (!gtk_text_iter_ends_line (&selection))
        if (!gtk_text_iter_forward_char (&selection))
          break;
    }
  else if (gtk_text_iter_compare (&iter, &selection) > 0)
    text_iter_swap (&iter, &selection);

  offset = gtk_text_iter_get_offset (&iter);

  slice = gtk_text_iter_get_slice (&iter, &selection);
  parts = g_strsplit (slice, "\n", -1);
  str = g_string_new (NULL);

  for (i = 0; parts [i]; i++)
    {
      g_strstrip (parts [i]);
      if (*parts [i])
        {
          if (str->len)
            g_string_append (str, " ");
          g_string_append (str, parts [i]);
        }
    }

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &iter, &selection);
  gtk_text_buffer_insert (buffer, &iter, str->str, str->len);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_buffer_end_user_action (buffer);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);

  g_strfreev (parts);
  g_free (slice);
  g_string_free (str, TRUE);
}

static void
gb_source_vim_insert_nl_before (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint line;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  line = gtk_text_iter_get_line (&iter);

  /*
   * Insert a newline before the current line.
   */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  gtk_text_buffer_insert (buffer, &iter, "\n", 1);

  /*
   * Move ourselves back to the line we were one.
   */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);

  /*
   * Select this position as the cursor.
   */
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  /*
   * We might need to auto-indent the cursor after the newline.
   */
  gb_source_vim_maybe_auto_indent (vim);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_insert_nl_after (GbSourceVim *vim,
                               gboolean     auto_indent)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  /*
   * Move to the end of the current line and insert a newline.
   */
  while (!gtk_text_iter_ends_line (&iter))
    if (!gtk_text_iter_forward_char (&iter))
      break;
  gtk_text_buffer_insert (buffer, &iter, "\n", 1);

  /*
   * Select this position as the cursor to update insert.
   */
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  /*
   * We might need to auto-indent after the newline.
   */
  if (auto_indent)
    gb_source_vim_maybe_auto_indent (vim);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_source_vim_delete_to_line_start (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  /*
   * Clear any selection so we are left at the cursor position.
   */
  gb_source_vim_clear_selection (vim);

  /*
   * Get everything we need to determine the deletion region.
   */
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
  gtk_text_iter_assign (&end, &begin);

  /*
   * Move backward to the start of the line. VIM actually moves back to the
   * first non-whitespace character at the beginning of the line rather
   * than just position 0.
   *
   * If we are at the start of a line already, we actually just want to
   * remove the \n.
   */
  if (!gtk_text_iter_starts_line (&begin))
    {
      gb_source_vim_move_line_start (vim, FALSE);

      gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);

      if (gtk_text_iter_compare (&begin, &end) > 0)
        {
          while (!gtk_text_iter_starts_line (&begin))
            if (!gtk_text_iter_backward_char (&begin))
              break;
        }
    }
  else
    gtk_text_iter_backward_char (&begin);

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_end_user_action (buffer);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static void
gb_source_vim_paste (GbSourceVim *vim)
{
  GtkClipboard *clipboard;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint line;
  guint offset;
  gchar *text;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  /*
   * Track the current insert location so we can jump back to it.
   */
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  line = gtk_text_iter_get_line (&iter);
  offset = gtk_text_iter_get_line_offset (&iter);

  gtk_text_buffer_begin_user_action (buffer);

  /*
   * Fetch the clipboard contents so we can check to see if we are pasting a
   * whole line (which needs to be treated differently).
   */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (vim->priv->text_view),
                                        GDK_SELECTION_CLIPBOARD);
  text = gtk_clipboard_wait_for_text (clipboard);

  /*
   * If we are pasting an entire line, we don't want to paste it at the current
   * location. We want to insert a new line after the current line, and then
   * paste it there (so move the insert mark first).
   */
  if (text && g_str_has_suffix (text, "\n"))
    {
      const gchar *tmp;
      gchar *trimmed;

      /*
       * WORKAROUND:
       *
       * This is a hack so that we can continue to use the paste code from
       * within GtkTextBuffer.
       *
       * We needed to keep the trailing \n in the text so that we know when
       * we are selecting whole lines. We also need to insert a new line
       * manually based on the context. Furthermore, we need to remove the
       * trailing line since we already added one.
       *
       * Terribly annoying, but the result is something that feels very nice,
       * just like VIM.
       */

      trimmed = g_strndup (text, strlen (text) - 1);
      gb_source_vim_insert_nl_after (vim, FALSE);
      gtk_clipboard_set_text (clipboard, trimmed, -1);
      g_signal_emit_by_name (vim->priv->text_view, "paste-clipboard");
      gtk_clipboard_set_text (clipboard, text, -1);
      g_free (trimmed);

      /*
       * VIM leaves us on the first non-whitespace character.
       */
      offset = 0;
      for (tmp = text; *tmp; tmp = g_utf8_next_char (tmp))
        {
          gunichar ch;

          ch = g_utf8_get_char (tmp);
          if (g_unichar_isspace (ch))
            {
              offset++;
              continue;
            }
          break;
        }

      line++;
    }
  else
    {
      GtkTextIter tmp;
      GtkTextIter tmp2;

      /*
       * By default, GtkTextBuffer will paste at our current position.
       * While VIM will paste after the current position. Let's advance the
       * buffer a single character on the current line if possible. We switch
       * to insert mode so that we can move past the last character in the
       * buffer. Possibly should consider an alternate design for this.
       */
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
      gb_source_vim_move_forward (vim);
      g_signal_emit_by_name (vim->priv->text_view, "paste-clipboard");
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);

      gtk_text_buffer_get_selection_bounds (buffer, &tmp, &tmp2);
      offset = gtk_text_iter_get_line_offset (&tmp);
      if (offset)
        offset--;
    }

  gtk_text_buffer_end_user_action (buffer);

  /*
   * Restore the cursor position.
   */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  for (; offset; offset--)
    if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
      break;
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  g_free (text);
}

static void
gb_source_vim_move_to_end (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (has_selection)
    gb_source_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static void
gb_source_vim_yank (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkClipboard *clipboard;
  gchar *text;

  g_assert (GB_IS_SOURCE_VIM (vim));

  /*
   * Get the current textview selection.
   */
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  /*
   * Copy the selected text.
   */
  text = gtk_text_iter_get_slice (&begin, &end);

  /*
   * We might need to synthesize a trailing \n if this is at the end of the
   * buffer and we are performing a full line selection.
   */
  if (GTK_SOURCE_IS_BUFFER (buffer))
    {
      GtkSourceBuffer *sb = GTK_SOURCE_BUFFER (buffer);
      GtkTextIter line_start;
      GtkTextIter eob;

      gtk_text_buffer_get_end_iter (buffer, &eob);
      gtk_text_buffer_get_iter_at_line (buffer, &line_start,
                                        gtk_text_iter_get_line (&end));

      if (gtk_source_buffer_get_implicit_trailing_newline (sb) &&
          gtk_text_iter_equal (&eob, &end) &&
          (gtk_text_iter_compare (&begin, &line_start) <= 0))
        {
          gchar *tmp = text;

          text = g_strdup_printf ("%s\n", tmp);
          g_free (tmp);
        }
    }

  /*
   * Update the selection clipboard.
   */
  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (vim->priv->text_view),
                                        GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);
  g_free (text);

  /*
   * Move the cursor to the first character that was selected.
   */
  gtk_text_buffer_select_range (buffer, &begin, &begin);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);
}

static gboolean
gb_source_vim_select_current_word (GbSourceVim *vim,
                                   GtkTextIter *begin,
                                   GtkTextIter *end)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;

  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);
  g_return_val_if_fail (begin, FALSE);
  g_return_val_if_fail (end, FALSE);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, begin, insert);

  if (text_iter_forward_vim_word_end (begin))
    {
      gtk_text_iter_assign (end, begin);
      gtk_text_iter_forward_char (end);
      if (text_iter_backward_vim_word (begin))
        return TRUE;
    }

  return FALSE;
}

static void
gb_source_vim_search_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GtkSourceSearchContext *search_context = (GtkSourceSearchContext *)source;
  GbSourceVim *vim = user_data;
  GtkTextIter match_begin;
  GtkTextIter match_end;

  g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (gtk_source_search_context_backward_finish (search_context, result,
                                                 &match_begin, &match_end,
                                                 NULL))
    {
      if (vim->priv->text_view)
        {
          GtkTextBuffer *buffer;

          buffer = gtk_text_view_get_buffer (vim->priv->text_view);
          gtk_text_buffer_select_range (buffer, &match_begin, &match_begin);
          gtk_text_view_scroll_to_iter (vim->priv->text_view, &match_begin,
                                        0.0, TRUE, 0.0, 0.5);
        }
    }

  g_object_unref (vim);
}

static void
gb_source_vim_reverse_search (GbSourceVim *vim)
{
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (!GTK_SOURCE_IS_VIEW (vim->priv->text_view))
    return;

  if (gb_source_vim_select_current_word (vim, &begin, &end))
    {
      GtkTextIter start_iter;
      gchar *text;

      text = gtk_text_iter_get_slice (&begin, &end);

      if (gtk_text_iter_compare (&begin, &end) <= 0)
        gtk_text_iter_assign (&start_iter, &begin);
      else
        gtk_text_iter_assign (&start_iter, &end);

      g_object_set (vim->priv->search_settings,
                    "at-word-boundaries", TRUE,
                    "case-sensitive", TRUE,
                    "search-text", text,
                    "wrap-around", TRUE,
                    NULL);

      gtk_source_search_context_set_highlight (vim->priv->search_context,
                                               TRUE);

      gtk_source_search_context_backward_async (vim->priv->search_context,
                                                &start_iter,
                                                NULL,
                                                gb_source_vim_search_cb,
                                                g_object_ref (vim));

      g_free (text);
    }
}

static void
gb_source_vim_search (GbSourceVim *vim)
{
  GtkTextIter iter;
  GtkTextIter selection;
  GtkTextIter start_iter;
  gboolean has_selection;
  gchar *text = NULL;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (!GTK_SOURCE_IS_VIEW (vim->priv->text_view))
    return;

  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (has_selection)
    text = gtk_text_iter_get_slice (&iter, &selection);
  else if (gb_source_vim_select_current_word (vim, &iter, &selection))
    text = gtk_text_iter_get_slice (&iter, &selection);
  else
    return;

  if (gtk_text_iter_compare (&iter, &selection) > 0)
    gtk_text_iter_assign (&start_iter, &iter);
  else
    gtk_text_iter_assign (&start_iter, &selection);

  g_object_set (vim->priv->search_settings,
                "at-word-boundaries", TRUE,
                "case-sensitive", TRUE,
                "search-text", text,
                "wrap-around", TRUE,
                NULL);

  gtk_source_search_context_set_highlight (vim->priv->search_context,
                                           TRUE);

  gtk_source_search_context_forward_async (vim->priv->search_context,
                                           &start_iter,
                                           NULL,
                                           gb_source_vim_search_cb,
                                           g_object_ref (vim));

  g_free (text);
}

static void
gb_source_vim_move_to_line_n (GbSourceVim *vim,
                              guint        line)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter, selection;
  gboolean has_selection;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  if (is_single_line_selection (&iter, &selection))
    {
      gtk_text_iter_set_line (&iter, line);

      if (gtk_text_iter_compare (&iter, &selection) > 0)
        gtk_text_iter_forward_line (&iter);
    }
  else
    gtk_text_iter_set_line (&iter, line);

  if (has_selection)
    {
      gb_source_vim_select_range (vim, &iter, &selection);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    {
      gb_source_vim_select_range (vim, &iter, &iter);
    }

  vim->priv->target_line_offset = gb_source_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static gboolean
reshow_highlight (gpointer data)
{
  GbSourceVim *vim = data;
  GtkSourceView *source_view;

  vim->priv->anim_timeout = 0;

  source_view = GTK_SOURCE_VIEW (vim->priv->text_view);
  gtk_source_view_set_highlight_current_line (source_view, TRUE);

  return G_SOURCE_REMOVE;
}

static void
gb_source_vim_move_to_iter (GbSourceVim *vim,
                            GtkTextIter *iter,
                            gdouble      yalign)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (iter);
  g_assert (yalign >= 0.0);
  g_assert (yalign <= 1.0);

  if (GTK_SOURCE_IS_VIEW (vim->priv->text_view))
    {
      GtkSourceView *source_view;

      source_view = GTK_SOURCE_VIEW (vim->priv->text_view);
      if (vim->priv->anim_timeout ||
          gtk_source_view_get_highlight_current_line (source_view))
        {
          if (vim->priv->anim_timeout)
            g_source_remove (vim->priv->anim_timeout);
          gtk_source_view_set_highlight_current_line (source_view, FALSE);
          vim->priv->anim_timeout = g_timeout_add (200, reshow_highlight, vim);
        }
    }

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_move_mark (buffer, insert, iter);
      gb_source_vim_ensure_anchor_selected (vim);
    }
  else
    gtk_text_buffer_select_range (buffer, iter, iter);

  gtk_text_view_scroll_to_iter (vim->priv->text_view, iter, 0.0,
                                TRUE, 0.5, yalign);
}

static void
gb_source_vim_move_page (GbSourceVim                 *vim,
                         GbSourceVimPageDirectionType direction)
{
  GdkRectangle rect;
  GtkTextIter iter_top, iter_bottom, iter_current;
  guint offset;
  gint line, line_top, line_bottom, line_current;
  GtkTextBuffer *buffer;
  gfloat yalign = 0.0;

  g_assert (GB_IS_SOURCE_VIM (vim));

  gtk_text_view_get_visible_rect (vim->priv->text_view, &rect);
  gtk_text_view_get_iter_at_location (vim->priv->text_view, &iter_top,
                                      rect.x, rect.y);
  gtk_text_view_get_iter_at_location (vim->priv->text_view, &iter_bottom,
                                      rect.x, rect.y + rect.height);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &iter_current, NULL);

  line_top = gtk_text_iter_get_line (&iter_top);
  line_bottom = gtk_text_iter_get_line (&iter_bottom);
  line_current = gtk_text_iter_get_line (&iter_current);

  if (direction == GB_SOURCE_VIM_HALF_PAGE_UP ||
      direction == GB_SOURCE_VIM_HALF_PAGE_DOWN)
    {
      /* keep current yalign */
      if (line_bottom != line_top)
        yalign = MAX (0.0, (float)(line_current - line_top) /
                           (float)(line_bottom - line_top));
    }

  switch (direction)
    {
    case GB_SOURCE_VIM_HALF_PAGE_UP:
      line = line_current - (line_bottom - line_top) / 2;
      break;
    case GB_SOURCE_VIM_HALF_PAGE_DOWN:
      line = line_current + (line_bottom - line_top) / 2;
      break;
    case GB_SOURCE_VIM_PAGE_UP:
      yalign = 1.0;
      line = gtk_text_iter_get_line (&iter_top) + SCROLL_OFF;
      break;
    case GB_SOURCE_VIM_PAGE_DOWN:
      yalign = 0.0;
      /*
       * rect.y + rect.height is the next line after the end of the buffer so
       * now we have to decrease one more.
       */
      line = MAX (0, gtk_text_iter_get_line (&iter_bottom) - SCROLL_OFF - 1);
      break;
    default:
      g_assert_not_reached();
    }

  gtk_text_iter_set_line (&iter_current, line);

  for (offset = vim->priv->target_line_offset; offset; offset--)
    if (gtk_text_iter_ends_line (&iter_current) ||
        !gtk_text_iter_forward_char (&iter_current))
      break;

  gb_source_vim_move_to_iter (vim, &iter_current, yalign);
}

static void
gb_source_vim_indent (GbSourceVim *vim)
{
#ifndef GB_SOURCE_VIM_EXTERNAL
  GbSourceView *view;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  view = GB_SOURCE_VIEW (vim->priv->text_view);
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gtk_text_buffer_get_has_selection (buffer))
    gb_source_view_indent_selection (view);
#endif
}

static void
gb_source_vim_unindent (GbSourceVim *vim)
{
#ifndef GB_SOURCE_VIM_EXTERNAL
  GbSourceView *view;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  view = GB_SOURCE_VIEW (vim->priv->text_view);
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gtk_text_buffer_get_has_selection (buffer))
    gb_source_view_unindent_selection (view);
#endif
}

static void
gb_source_vim_add (GbSourceVim *vim,
                   gint         by_count)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gchar *endptr = NULL;
  gchar *replace = NULL;
  gchar *slice;
  gint64 value = 0;

  g_assert (vim);

  /*
   * TODO: There are a lot of smarts we can put in here. Guessing the base
   *       comes to mind (hex, octal, etc).
   */

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &iter, &selection);

  slice = gtk_text_iter_get_slice (&iter, &selection);
  value = g_ascii_strtoll (slice, &endptr, 10);

  if (((value == G_MAXINT64) || (value == G_MININT64)) && (errno == ERANGE))
    goto cleanup;

  if (!endptr || *endptr)
    goto cleanup;

  value += by_count;

  replace = g_strdup_printf ("%"G_GINT64_FORMAT, value);

  gtk_text_buffer_delete (buffer, &iter, &selection);
  gtk_text_buffer_insert (buffer, &iter, replace, -1);
  gtk_text_iter_backward_char (&iter);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

cleanup:
  g_free (slice);
  g_free (replace);
}

static GbSourceVimPhraseStatus
gb_source_vim_parse_phrase (GbSourceVim       *vim,
                            GbSourceVimPhrase *phrase)
{
  const gchar *str;
  guint count = 0;
  gchar key;
  gchar modifier;
  gint n_scanned;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (phrase);

  phrase->key = 0;
  phrase->count = 0;
  phrase->modifier = 0;

  str = vim->priv->phrase->str;

  n_scanned = sscanf (str, "%u%c%c", &count, &key, &modifier);

  if (n_scanned == 3)
    {
      phrase->count = count;
      phrase->key = key;
      phrase->modifier = modifier;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 2)
    {
      phrase->count = count;
      phrase->key = key;
      phrase->modifier = 0;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  /* Special case for "0" command. */
  if ((n_scanned == 1) && (count == 0))
    {
      phrase->key = '0';
      phrase->count = 0;
      phrase->modifier = 0;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 1)
    return GB_SOURCE_VIM_PHRASE_NEED_MORE;

  n_scanned = sscanf (str, "%c%u%c", &key, &count, &modifier);

  if (n_scanned == 3)
    {
      phrase->count = count;
      phrase->key = key;
      phrase->modifier = modifier;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  /* there's a count following key - the modifier is non-optional then */
  if (n_scanned == 2)
    return GB_SOURCE_VIM_PHRASE_NEED_MORE;

  n_scanned = sscanf (str, "%c%c", &key, &modifier);

  if (n_scanned == 2)
    {
      phrase->count = 0;
      phrase->key = key;
      phrase->modifier = modifier;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 1)
    {
      phrase->count = 0;
      phrase->key = key;
      phrase->modifier = 0;

      return GB_SOURCE_VIM_PHRASE_SUCCESS;
    }

  return GB_SOURCE_VIM_PHRASE_FAILED;
}

static gboolean
gb_source_vim_handle_normal (GbSourceVim *vim,
                             GdkEventKey *event)
{
  GbSourceVimCommand *cmd;
  GbSourceVimPhraseStatus status;
  GbSourceVimPhrase phrase;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (event);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  switch (event->keyval)
    {
    case GDK_KEY_bracketleft:
      if ((event->state & GDK_CONTROL_MASK) == 0)
        break;
      /* Fall through */
    case GDK_KEY_Escape:
      gb_source_vim_clear_selection (vim);
      gb_source_vim_clear_phrase (vim);
      return TRUE;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      gb_source_vim_clear_phrase (vim);
      gb_source_vim_move_down (vim);
      return TRUE;

    case GDK_KEY_BackSpace:
      gb_source_vim_clear_phrase (vim);
      if (!vim->priv->phrase->len)
        gb_source_vim_move_backward (vim);
      return TRUE;

    case GDK_KEY_colon:
      if (!vim->priv->phrase->len)
        {
          gb_source_vim_set_mode (vim, GB_SOURCE_VIM_COMMAND);
          return TRUE;
        }
      break;

    case GDK_KEY_a:
    case GDK_KEY_x:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          GtkTextIter begin;
          GtkTextIter end;

          gb_source_vim_clear_phrase (vim);
          gb_source_vim_clear_selection (vim);
          if (gb_source_vim_select_current_word (vim, &begin, &end))
            {
              if (gtk_text_iter_backward_char (&begin) &&
                  ('-' != gtk_text_iter_get_char (&begin)))
                gtk_text_iter_forward_char (&begin);
              gtk_text_buffer_select_range (buffer, &begin, &end);
              gb_source_vim_add (vim, (event->keyval == GDK_KEY_a) ? 1 : -1);
              gb_source_vim_clear_selection (vim);
            }
          return TRUE;
        }
      break;

    case GDK_KEY_b:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_clear_phrase (vim);
          gb_source_vim_move_page (vim, GB_SOURCE_VIM_PAGE_UP);
          return TRUE;
        }
      break;

    case GDK_KEY_d:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_clear_phrase (vim);
          gb_source_vim_move_page (vim, GB_SOURCE_VIM_HALF_PAGE_DOWN);
          return TRUE;
        }
      break;

    case GDK_KEY_f:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_clear_phrase (vim);
          gb_source_vim_move_page (vim, GB_SOURCE_VIM_PAGE_DOWN);
          return TRUE;
        }
      break;

    case GDK_KEY_r:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_clear_phrase (vim);
          gb_source_vim_redo (vim);
          return TRUE;
        }
      break;

    case GDK_KEY_u:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_clear_phrase (vim);
          gb_source_vim_move_page (vim, GB_SOURCE_VIM_HALF_PAGE_UP);
          return TRUE;
        }
      break;

    default:
      break;
    }

  if (gtk_bindings_activate_event (G_OBJECT (vim->priv->text_view), event))
    return TRUE;

  /*
   * TODO: The GdkEventKey.string field is deprecated, so we will need to
   *       determine how to do this more precisely once we can no longer use
   *       that.
   */

  if (event->string && *event->string)
    {
      g_string_append (vim->priv->phrase, event->string);
      g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_PHRASE]);
    }

  status = gb_source_vim_parse_phrase (vim, &phrase);

  switch (status)
    {
    case GB_SOURCE_VIM_PHRASE_SUCCESS:
      cmd = g_hash_table_lookup (gCommands, GINT_TO_POINTER (phrase.key));
      if (!cmd)
        {
          gb_source_vim_clear_phrase (vim);
          break;
        }

      if (cmd->flags & GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER &&
          !((cmd->flags & GB_SOURCE_VIM_COMMAND_FLAG_VISUAL) &&
            gtk_text_buffer_get_has_selection (buffer)) &&
          !phrase.modifier)
        break;

      gb_source_vim_clear_phrase (vim);

      cmd->func (vim, phrase.count, phrase.modifier);
      if (cmd->flags & GB_SOURCE_VIM_COMMAND_FLAG_VISUAL)
        gb_source_vim_clear_selection (vim);

      break;

    case GB_SOURCE_VIM_PHRASE_NEED_MORE:
      break;

    default:
    case GB_SOURCE_VIM_PHRASE_FAILED:
      gb_source_vim_clear_phrase (vim);
      break;
    }

  return TRUE;
}

static gboolean
gb_source_vim_handle_insert (GbSourceVim *vim,
                             GdkEventKey *event)
{
  switch (event->keyval)
    {
    case GDK_KEY_bracketleft:
      if ((event->state & GDK_CONTROL_MASK) == 0)
        break;
      /* Fall through */
    case GDK_KEY_Escape:
      /*
       * First move back onto the last character we entered, and then
       * return to NORMAL mode.
       */
      gb_source_vim_move_backward (vim);
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);
      return FALSE;

    case GDK_KEY_u:
      /*
       * Delete everything before the cursor upon <Control>U.
       */
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_source_vim_delete_to_line_start (vim);
          return TRUE;
        }

      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gb_source_vim_handle_command (GbSourceVim *vim,
                              GdkEventKey *event)
{
  /*
   * We typically shouldn't be hitting here, we should be focused in the
   * command_entry widget.
   */

  switch (event->keyval)
    {
    case GDK_KEY_bracketleft:
      if ((event->state & GDK_CONTROL_MASK) == 0)
        break;
      /* Fall through */
    case GDK_KEY_Escape:
      /*
       * Escape back into NORMAL mode.
       */
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);
      return TRUE;

    default:
      break;
    }

  if (!gtk_bindings_activate_event (G_OBJECT (vim->priv->text_view), event))
    {
      /*
       * TODO: Show visual error because we can't input right now. We shouldn't
       *       even get here though.
       */
    }

  return TRUE;
}

static gboolean
gb_source_vim_key_press_event_cb (GtkTextView *text_view,
                                  GdkEventKey *event,
                                  GbSourceVim *vim)
{
  gboolean ret;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);

  switch (vim->priv->mode)
    {
    case GB_SOURCE_VIM_NORMAL:
      ret = gb_source_vim_handle_normal (vim, event);
      break;

    case GB_SOURCE_VIM_INSERT:
      ret = gb_source_vim_handle_insert (vim, event);
      break;

    case GB_SOURCE_VIM_COMMAND:
      ret = gb_source_vim_handle_command (vim, event);
      break;

    default:
      g_assert_not_reached();
    }

  return ret;
}

static gboolean
gb_source_vim_focus_in_event_cb (GtkTextView *text_view,
                                 GdkEvent    *event,
                                 GbSourceVim *vim)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);

  if (vim->priv->mode == GB_SOURCE_VIM_COMMAND)
    gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);

  return FALSE;
}

static void
gb_source_vim_mark_set_cb (GtkTextBuffer *buffer,
                           GtkTextIter   *iter,
                           GtkTextMark   *mark,
                           GbSourceVim   *vim)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter);
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (vim->priv->mode == GB_SOURCE_VIM_INSERT)
    return;

  if (!gtk_widget_has_focus (GTK_WIDGET (vim->priv->text_view)))
    return;

  if (mark != gtk_text_buffer_get_insert (buffer))
    return;

  if (gtk_text_iter_ends_line (iter) &&
      !gtk_text_iter_starts_line (iter) &&
      !gtk_text_buffer_get_has_selection (buffer))
    {
      /*
       * Probably want to add a canary here for dealing with reentrancy.
       */
      if (gtk_text_iter_backward_char (iter))
        gtk_text_buffer_select_range (buffer, iter, iter);
    }
}

static void
gb_source_vim_delete_range_cb (GtkTextBuffer *buffer,
                               GtkTextIter   *begin,
                               GtkTextIter   *end,
                               GbSourceVim   *vim)
{
  GtkTextIter iter;
  GtkTextMark *insert;
  guint line;
  guint end_line;
  guint begin_line;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (begin);
  g_return_if_fail (end);
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  if (vim->priv->mode == GB_SOURCE_VIM_INSERT)
    return;

  /*
   * Replace the cursor if we maybe deleted past the end of the line.
   * This should force the cursor to be on the last character instead of
   * after it.
   */

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  line = gtk_text_iter_get_line (&iter);
  begin_line = gtk_text_iter_get_line (begin);
  end_line = gtk_text_iter_get_line (end);

  if (line >= begin_line && line <= end_line)
    {
      if (gtk_text_iter_ends_line (end))
        gb_source_vim_move_line_end (vim);
    }
}

static int
str_compare_qsort (const void *aptr,
                   const void *bptr)
{
  const gchar * const *a = aptr;
  const gchar * const *b = bptr;

  return g_strcmp0 (*a, *b);
}

static void
gb_source_vim_op_sort (GbSourceVim *vim,
                       const gchar *command_text)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter cursor;
  guint cursor_offset;
  gchar *text;
  gchar **parts;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_equal (&begin, &end))
    return;

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, insert);
  cursor_offset = gtk_text_iter_get_offset (&cursor);

  if (gtk_text_iter_compare (&begin, &end) > 0)
    text_iter_swap (&begin, &end);

  if (gtk_text_iter_starts_line (&end))
    gtk_text_iter_backward_char (&end);

  text = gtk_text_iter_get_slice (&begin, &end);
  parts = g_strsplit (text, "\n", 0);
  g_free (text);

  qsort (parts, g_strv_length (parts), sizeof (gchar *),
         str_compare_qsort);

  text = g_strjoinv ("\n", parts);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, text, -1);
  g_free (text);
  g_strfreev (parts);

  gtk_text_buffer_get_iter_at_offset (buffer, &begin, cursor_offset);
  gtk_text_buffer_select_range (buffer, &begin, &begin);
}

static void
gb_source_vim_connect (GbSourceVim *vim)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));
  g_return_if_fail (!vim->priv->connected);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  vim->priv->key_press_event_handler =
    g_signal_connect_object (vim->priv->text_view,
                             "key-press-event",
                             G_CALLBACK (gb_source_vim_key_press_event_cb),
                             vim,
                             0);

  vim->priv->focus_in_event_handler =
    g_signal_connect_object (vim->priv->text_view,
                             "focus-in-event",
                             G_CALLBACK (gb_source_vim_focus_in_event_cb),
                             vim,
                             0);

  vim->priv->mark_set_handler =
    g_signal_connect_object (buffer,
                            "mark-set",
                            G_CALLBACK (gb_source_vim_mark_set_cb),
                            vim,
                            G_CONNECT_AFTER);

  vim->priv->delete_range_handler =
    g_signal_connect_object (buffer,
                            "delete-range",
                            G_CALLBACK (gb_source_vim_delete_range_cb),
                            vim,
                            G_CONNECT_AFTER);

  if (GTK_SOURCE_IS_BUFFER (buffer))
    vim->priv->search_context =
      gtk_source_search_context_new (GTK_SOURCE_BUFFER (buffer),
                                     vim->priv->search_settings);

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);

  vim->priv->connected = TRUE;
}

static void
gb_source_vim_disconnect (GbSourceVim *vim)
{
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));
  g_return_if_fail (vim->priv->connected);

  if (vim->priv->mode == GB_SOURCE_VIM_NORMAL)
    gtk_text_view_set_overwrite (vim->priv->text_view, FALSE);

  g_signal_handler_disconnect (vim->priv->text_view,
                               vim->priv->key_press_event_handler);
  vim->priv->key_press_event_handler = 0;

  g_signal_handler_disconnect (vim->priv->text_view,
                               vim->priv->focus_in_event_handler);
  vim->priv->focus_in_event_handler = 0;

  g_signal_handler_disconnect (gtk_text_view_get_buffer (vim->priv->text_view),
                               vim->priv->mark_set_handler);
  vim->priv->mark_set_handler = 0;

  g_signal_handler_disconnect (gtk_text_view_get_buffer (vim->priv->text_view),
                               vim->priv->delete_range_handler);
  vim->priv->delete_range_handler = 0;

  g_clear_object (&vim->priv->search_context);

  vim->priv->mode = 0;

  vim->priv->connected = FALSE;
}

gboolean
gb_source_vim_get_enabled (GbSourceVim *vim)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);

  return vim->priv->enabled;
}

void
gb_source_vim_set_enabled (GbSourceVim *vim,
                           gboolean     enabled)
{
  GbSourceVimPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  priv = vim->priv;

  if (priv->enabled == enabled)
    return;

  if (enabled)
    {
      gb_source_vim_connect (vim);
      priv->enabled = TRUE;
    }
  else
    {
      gb_source_vim_disconnect (vim);
      priv->enabled = FALSE;
    }

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_ENABLED]);
}

GtkWidget *
gb_source_vim_get_text_view (GbSourceVim *vim)
{
  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), NULL);

  return (GtkWidget *)vim->priv->text_view;
}

static void
gb_source_vim_set_text_view (GbSourceVim *vim,
                             GtkTextView *text_view)
{
  GbSourceVimPrivate *priv;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = vim->priv;

  if (priv->text_view == text_view)
    return;

  if (priv->text_view)
    {
      if (priv->enabled)
        gb_source_vim_disconnect (vim);
      g_object_remove_weak_pointer (G_OBJECT (priv->text_view),
                                    (gpointer *)&priv->text_view);
      priv->text_view = NULL;
    }

  if (text_view)
    {
      priv->text_view = text_view;
      g_object_add_weak_pointer (G_OBJECT (text_view),
                                 (gpointer *)&priv->text_view);
      if (priv->enabled)
        gb_source_vim_connect (vim);
    }

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_TEXT_VIEW]);
}

static void
gb_source_vim_op_filetype (GbSourceVim *vim,
                           const gchar *name)
{
  GtkSourceLanguageManager *manager;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (g_str_has_prefix (name, "set filetype="));

  name += strlen ("set filetype=");

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  manager = gtk_source_language_manager_get_default ();
  language = gtk_source_language_manager_get_language (manager, name);
  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), language);

  gtk_widget_queue_draw (GTK_WIDGET (vim->priv->text_view));
}

static void
gb_source_vim_op_syntax (GbSourceVim *vim,
                         const gchar *name)
{
  GtkTextBuffer *buffer;
  gboolean enabled;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (g_str_has_prefix (name, "syntax "));

  name += strlen ("syntax ");

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  if (g_strcmp0 (name, "on") == 0)
    enabled = TRUE;
  else if (g_strcmp0 (name, "off") == 0)
    enabled = FALSE;
  else
    return;

  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), enabled);
}

static void
gb_source_vim_op_nu (GbSourceVim *vim,
                     const gchar *command_text)
{
  gboolean enable;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (g_str_has_prefix (command_text, "set "));

  command_text += strlen ("set ");

  if (g_strcmp0 (command_text, "nu") == 0)
    enable = TRUE;
  else if (g_strcmp0 (command_text, "nonu") == 0)
    enable = FALSE;
  else
    return;

  if (GTK_SOURCE_IS_VIEW (vim->priv->text_view))
    {
      GtkSourceView *source_view;

      source_view = GTK_SOURCE_VIEW (vim->priv->text_view);
      gtk_source_view_set_show_line_numbers (source_view, enable);
    }
}

static void
gb_source_vim_op_colorscheme (GbSourceVim *vim,
                              const gchar *name)
{
  GtkSourceStyleSchemeManager *manager;
  GtkSourceStyleScheme *scheme;
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (g_str_has_prefix (name, "colorscheme "));

  name += strlen ("colorscheme ");

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  if (!GTK_SOURCE_IS_BUFFER (buffer))
    return;

  manager = gtk_source_style_scheme_manager_get_default ();
  scheme = gtk_source_style_scheme_manager_get_scheme (manager, name);

  if (scheme)
    gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (buffer), scheme);
}

static void
gb_source_vim_do_search_and_replace (GbSourceVim *vim,
                                     GtkTextIter *begin,
                                     GtkTextIter *end,
                                     const gchar *search_text,
                                     const gchar *replace_text,
                                     gboolean     is_global)
{
  GError *error = NULL;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (search_text);
  g_assert (replace_text);
  g_assert ((!begin && !end) || (begin && end));

  /*
   * TODO: This is pretty incomplete. We don't actually respect is_global, or
   *       if we should be limiting to the current selection buffer.
   *       But having it in any state is better than none.
   */

  if (!vim->priv->search_context)
    return;

  gtk_source_search_settings_set_search_text (vim->priv->search_settings,
                                              search_text);
  gtk_source_search_settings_set_case_sensitive (vim->priv->search_settings,
                                                 TRUE);

  if (begin)
    {
      /* todo: limit to selection range */
      g_warning ("TODO: Selection based search/replace");
    }
  else
    {
      if (!gtk_source_search_context_replace_all (vim->priv->search_context,
                                                  replace_text, -1, &error))
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }
    }
}

static void
gb_source_vim_op_search_and_replace (GbSourceVim *vim,
                                     const gchar *command)
{
  GtkTextBuffer *buffer;
  const gchar *search_begin = NULL;
  const gchar *search_end = NULL;
  const gchar *replace_begin = NULL;
  const gchar *replace_end = NULL;
  gchar *search_text = NULL;
  gchar *replace_text = NULL;
  gunichar separator;
  gboolean is_global = FALSE;

  g_assert (GB_IS_SOURCE_VIM (vim));
  g_assert (g_str_has_prefix (command, "%s"));

  command += strlen ("%s");

  separator = g_utf8_get_char (command);
  if (!separator)
    return;

  search_begin = command = g_utf8_next_char (command);

  for (; *command; command = g_utf8_next_char (command))
    {
      if (*command == '\\')
        {
          command = g_utf8_next_char (command);
          if (!*command)
            return;
          continue;
        }

      if (g_utf8_get_char (command) == separator)
        {
          search_end = command;
          break;
        }
    }

  if (!search_end)
    return;

  replace_begin = command = g_utf8_next_char (command);

  for (; *command; command = g_utf8_next_char (command))
    {
      if (*command == '\\')
        {
          command = g_utf8_next_char (command);
          if (!*command)
            return;
          continue;
        }

      if (g_utf8_get_char (command) == separator)
        {
          replace_end = command;
          break;
        }
    }

  if (!replace_end)
    return;

  command = g_utf8_next_char (command);

  if (*command)
    {
      for (; *command; command++)
        {
          switch (*command)
            {
            case 'g':
              is_global = TRUE;
                break;
            /* what other options are supported? */
            default:
              break;
            }
        }
    }

  search_text = g_strndup (search_begin, search_end - search_begin);
  replace_text = g_strndup (replace_begin, replace_end - replace_begin);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      GtkTextIter begin;
      GtkTextIter end;

      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
      gb_source_vim_do_search_and_replace (vim, &begin, &end, search_text,
                                           replace_text, is_global);
    }
  else
    gb_source_vim_do_search_and_replace (vim, NULL, NULL, search_text,
                                         replace_text, is_global);

  g_free (search_text);
  g_free (replace_text);
}

static void
gb_source_vim_op_nohl (GbSourceVim *vim,
                       const gchar *command_text)
{
  if (vim->priv->search_context)
    gtk_source_search_context_set_highlight (vim->priv->search_context, FALSE);
}

static GbSourceVimOperation
gb_source_vim_parse_operation (const gchar *command_text)
{
  g_return_val_if_fail (command_text, NULL);

  if (g_str_equal (command_text, "sort"))
    return gb_source_vim_op_sort;
  else if (g_str_equal (command_text, "nohl"))
    return gb_source_vim_op_nohl;
  else if (g_str_has_prefix (command_text, "set filetype="))
    return gb_source_vim_op_filetype;
  else if (g_str_has_prefix (command_text, "syntax "))
    return gb_source_vim_op_syntax;
  else if (g_str_equal (command_text, "set nu"))
    return gb_source_vim_op_nu;
  else if (g_str_equal (command_text, "set nonu"))
    return gb_source_vim_op_nu;
  else if (g_str_has_prefix (command_text, "colorscheme "))
    return gb_source_vim_op_colorscheme;
  else if (g_str_has_prefix (command_text, "%s"))
    return gb_source_vim_op_search_and_replace;

  return NULL;
}

gboolean
gb_source_vim_is_command (const gchar *command_text)
{
  GbSourceVimOperation func;

  g_return_val_if_fail (command_text, FALSE);

  func = gb_source_vim_parse_operation (command_text);
  if (func)
    return TRUE;

  return FALSE;
}

gboolean
gb_source_vim_execute_command (GbSourceVim *vim,
                               const gchar *command)
{
  GbSourceVimOperation func;
  gboolean ret = FALSE;
  gchar *copy;

  g_return_val_if_fail (GB_IS_SOURCE_VIM (vim), FALSE);
  g_return_val_if_fail (command, FALSE);

  copy = g_strstrip (g_strdup (command));
  func = gb_source_vim_parse_operation (copy);

  if (func)
    {
      func (vim, command);
      gb_source_vim_clear_selection (vim);
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_NORMAL);
      ret = TRUE;
    }

  g_free (copy);

  return ret;
}

static void
gb_source_vim_finalize (GObject *object)
{
  GbSourceVimPrivate *priv = GB_SOURCE_VIM (object)->priv;

  if (priv->anim_timeout)
    {
      g_source_remove (priv->anim_timeout);
      priv->anim_timeout = 0;
    }

  if (priv->text_view)
    {
      gb_source_vim_disconnect (GB_SOURCE_VIM (object));
      g_object_remove_weak_pointer (G_OBJECT (priv->text_view),
                                    (gpointer *)&priv->text_view);
      priv->text_view = NULL;
    }

  g_clear_object (&priv->search_settings);

  g_string_free (priv->phrase, TRUE);
  priv->phrase = NULL;

  G_OBJECT_CLASS (gb_source_vim_parent_class)->finalize (object);
}

static void
gb_source_vim_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbSourceVim *vim = GB_SOURCE_VIM (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gb_source_vim_get_enabled (vim));
      break;

    case PROP_MODE:
      g_value_set_enum (value, gb_source_vim_get_mode (vim));
      break;

    case PROP_PHRASE:
      g_value_set_string (value, vim->priv->phrase->str);
      break;

    case PROP_TEXT_VIEW:
      g_value_set_object (value, gb_source_vim_get_text_view (vim));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_vim_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbSourceVim *vim = GB_SOURCE_VIM (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gb_source_vim_set_enabled (vim, g_value_get_boolean (value));
      break;

    case PROP_TEXT_VIEW:
      gb_source_vim_set_text_view (vim, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_source_vim_cmd_repeat (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  /* TODO! */
}

static void
gb_source_vim_cmd_begin_search (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gchar *text = NULL;

  g_assert (GB_IS_SOURCE_VIM (vim));

  if (vim->priv->search_context)
    gtk_source_search_context_set_highlight (vim->priv->search_context, FALSE);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
      text = gtk_text_iter_get_slice (&begin, &end);
    }

  g_signal_emit (vim, gSignals [BEGIN_SEARCH], 0, text);

  g_free (text);
}

static void
gb_source_vim_cmd_forward_line_end (GbSourceVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_move_line_end (vim);
}

static void
gb_source_vim_cmd_backward_0 (GbSourceVim *vim,
                              guint        count,
                              gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_move_line0 (vim);
}

static void
gb_source_vim_cmd_backward_start (GbSourceVim *vim,
                                  guint        count,
                                  gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_move_line_start (vim, FALSE);
}

static void
gb_source_vim_cmd_backward_paragraph (GbSourceVim *vim,
                                      guint        count,
                                      gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_backward_paragraph (vim);
}

static void
gb_source_vim_cmd_forward_paragraph (GbSourceVim *vim,
                                      guint        count,
                                      gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_forward_paragraph (vim);
}

static void
gb_source_vim_cmd_match_backward (GbSourceVim *vim,
                                  guint        count,
                                  gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_reverse_search (vim);
}

static void
gb_source_vim_cmd_match_forward (GbSourceVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_search (vim);
}

static void
gb_source_vim_cmd_indent (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_indent (vim);

  gb_source_vim_clear_selection (vim);
}

static void
gb_source_vim_cmd_unindent (GbSourceVim *vim,
                            guint        count,
                            gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_unindent (vim);

  gb_source_vim_clear_selection (vim);
}

static void
gb_source_vim_cmd_insert_end (GbSourceVim *vim,
                              guint        count,
                              gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_clear_selection (vim);
  gb_source_vim_move_line_end (vim);
}

static void
gb_source_vim_cmd_insert_after (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_clear_selection (vim);
  gb_source_vim_move_forward (vim);
}

static void
gb_source_vim_cmd_backward_word (GbSourceVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_backward_word (vim);
}

static void
gb_source_vim_cmd_change (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  if (modifier == 'c')
    {
      gb_source_vim_cmd_delete (vim, count, 'd');
      gb_source_vim_cmd_insert_before_line (vim, 0, '\0');
    }
  else if (modifier != 'd')
    {
      /* cd should do nothing */
      gb_source_vim_cmd_delete (vim, count, modifier);
      gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
    }
}

static void
gb_source_vim_cmd_change_to_end (GbSourceVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_cmd_delete_to_end (vim, count, '\0');
  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_move_forward (vim);
}

static void
gb_source_vim_cmd_delete (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (!gtk_text_buffer_get_has_selection (buffer))
    {
      if (modifier == 'd')
        {
          GtkTextMark *insert;
          GtkTextIter end_iter;
          GtkTextIter mark_iter;

          /*
           * WORKAROUND:
           *
           * We need to workaround that we can't "line select" the last line
           * in the buffer with GtkTextBuffer. So instead, we'll just handle
           * that case specially here.
           */
          insert = gtk_text_buffer_get_insert (buffer);
          gtk_text_buffer_get_iter_at_mark (buffer, &mark_iter, insert);
          gtk_text_buffer_get_end_iter (buffer, &end_iter);

          if (gtk_text_iter_equal (&mark_iter, &end_iter))
            {
              gtk_text_iter_backward_char (&mark_iter);
              gb_source_vim_select_range (vim, &mark_iter, &end_iter);
            }
          else
            gb_source_vim_cmd_select_line (vim, count, '\0');
        }
      else
        gb_source_vim_apply_motion (vim, modifier, count);
    }

  gb_source_vim_delete_selection (vim);
}

static void
gb_source_vim_cmd_delete_to_end (GbSourceVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  gb_source_vim_clear_selection (vim);
  gb_source_vim_select_char (vim);
  gb_source_vim_move_line_end (vim);
  for (i = 1; i < count; i++)
    gb_source_vim_move_down (vim);
  gb_source_vim_delete_selection (vim);
}

static void
gb_source_vim_cmd_forward_word_end (GbSourceVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_forward_word_end (vim);
}

static void
gb_source_vim_cmd_g (GbSourceVim *vim,
                     guint        count,
                     gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  /*
   * TODO: We have more plumbing todo so we can support commands that are
   *       multiple characters (gU gu g~ and gg are all separate commands).
   *       We can support `gU' on a selection, but not `gUw'.
   */

  switch (modifier)
    {
    case '~':
      /* swap case */
      break;

    case 'u':
      /* lowercase */
      break;

    case 'U':
      /* uppercase */
      break;

    case 'g':
      /* jump to beginning of buffer. */
      gb_source_vim_clear_selection (vim);
      gb_source_vim_move_to_line_n (vim, 0);
      break;

    default:
      break;
    }
}

static void
gb_source_vim_cmd_goto_line (GbSourceVim *vim,
                             guint        count,
                             gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  if (count)
    gb_source_vim_move_to_line_n (vim, count - 1);
  else
    gb_source_vim_move_to_end (vim);
}

static void
gb_source_vim_cmd_move_backward (GbSourceVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_backward (vim);
}

static void
gb_source_vim_cmd_insert_start (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_clear_selection (vim);
  gb_source_vim_move_line_start (vim, TRUE);
}

static void
gb_source_vim_cmd_insert (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_clear_selection (vim);
}

static void
gb_source_vim_cmd_move_down (GbSourceVim *vim,
                             guint        count,
                             gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_down (vim);
}

static void
gb_source_vim_cmd_move_up (GbSourceVim *vim,
                           guint        count,
                           gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_up (vim);
}

static void
gb_source_vim_cmd_move_forward (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_forward (vim);
}

static void
gb_source_vim_cmd_jump_to_doc (GbSourceVim *vim,
                               guint        count,
                               gchar        modifier)
{
  GtkTextIter begin;
  GtkTextIter end;
  gchar *word;

  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_select_current_word (vim, &begin, &end);
  word = gtk_text_iter_get_slice (&begin, &end);
  g_signal_emit (vim, gSignals [JUMP_TO_DOC], 0, word);
  g_free (word);

  gb_source_vim_select_range (vim, &begin, &begin);
}

static void
gb_source_vim_cmd_insert_before_line (GbSourceVim *vim,
                                      guint        count,
                                      gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_insert_nl_before (vim);
}

static void
gb_source_vim_cmd_insert_after_line (GbSourceVim *vim,
                                     guint        count,
                                     gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gb_source_vim_insert_nl_after (vim, TRUE);
}

static void
gb_source_vim_cmd_paste_after (GbSourceVim *vim,
                               guint        count,
                               gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_paste (vim);
}

static void
gb_source_vim_cmd_paste_before (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  /* TODO: Paste Before intead of after. */
  gb_source_vim_cmd_paste_after (vim, count, modifier);
}

static void
gb_source_vim_cmd_overwrite (GbSourceVim *vim,
                             guint        count,
                             gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_set_mode (vim, GB_SOURCE_VIM_INSERT);
  gtk_text_view_set_overwrite (vim->priv->text_view, TRUE);
}

static void
gb_source_vim_cmd_replace (GbSourceVim *vim,
                           guint        count,
                           gchar        modifier)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  gboolean at_end;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  gtk_text_buffer_begin_user_action (buffer);
  gb_source_vim_delete_selection (vim);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_forward_char (&begin);
  if (gtk_text_iter_ends_line (&begin))
    at_end = TRUE;
  else
    {
      gtk_text_iter_backward_char (&begin);
      at_end = FALSE;
    }

  gtk_text_buffer_insert (buffer, &begin, &modifier, 1);
  if (at_end)
    gb_source_vim_move_forward (vim);
  else
    gb_source_vim_move_backward (vim);

  gtk_text_buffer_end_user_action (buffer);
}

static void
gb_source_vim_cmd_substitute (GbSourceVim *vim,
                              guint        count,
                              gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_cmd_change (vim, count, 'l');
}

static void
gb_source_vim_cmd_undo (GbSourceVim *vim,
                        guint        count,
                        gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_undo (vim);
}

static void
gb_source_vim_cmd_select_line (GbSourceVim *vim,
                               guint        count,
                               gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  gb_source_vim_select_line (vim);
  for (i = 1; i < count; i++)
    gb_source_vim_move_down (vim);
}

static void
gb_source_vim_cmd_select (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  gb_source_vim_select_char (vim);
  for (i = 1; i < count; i++)
    gb_source_vim_move_forward (vim);
}

static void
gb_source_vim_cmd_forward_word (GbSourceVim *vim,
                                guint        count,
                                gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_move_forward_word (vim);
}

static void
gb_source_vim_cmd_delete_selection (GbSourceVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_delete_selection (vim);
}

static void
gb_source_vim_cmd_yank (GbSourceVim *vim,
                        guint        count,
                        gchar        modifier)
{
  GtkTextBuffer *buffer;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  gb_source_vim_save_position (vim);

  if (!gtk_text_buffer_get_has_selection (buffer))
    {
      if (modifier == 'y')
        gb_source_vim_cmd_select_line (vim, count, '\0');
      else
        gb_source_vim_apply_motion (vim, modifier, count);
    }

  gb_source_vim_yank (vim);
  gb_source_vim_clear_selection (vim);
  gb_source_vim_restore_position (vim);
}

static void
gb_source_vim_cmd_join (GbSourceVim *vim,
                        guint        count,
                        gchar        modifier)
{
  g_assert (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_join (vim);
}

static void
gb_source_vim_cmd_center (GbSourceVim *vim,
                          guint        count,
                          gchar        modifier)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  switch (modifier)
    {
    case 'b':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.5, 1.0);
      break;

    case 't':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.5, 0.0);
      break;

    case 'z':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.5, 0.5);
      break;

    default:
      break;
    }
}

static void
gb_source_vim_cmd_matching_bracket (GbSourceVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  GtkTextIter iter;
  GtkTextIter selection;

  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  gb_source_vim_get_selection_bounds (vim, &iter, &selection);

  switch (gtk_text_iter_get_char (&iter))
    {
    case '{':
    case '}':
    case '[':
    case ']':
    case '(':
    case ')':
      gb_source_vim_move_matching_bracket (vim);
      break;

    default:
      break;
    }
}

static void
gb_source_vim_cmd_toggle_case (GbSourceVim *vim,
                               guint        count,
                               gchar        modifier)
{
  GtkTextBuffer *buffer;
  guint i;

  g_assert (GB_IS_SOURCE_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gtk_text_buffer_get_has_selection (buffer))
    count = 1;
  else
    count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_source_vim_toggle_case (vim);
}

static void
gb_source_vim_class_register_command (GbSourceVimClass       *klass,
                                      gchar                   key,
                                      GbSourceVimCommandFlags flags,
                                      GbSourceVimCommandType  type,
                                      GbSourceVimCommandFunc  func)
{
  GbSourceVimCommand *cmd;
  gpointer keyptr = GINT_TO_POINTER ((gint)key);

  g_assert (GB_IS_SOURCE_VIM_CLASS (klass));

  /*
   * TODO: It would be neat to have gCommands be a field in the klass. We
   *       could then just chain up to discover the proper command. This
   *       allows for subclasses to override and add new commands.
   *       To do so will probably take registering the GObjectClass
   *       manually to set base_init().
   */

  if (!gCommands)
    gCommands = g_hash_table_new (g_direct_hash, g_direct_equal);

  cmd = g_new0 (GbSourceVimCommand, 1);
  cmd->type = type;
  cmd->key = key;
  cmd->func = func;
  cmd->flags = flags;

  g_hash_table_replace (gCommands, keyptr, cmd);
}

static void
gb_source_vim_class_init (GbSourceVimClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_source_vim_finalize;
  object_class->get_property = gb_source_vim_get_property;
  object_class->set_property = gb_source_vim_set_property;

  gParamSpecs [PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          _("Enabled"),
                          _("If the VIM engine is enabled."),
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_ENABLED,
                                   gParamSpecs [PROP_ENABLED]);

  gParamSpecs [PROP_MODE] =
    g_param_spec_enum ("mode",
                       _("Mode"),
                       _("The current mode of the widget."),
                       GB_TYPE_SOURCE_VIM_MODE,
                       GB_SOURCE_VIM_NORMAL,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MODE,
                                   gParamSpecs [PROP_MODE]);

  gParamSpecs [PROP_PHRASE] =
    g_param_spec_string ("phrase",
                         _("Phrase"),
                         _("The current phrase input."),
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_PHRASE,
                                   gParamSpecs [PROP_PHRASE]);

  gParamSpecs [PROP_TEXT_VIEW] =
    g_param_spec_object ("text-view",
                         _("Text View"),
                         _("The text view the VIM engine is managing."),
                         GTK_TYPE_TEXT_VIEW,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_TEXT_VIEW,
                                   gParamSpecs [PROP_TEXT_VIEW]);

  /**
   * GbSourceVim::begin-search:
   * @search_text: (allow none): Optional search text to apply to the search.
   *
   * This signal is emitted when the `/` key is pressed. The consuming code
   * should make their search entry widget visible and set the search text
   * to @search_text if non-%NULL.
   */
  gSignals [BEGIN_SEARCH] =
    g_signal_new ("begin-search",
                  GB_TYPE_SOURCE_VIM,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceVimClass, begin_search),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  /**
   * GbSourceVim::command-visibility-toggled:
   * @visible: If the the command entry should be visible.
   *
   * The "command-visibility-toggled" signal is emitted when the command entry
   * should be shown or hidden. The command entry is used to interact with the
   * VIM style command line.
   */
  gSignals [COMMAND_VISIBILITY_TOGGLED] =
    g_signal_new ("command-visibility-toggled",
                  GB_TYPE_SOURCE_VIM,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceVimClass,
                                   command_visibility_toggled),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  /**
   * GbSourceVim::jump-to-doc:
   * @search_text: keyword to search for.
   *
   * Requests that documentation for @search_text is shown. This is typically
   * performed with SHIFT-K in VIM.
   */
  gSignals [JUMP_TO_DOC] =
    g_signal_new ("jump-to-doc",
                  GB_TYPE_SOURCE_VIM,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GbSourceVimClass, jump_to_doc),
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  /*
   * Register all of our internal VIM commands. These can be used directly
   * or via phrases.
   */
  gb_source_vim_class_register_command (klass, '.',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_repeat);
  gb_source_vim_class_register_command (klass, '/',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_JUMP,
                                        gb_source_vim_cmd_begin_search);
  gb_source_vim_class_register_command (klass, '$',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_line_end);
  gb_source_vim_class_register_command (klass, '0',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_backward_0);
  gb_source_vim_class_register_command (klass, '^',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_backward_start);
  gb_source_vim_class_register_command (klass, '}',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_paragraph);
  gb_source_vim_class_register_command (klass, '{',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_backward_paragraph);
  gb_source_vim_class_register_command (klass, '#',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_JUMP,
                                        gb_source_vim_cmd_match_backward);
  gb_source_vim_class_register_command (klass, '*',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_JUMP,
                                        gb_source_vim_cmd_match_forward);
  gb_source_vim_class_register_command (klass, '>',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_indent);
  gb_source_vim_class_register_command (klass, '<',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_unindent);
  gb_source_vim_class_register_command (klass, '%',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_JUMP,
                                        gb_source_vim_cmd_matching_bracket);
  gb_source_vim_class_register_command (klass, '~',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_toggle_case);
  gb_source_vim_class_register_command (klass, 'A',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert_end);
  gb_source_vim_class_register_command (klass, 'a',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert_after);
  gb_source_vim_class_register_command (klass, 'B',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_backward_word);
  gb_source_vim_class_register_command (klass, 'b',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_backward_word);
  gb_source_vim_class_register_command (klass, 'c',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER |
                                        GB_SOURCE_VIM_COMMAND_FLAG_VISUAL,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_change);
  gb_source_vim_class_register_command (klass, 'C',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_change_to_end);
  gb_source_vim_class_register_command (klass, 'd',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER |
                                        GB_SOURCE_VIM_COMMAND_FLAG_VISUAL,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_delete);
  gb_source_vim_class_register_command (klass, 'D',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_delete_to_end);
  gb_source_vim_class_register_command (klass, 'E',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_word_end);
  gb_source_vim_class_register_command (klass, 'e',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_word_end);
  gb_source_vim_class_register_command (klass, 'G',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_LINEWISE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_goto_line);
  gb_source_vim_class_register_command (klass, 'g',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_g);
  gb_source_vim_class_register_command (klass, 'h',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_move_backward);
  gb_source_vim_class_register_command (klass, 'I',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert_start);
  gb_source_vim_class_register_command (klass, 'i',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert);
  gb_source_vim_class_register_command (klass, 'j',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_LINEWISE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_move_down);
  gb_source_vim_class_register_command (klass, 'J',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_join);
  gb_source_vim_class_register_command (klass, 'k',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_LINEWISE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_move_up);
  gb_source_vim_class_register_command (klass, 'K',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_NOOP,
                                        gb_source_vim_cmd_jump_to_doc);
  gb_source_vim_class_register_command (klass, 'l',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_move_forward);
  gb_source_vim_class_register_command (klass, 'O',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert_before_line);
  gb_source_vim_class_register_command (klass, 'o',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_insert_after_line);
  gb_source_vim_class_register_command (klass, 'P',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_paste_before);
  gb_source_vim_class_register_command (klass, 'p',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_paste_after);
  gb_source_vim_class_register_command (klass, 'R',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_overwrite);
  gb_source_vim_class_register_command (klass, 'r',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_replace);
  gb_source_vim_class_register_command (klass, 's',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_substitute);
  gb_source_vim_class_register_command (klass, 'u',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_undo);
  gb_source_vim_class_register_command (klass, 'V',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_NOOP,
                                        gb_source_vim_cmd_select_line);
  gb_source_vim_class_register_command (klass, 'v',
                                        GB_SOURCE_VIM_COMMAND_FLAG_NONE,
                                        GB_SOURCE_VIM_COMMAND_NOOP,
                                        gb_source_vim_cmd_select);
  gb_source_vim_class_register_command (klass, 'W',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_word);
  gb_source_vim_class_register_command (klass, 'w',
                                        GB_SOURCE_VIM_COMMAND_FLAG_MOTION_EXCLUSIVE,
                                        GB_SOURCE_VIM_COMMAND_MOVEMENT,
                                        gb_source_vim_cmd_forward_word);
  gb_source_vim_class_register_command (klass, 'x',
                                        GB_SOURCE_VIM_COMMAND_FLAG_VISUAL,
                                        GB_SOURCE_VIM_COMMAND_CHANGE,
                                        gb_source_vim_cmd_delete_selection);
  gb_source_vim_class_register_command (klass, 'y',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER |
                                        GB_SOURCE_VIM_COMMAND_FLAG_VISUAL,
                                        GB_SOURCE_VIM_COMMAND_NOOP,
                                        gb_source_vim_cmd_yank);
  gb_source_vim_class_register_command (klass, 'z',
                                        GB_SOURCE_VIM_COMMAND_FLAG_REQUIRES_MODIFIER,
                                        GB_SOURCE_VIM_COMMAND_NOOP,
                                        gb_source_vim_cmd_center);
}

static void
gb_source_vim_init (GbSourceVim *vim)
{
  vim->priv = gb_source_vim_get_instance_private (vim);
  vim->priv->enabled = FALSE;
  vim->priv->mode = 0;
  vim->priv->phrase = g_string_new (NULL);
  vim->priv->search_settings = gtk_source_search_settings_new ();
}

GType
gb_source_vim_mode_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GB_SOURCE_VIM_NORMAL, "GB_SOURCE_VIM_NORMAL", "NORMAL" },
    { GB_SOURCE_VIM_INSERT, "GB_SOURCE_VIM_INSERT", "INSERT" },
    { GB_SOURCE_VIM_COMMAND, "GB_SOURCE_VIM_COMMAND", "COMMAND" },
    { 0 }
  };

  if (!type_id)
    type_id = g_enum_register_static ("GbSourceVimMode", values);

  return type_id;
}
