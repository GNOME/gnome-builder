/* gb-editor-vim.c
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

#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>

#include "gb-editor-vim.h"
#include "gb-log.h"
#include "gb-source-view.h"
#include "gb-string.h"

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
 *  - Mark which commands are "movements" so that we can use that when
 *    looking up movement modifiers. Also should mark jumps, etc.
 */

/**
 * GbEditorVimCommandFunc:
 * @vim: The #GbEditorVim instance.
 * @count: The number modifier for the command.
 * @modifier: A potential trailing modifer character.
 *
 * This is a function prototype for commands to implement themselves. They
 * can potentially use the count to perform the operation multiple times.
 *
 * However, not all commands support this or will use it.
 */
typedef void (*GbEditorVimCommandFunc) (GbEditorVim        *vim,
                                        guint               count,
                                        gchar               modifier);

struct _GbEditorVimPrivate
{
  GtkTextView     *text_view;
  GString         *phrase;
  GbEditorVimMode  mode;
  gulong           key_press_event_handler;
  gulong           focus_in_event_handler;
  gulong           mark_set_handler;
  gulong           delete_range_handler;
  guint            target_line_offset;
  guint            enabled : 1;
  guint            connected : 1;
};

typedef enum
{
  GB_EDITOR_VIM_COMMAND_NOOP,
  GB_EDITOR_VIM_COMMAND_MOVEMENT,
  GB_EDITOR_VIM_COMMAND_CHANGE,
  GB_EDITOR_VIM_COMMAND_JUMP,
} GbEditorVimCommandType;

/**
 * GbEditorVimCommand:
 *
 * This structure encapsulates what we need to know about a command before
 * we can dispatch it. requires_modifier means there needs to be a
 * supplimental character provided after the key. Such an example would be
 * "dd", "dw", "yy", or "gg".
 */
typedef struct
{
  GbEditorVimCommandFunc  func;
  GbEditorVimCommandType  type;
  gchar                   key;
  guint                   requires_modifier : 1;
} GbEditorVimCommand;

typedef enum
{
  GB_EDITOR_VIM_PHRASE_FAILED,
  GB_EDITOR_VIM_PHRASE_SUCCESS,
  GB_EDITOR_VIM_PHRASE_NEED_MORE,
} GbEditorVimPhraseStatus;

typedef struct
{
  guint count;
  gchar key;
  gchar modifier;
} GbEditorVimPhrase;

enum
{
  PROP_0,
  PROP_ENABLED,
  PROP_MODE,
  PROP_TEXT_VIEW,
  LAST_PROP
};

enum
{
  COMMAND_VISIBILITY_TOGGLED,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorVim, gb_editor_vim, G_TYPE_OBJECT)

static GHashTable *gCommands;
static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

GbEditorVim *
gb_editor_vim_new (GtkTextView *text_view)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

  return g_object_new (GB_TYPE_EDITOR_VIM,
                       "text-view", text_view,
                       NULL);
}

static guint
gb_editor_vim_get_line_offset (GbEditorVim *vim)
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
gb_editor_vim_clear_selection (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
      gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

GbEditorVimMode
gb_editor_vim_get_mode (GbEditorVim *vim)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), 0);

  return vim->priv->mode;
}

void
gb_editor_vim_set_mode (GbEditorVim     *vim,
                        GbEditorVimMode  mode)
{
  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  vim->priv->mode = mode;

  /*
   * Switch to the "block mode" cursor for non-insert mode. We are totally
   * abusing "overwrite" here simply to look more like VIM.
   */
  gtk_text_view_set_overwrite (vim->priv->text_view,
                               (mode != GB_EDITOR_VIM_INSERT));

  /*
   * Clear any in flight phrases.
   */
  g_string_truncate (vim->priv->phrase, 0);

  /*
   * If we are going back to navigation mode, stash our current buffer
   * position for use in commands like j and k.
   */
  if (mode == GB_EDITOR_VIM_NORMAL)
    vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  /*
   * If there are any snippets active, escape out of them.
   */
  if ((mode != GB_EDITOR_VIM_INSERT) &&
      GB_IS_SOURCE_VIEW (vim->priv->text_view))
    gb_source_view_clear_snippets (GB_SOURCE_VIEW (vim->priv->text_view));

  /*
   * Make the command entry visible if necessary.
   */
  g_signal_emit (vim, gSignals [COMMAND_VISIBILITY_TOGGLED], 0,
                 (mode == GB_EDITOR_VIM_COMMAND));

  /*
   * TODO: This should actually happen always on insert->normal transition.
   *
   * If we are are going to normal mode and are at the end of the line,
   * then move back a character so we are on the last character as opposed
   * to after it. This matches closer to VIM.
   */
  if (mode == GB_EDITOR_VIM_NORMAL)
    {
      /* TODO: Old code did not respect selections */
    }

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_MODE]);
}

static void
gb_editor_vim_maybe_auto_indent (GbEditorVim *vim)
{
  GbSourceAutoIndenter *auto_indenter;
  GbEditorVimPrivate *priv;
  GbSourceView *source_view;
  GdkEvent fake_event = { 0 };

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

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
}

static gboolean
gb_editor_vim_get_selection_bounds (GbEditorVim *vim,
                                    GtkTextIter *insert_iter,
                                    GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
gb_editor_vim_select_range (GbEditorVim *vim,
                            GtkTextIter *insert_iter,
                            GtkTextIter *selection_iter)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextMark *selection;

  ENTRY;

  g_assert (GB_IS_EDITOR_VIM (vim));
  g_assert (insert_iter);
  g_assert (selection_iter);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  selection = gtk_text_buffer_get_selection_bound (buffer);

  gtk_text_buffer_move_mark (buffer, insert, insert_iter);
  gtk_text_buffer_move_mark (buffer, selection, selection_iter);

  EXIT;
}

static void
gb_editor_vim_move_line0 (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (vim->priv->text_view));
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  gtk_text_iter_set_line_offset (&iter, 0);

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_move_line_start (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter original;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (vim->priv->text_view));
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);
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
  if (g_unichar_isspace (gtk_text_iter_get_char (&iter)) ||
      gtk_text_iter_equal (&iter, &original))
    {
      gb_editor_vim_move_line0 (vim);
      return;
    }

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_move_line_end (GbEditorVim *vim)
{
  GbEditorVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  while (!gtk_text_iter_ends_line (&iter))
    if (!gtk_text_iter_forward_char (&iter))
      break;

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_move_backward (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);
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
          gb_editor_vim_select_range (vim, &iter, &selection);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);

      vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
    }
}

static void
gb_editor_vim_move_backward_word (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  GtkTextMark *insert;
  gboolean has_selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  if (!gtk_text_iter_backward_word_start (&iter))
    gtk_text_buffer_get_start_iter (buffer, &iter);

  if (has_selection)
    {
      if (gtk_text_iter_equal (&iter, &selection))
        gtk_text_iter_backward_word_start (&iter);
      gb_editor_vim_select_range (vim, &iter, &selection);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_move_forward (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);

  if (gtk_text_iter_forward_char (&iter) &&
      (line == gtk_text_iter_get_line (&iter)))
    {
      if (has_selection)
        {
          if (gtk_text_iter_equal (&iter, &selection))
            {
              gtk_text_iter_forward_char (&iter);
              gtk_text_iter_backward_char (&selection);
            }
          gb_editor_vim_select_range (vim, &iter, &selection);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);

      vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
    }
}

static void
gb_editor_vim_move_forward_word (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  GtkTextMark *insert;
  gboolean has_selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

  /*
   * TODO: Make the word boundaries more like VIM.
   */

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  if (gtk_text_iter_inside_word (&iter))
    {
      if (!gtk_text_iter_forward_word_end (&iter))
        gtk_text_buffer_get_end_iter (buffer, &iter);
    }

  if (gtk_text_iter_forward_word_end (&iter))
    gtk_text_iter_backward_word_start (&iter);

  if (has_selection)
    {
      if (gtk_text_iter_equal (&iter, &selection))
        gtk_text_iter_forward_word_end (&iter);
      gb_editor_vim_select_range (vim, &iter, &selection);
    }
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
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
gb_editor_vim_move_down (GbEditorVim *vim)
{
  GbEditorVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;
  guint offset;

  g_assert (GB_IS_EDITOR_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);
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
      if (gtk_text_iter_compare (&iter, &selection) < 0)
        text_iter_swap (&iter, &selection);
      gtk_text_iter_set_line (&iter, gtk_text_iter_get_line (&iter) + 1);
      gb_editor_vim_select_range (vim, &iter, &selection);
      GOTO (move_mark);
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
          if (gtk_text_iter_equal (&iter, &selection))
            gtk_text_iter_forward_char (&iter);
          gb_editor_vim_select_range (vim, &iter, &selection);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

move_mark:
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_move_up (GbEditorVim *vim)
{
  GbEditorVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;
  guint line;
  guint offset;

  g_assert (GB_IS_EDITOR_VIM (vim));

  priv = vim->priv;

  buffer = gtk_text_view_get_buffer (priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);
  line = gtk_text_iter_get_line (&iter);
  offset = vim->priv->target_line_offset;

  if (line == 0)
    return;

  /*
   * TODO: Bug when `vk`: Missing original character.
   */

  if (is_single_line_selection (&iter, &selection))
    {
      if (gtk_text_iter_compare (&iter, &selection) > 0)
        text_iter_swap (&iter, &selection);
      gtk_text_iter_set_line (&iter, gtk_text_iter_get_line (&iter) - 1);
      gb_editor_vim_select_range (vim, &iter, &selection);
      GOTO (move_mark);
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
          gb_editor_vim_select_range (vim, &iter, &selection);
        }
      else
        gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

move_mark:
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_delete_selection (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkClipboard *clipboard;
  gchar *text;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
            return;
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

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_select_line (GbEditorVim *vim)
{
  GbEditorVimPrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
    if (!gtk_text_iter_forward_char (&end))
      break;

  /*
   * We actually want to select the \n befire the line.
   */
  if (gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_char (&end);

  gtk_text_buffer_select_range (buffer, &begin, &end);

  vim->priv->target_line_offset = 0;
}

static void
gb_editor_vim_select_char (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_forward_char (&end))
    gb_editor_vim_select_range (vim, &end, &begin);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_undo (GbEditorVim *vim)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
   * clear it manually to the insert mark position.
   */
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_redo (GbEditorVim *vim)
{
  GtkSourceUndoManager *undo;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

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

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_insert_nl_before (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint line;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
  gb_editor_vim_maybe_auto_indent (vim);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_insert_nl_after (GbEditorVim *vim,
                               gboolean     auto_indent)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
    gb_editor_vim_maybe_auto_indent (vim);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_delete_to_line_start (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  /*
   * Clear any selection so we are left at the cursor position.
   */
  gb_editor_vim_clear_selection (vim);

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
      gb_editor_vim_move_line_start (vim);

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

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_paste (GbEditorVim *vim)
{
  GtkClipboard *clipboard;
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  guint line;
  guint offset;
  gchar *text;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

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
      gb_editor_vim_insert_nl_after (vim, FALSE);
      gtk_clipboard_set_text (clipboard, trimmed, -1);
      g_signal_emit_by_name (vim->priv->text_view, "paste-clipboard");
      gtk_clipboard_set_text (clipboard, text, -1);
      g_free (trimmed);
    }
  else
    {
      /*
       * By default, GtkTextBuffer will paste at our current position.
       * While VIM will paste after the current position. Let's advance the
       * buffer a single character on the current line if possible. We switch
       * to insert mode so that we can move past the last character in the
       * buffer. Possibly should consider an alternate design for this.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gb_editor_vim_move_forward (vim);
      g_signal_emit_by_name (vim->priv->text_view, "paste-clipboard");
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);
    }

  gtk_text_buffer_end_user_action (buffer);

  /*
   * Restore the cursor position.
   */
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line + 1);
  for (; offset; offset--)
    if (gtk_text_iter_ends_line (&iter) || !gtk_text_iter_forward_char (&iter))
      break;
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  g_free (text);
}

static void
gb_editor_vim_move_to_end (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  gtk_text_buffer_get_end_iter (buffer, &iter);

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_move_end_of_word (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  /*
   * Move forward to the end of the next word. If we successfully find it,
   * move back one character so the cursor is "on-top" of the character just
   * like in VIM.
   */
  if (!gtk_text_iter_forward_char (&iter) ||
      !gtk_text_iter_forward_word_end (&iter))
    gtk_text_buffer_get_end_iter (buffer, &iter);
  else if (!has_selection)
    gtk_text_iter_backward_char (&iter);

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_yank (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkClipboard *clipboard;
  gchar *text;

  g_assert (GB_IS_EDITOR_VIM (vim));

  /*
   * Get the current textview selection.
   */
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  /*
   * Copy the selected text and insert it into the clipboard.
   */
  text = gtk_text_iter_get_slice (&begin, &end);
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
  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static gboolean
gb_editor_vim_select_current_word (GbEditorVim *vim,
                                   GtkTextIter *begin,
                                   GtkTextIter *end)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;

  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);
  g_return_val_if_fail (begin, FALSE);
  g_return_val_if_fail (end, FALSE);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, begin, insert);

  if (gtk_text_iter_forward_word_end (begin))
    {
      gtk_text_iter_assign (end, begin);
      if (gtk_text_iter_backward_word_start (begin))
        return TRUE;
    }

  return FALSE;
}

static void
gb_editor_vim_reverse_search (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GbSourceView *source_view;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  source_view = GB_SOURCE_VIEW (vim->priv->text_view);
  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  if (gb_editor_vim_select_current_word (vim, &begin, &end))
    {
      gchar *text;

      /*
       * Fetch the search text.
       */
      text = gtk_text_iter_get_slice (&begin, &end);

      /*
       * Move to right before the current word and clear the selection.
       */
      if (gtk_text_iter_compare (&begin, &end) <= 0)
        gtk_text_buffer_select_range (buffer, &begin, &begin);
      else
        gtk_text_buffer_select_range (buffer, &end, &end);

      /*
       * Start searching.
       */
      gb_source_view_begin_search (source_view, GTK_DIR_UP, text);
      g_free (text);

      /*
       * But don't let the search entry focus. VIM let's us just keep hitting
       * '#' over and over without any intervention, and that's a useful
       * feature.
       */
      gtk_widget_grab_focus (GTK_WIDGET (vim->priv->text_view));

      /*
       * TODO: It would be nice to clear the selection to match closer to
       *       VIM. However, we do a delayed selection of the match in the
       *       editor tab (which eventually needs to search asynchronously
       *       too). So we need a better way to do this.
       */
#if 0
      gb_editor_vim_clear_selection (vim);
#endif
    }
}

static void
gb_editor_vim_search (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GbSourceView *source_view;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  source_view = GB_SOURCE_VIEW (vim->priv->text_view);

  if (gb_editor_vim_select_current_word (vim, &begin, &end))
    {
      gchar *text;

      /*
       * Query the search text.
       */
      text = gtk_text_iter_get_slice (&begin, &end);

      /*
       * Move past the current word so that we don't reselect it.
       */
      buffer = gtk_text_view_get_buffer (vim->priv->text_view);
      if (gtk_text_buffer_get_has_selection (buffer))
        {
          gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
          if (gtk_text_iter_compare (&begin, &end) <= 0)
            gtk_text_buffer_select_range (buffer, &end, &end);
          else
            gtk_text_buffer_select_range (buffer, &begin, &begin);
        }

      /*
       * Start searching.
       */
      gb_source_view_begin_search (source_view, GTK_DIR_DOWN, text);
      g_free (text);

      /*
       * But don't let the search entry focus. VIM let's us just keep hitting
       * '#' over and over without any intervention, and that's a useful
       * feature.
       */
      gtk_widget_grab_focus (GTK_WIDGET (vim->priv->text_view));

      /*
       * TODO: It would be nice to clear the selection to match closer to
       *       VIM. However, we do a delayed selection of the match in the
       *       editor tab (which eventually needs to search asynchronously
       *       too). So we need a better way to do this.
       */
#if 0
      gb_editor_vim_clear_selection (vim);
#endif
    }
}

static void
gb_editor_vim_move_to_line_n (GbEditorVim *vim,
                              guint        line)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  gtk_text_buffer_get_iter_at_line (buffer, &iter, line);
  gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_view_scroll_mark_onscreen (vim->priv->text_view, insert);
}

static void
gb_editor_vim_page_up (GbEditorVim *vim)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  g_signal_emit_by_name (vim->priv->text_view,
                         "move-cursor",
                         GTK_MOVEMENT_PAGES,
                         -1,
                         FALSE);
}

static void
gb_editor_vim_page_down (GbEditorVim *vim)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  g_signal_emit_by_name (vim->priv->text_view,
                         "move-cursor",
                         GTK_MOVEMENT_PAGES,
                         1,
                         FALSE);
}

static gboolean
gb_editor_vim_get_has_selection (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  return gtk_text_buffer_get_has_selection (buffer);
}

static void
gb_editor_vim_indent (GbEditorVim *vim)
{
  GbSourceView *view;

  g_assert (GB_IS_EDITOR_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  view = GB_SOURCE_VIEW (vim->priv->text_view);

  if (gb_editor_vim_get_has_selection (vim))
    gb_source_view_indent_selection (view);
}

static void
gb_editor_vim_unindent (GbEditorVim *vim)
{
  GbSourceView *view;

  g_assert (GB_IS_EDITOR_VIM (vim));

  if (!GB_IS_SOURCE_VIEW (vim->priv->text_view))
    return;

  view = GB_SOURCE_VIEW (vim->priv->text_view);

  if (gb_editor_vim_get_has_selection (vim))
    gb_source_view_unindent_selection (view);
}

static void
gb_editor_vim_clear_phrase (GbEditorVim *vim)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  g_string_truncate (vim->priv->phrase, 0);

#if 0
  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_PHRASE]);
#endif
}

static GbEditorVimPhraseStatus
gb_editor_vim_parse_phrase (GbEditorVim       *vim,
                            GbEditorVimPhrase *phrase)
{
  const gchar *str;
  guint count = 0;
  gchar key;
  gchar modifier;
  gint n_scanned;

  g_assert (GB_IS_EDITOR_VIM (vim));
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

      return GB_EDITOR_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 2)
    {
      phrase->count = count;
      phrase->key = key;
      phrase->modifier = 0;

      return GB_EDITOR_VIM_PHRASE_SUCCESS;
    }

  /* Special case for "0" command. */
  if ((n_scanned == 1) && (count == 0))
    {
      phrase->key = '0';
      phrase->count = 0;
      phrase->modifier = 0;

      return GB_EDITOR_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 1)
    return GB_EDITOR_VIM_PHRASE_NEED_MORE;

  n_scanned = sscanf (str, "%c%c", &key, &modifier);

  if (n_scanned == 2)
    {
      phrase->count = 0;
      phrase->key = key;
      phrase->modifier = modifier;

      return GB_EDITOR_VIM_PHRASE_SUCCESS;
    }

  if (n_scanned == 1)
    {
      phrase->count = 0;
      phrase->key = key;
      phrase->modifier = 0;

      return GB_EDITOR_VIM_PHRASE_SUCCESS;
    }

  return GB_EDITOR_VIM_PHRASE_FAILED;
}

static gboolean
gb_editor_vim_handle_normal (GbEditorVim *vim,
                             GdkEventKey *event)
{
  GbEditorVimCommand *cmd;
  GbEditorVimPhraseStatus status;
  GbEditorVimPhrase phrase;

  g_assert (GB_IS_EDITOR_VIM (vim));
  g_assert (event);

  switch (event->keyval)
    {
    case GDK_KEY_bracketleft:
      if ((event->state & GDK_CONTROL_MASK) == 0)
        break;
      /* Fall through */
    case GDK_KEY_Escape:
      gb_editor_vim_clear_selection (vim);
      gb_editor_vim_clear_phrase (vim);
      return TRUE;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      gb_editor_vim_clear_phrase (vim);
      gb_editor_vim_move_down (vim);
      return TRUE;

    case GDK_KEY_colon:
      if (!vim->priv->phrase->len)
        {
          gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_COMMAND);
          return TRUE;
        }
      break;

    case GDK_KEY_b:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_editor_vim_clear_phrase (vim);
          gb_editor_vim_page_up (vim);
          return TRUE;
        }
      break;

    case GDK_KEY_f:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_editor_vim_clear_phrase (vim);
          gb_editor_vim_page_down (vim);
          return TRUE;
        }
      break;

    case GDK_KEY_r:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_editor_vim_clear_phrase (vim);
          gb_editor_vim_redo (vim);
          return TRUE;
        }
      break;

    case GDK_KEY_u:
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_editor_vim_clear_phrase (vim);
          gb_editor_vim_clear_selection (vim);
          gb_editor_vim_select_char (vim);
          gb_editor_vim_move_line_start (vim);
          gb_editor_vim_delete_selection (vim);
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

  if (!gb_str_empty0 (event->string))
    g_string_append (vim->priv->phrase, event->string);

  status = gb_editor_vim_parse_phrase (vim, &phrase);

  switch (status)
    {
    case GB_EDITOR_VIM_PHRASE_SUCCESS:
      cmd = g_hash_table_lookup (gCommands, GINT_TO_POINTER (phrase.key));
      if (!cmd)
        {
          gb_editor_vim_clear_phrase (vim);
          break;
        }
      if (cmd->requires_modifier && !phrase.modifier)
        break;
      gb_editor_vim_clear_phrase (vim);
      cmd->func (vim, phrase.count, phrase.modifier);
      break;

    case GB_EDITOR_VIM_PHRASE_NEED_MORE:
      break;

    default:
    case GB_EDITOR_VIM_PHRASE_FAILED:
      gb_editor_vim_clear_phrase (vim);
      break;
    }

  return TRUE;
}

static gboolean
gb_editor_vim_handle_insert (GbEditorVim *vim,
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
      gb_editor_vim_move_backward (vim);
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);
      return TRUE;

    case GDK_KEY_u:
      /*
       * Delete everything before the cursor upon <Control>U.
       */
      if ((event->state & GDK_CONTROL_MASK) != 0)
        {
          gb_editor_vim_delete_to_line_start (vim);
          return TRUE;
        }

      break;

    default:
      break;
    }

  return FALSE;
}

static gboolean
gb_editor_vim_handle_command (GbEditorVim *vim,
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
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);
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
gb_editor_vim_key_press_event_cb (GtkTextView *text_view,
                                  GdkEventKey *event,
                                  GbEditorVim *vim)
{
  gboolean ret;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);

  switch (vim->priv->mode)
    {
    case GB_EDITOR_VIM_NORMAL:
      ret = gb_editor_vim_handle_normal (vim, event);
      break;

    case GB_EDITOR_VIM_INSERT:
      ret = gb_editor_vim_handle_insert (vim, event);
      break;

    case GB_EDITOR_VIM_COMMAND:
      ret = gb_editor_vim_handle_command (vim, event);
      break;

    default:
      g_assert_not_reached();
    }

  return ret;
}

static gboolean
gb_editor_vim_focus_in_event_cb (GtkTextView *text_view,
                                 GdkEvent    *event,
                                 GbEditorVim *vim)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);

  if (vim->priv->mode == GB_EDITOR_VIM_COMMAND)
    gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);

  return FALSE;
}

static void
gb_editor_vim_mark_set_cb (GtkTextBuffer *buffer,
                           GtkTextIter   *iter,
                           GtkTextMark   *mark,
                           GbEditorVim   *vim)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter);
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  if (vim->priv->mode == GB_EDITOR_VIM_INSERT)
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
gb_editor_vim_delete_range_cb (GtkTextBuffer *buffer,
                               GtkTextIter   *begin,
                               GtkTextIter   *end,
                               GbEditorVim   *vim)
{
  GtkTextIter iter;
  GtkTextMark *insert;
  guint line;
  guint end_line;
  guint begin_line;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (begin);
  g_return_if_fail (end);
  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  if (vim->priv->mode == GB_EDITOR_VIM_INSERT)
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
        gb_editor_vim_move_line_end (vim);
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
gb_editor_vim_sort (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter cursor;
  guint cursor_offset;
  gchar *text;
  gchar **parts;

  g_assert (GB_IS_EDITOR_VIM (vim));

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
gb_editor_vim_connect (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));
  g_return_if_fail (!vim->priv->connected);

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);

  vim->priv->key_press_event_handler =
    g_signal_connect (vim->priv->text_view,
                      "key-press-event",
                      G_CALLBACK (gb_editor_vim_key_press_event_cb),
                      vim);

  vim->priv->focus_in_event_handler =
    g_signal_connect (vim->priv->text_view,
                      "focus-in-event",
                      G_CALLBACK (gb_editor_vim_focus_in_event_cb),
                      vim);

  vim->priv->mark_set_handler =
    g_signal_connect (buffer,
                      "mark-set",
                      G_CALLBACK (gb_editor_vim_mark_set_cb),
                      vim);

  vim->priv->delete_range_handler =
    g_signal_connect_after (buffer,
                            "delete-range",
                            G_CALLBACK (gb_editor_vim_delete_range_cb),
                            vim);

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);

  vim->priv->connected = TRUE;
}

static void
gb_editor_vim_disconnect (GbEditorVim *vim)
{
  g_return_if_fail (GB_IS_EDITOR_VIM (vim));
  g_return_if_fail (vim->priv->connected);

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

  vim->priv->connected = FALSE;
}

gboolean
gb_editor_vim_get_enabled (GbEditorVim *vim)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);

  return vim->priv->enabled;
}

void
gb_editor_vim_set_enabled (GbEditorVim *vim,
                           gboolean     enabled)
{
  GbEditorVimPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  priv = vim->priv;

  if (priv->enabled == enabled)
    return;

  if (enabled)
    {
      gb_editor_vim_connect (vim);
      priv->enabled = TRUE;
    }
  else
    {
      gb_editor_vim_disconnect (vim);
      priv->enabled = FALSE;
    }

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_ENABLED]);
}

GtkWidget *
gb_editor_vim_get_text_view (GbEditorVim *vim)
{
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), NULL);

  return (GtkWidget *)vim->priv->text_view;
}

static void
gb_editor_vim_set_text_view (GbEditorVim *vim,
                             GtkTextView *text_view)
{
  GbEditorVimPrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

  priv = vim->priv;

  if (priv->text_view == text_view)
    return;

  if (priv->text_view)
    {
      if (priv->enabled)
        gb_editor_vim_disconnect (vim);
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
        gb_editor_vim_connect (vim);
    }

  g_object_notify_by_pspec (G_OBJECT (vim), gParamSpecs [PROP_TEXT_VIEW]);
}

void
gb_editor_vim_execute_command (GbEditorVim *vim,
                               const gchar *command)
{
  gchar *copy;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));
  g_return_if_fail (command);

  copy = g_strstrip (g_strdup (command));

  if (g_str_equal (copy, "sort"))
    gb_editor_vim_sort (vim);
  else
    g_debug (" TODO: Command Execution Support: %s", command);

  gb_editor_vim_clear_selection (vim);
  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);
  g_free (copy);
}

static void
gb_editor_vim_finalize (GObject *object)
{
  GbEditorVimPrivate *priv = GB_EDITOR_VIM (object)->priv;

  if (priv->text_view)
    {
      gb_editor_vim_disconnect (GB_EDITOR_VIM (object));
      g_object_remove_weak_pointer (G_OBJECT (priv->text_view),
                                    (gpointer *)&priv->text_view);
      priv->text_view = NULL;
    }

  g_string_free (priv->phrase, TRUE);
  priv->phrase = NULL;

  G_OBJECT_CLASS (gb_editor_vim_parent_class)->finalize (object);
}

static void
gb_editor_vim_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GbEditorVim *vim = GB_EDITOR_VIM (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      g_value_set_boolean (value, gb_editor_vim_get_enabled (vim));
      break;

    case PROP_MODE:
      g_value_set_enum (value, gb_editor_vim_get_mode (vim));
      break;

    case PROP_TEXT_VIEW:
      g_value_set_object (value, gb_editor_vim_get_text_view (vim));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_vim_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GbEditorVim *vim = GB_EDITOR_VIM (object);

  switch (prop_id)
    {
    case PROP_ENABLED:
      gb_editor_vim_set_enabled (vim, g_value_get_boolean (value));
      break;

    case PROP_TEXT_VIEW:
      gb_editor_vim_set_text_view (vim, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_vim_cmd_repeat (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  /* TODO! */
}

static void
gb_editor_vim_cmd_begin_search (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  if (GB_IS_SOURCE_VIEW (vim->priv->text_view))
    gb_source_view_begin_search (GB_SOURCE_VIEW (vim->priv->text_view),
                                 GTK_DIR_DOWN, NULL);
}

static void
gb_editor_vim_cmd_forward_line_end (GbEditorVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_move_line_end (vim);
}

static void
gb_editor_vim_cmd_backward_0 (GbEditorVim *vim,
                              guint        count,
                              gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_move_line0 (vim);
}

static void
gb_editor_vim_cmd_backward_start (GbEditorVim *vim,
                                  guint        count,
                                  gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_move_line_start (vim);
}

static void
gb_editor_vim_cmd_match_backward (GbEditorVim *vim,
                                  guint        count,
                                  gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_reverse_search (vim);
}

static void
gb_editor_vim_cmd_match_forward (GbEditorVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_search (vim);
}

static void
gb_editor_vim_cmd_indent (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_indent (vim);

  gb_editor_vim_clear_selection (vim);
}

static void
gb_editor_vim_cmd_unindent (GbEditorVim *vim,
                            guint        count,
                            gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_unindent (vim);

  gb_editor_vim_clear_selection (vim);
}

static void
gb_editor_vim_cmd_insert_end (GbEditorVim *vim,
                              guint        count,
                              gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_clear_selection (vim);
  gb_editor_vim_move_line_end (vim);
}

static void
gb_editor_vim_cmd_insert_after (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_clear_selection (vim);
  gb_editor_vim_move_forward (vim);
}

static void
gb_editor_vim_cmd_backward_word (GbEditorVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_backward_word (vim);
}

static void
gb_editor_vim_cmd_delete (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_clear_selection (vim);

  if (modifier == 'd')
    gb_editor_vim_select_line (vim);
  else
    {
      GbEditorVimCommand *cmd;

      cmd = g_hash_table_lookup (gCommands, GINT_TO_POINTER (modifier));
      if (!cmd || (cmd->type != GB_EDITOR_VIM_COMMAND_MOVEMENT))
        return;

      gb_editor_vim_select_char (vim);
      cmd->func (vim, count, '\0');
    }

  gb_editor_vim_delete_selection (vim);
}

static void
gb_editor_vim_cmd_delete_to_end (GbEditorVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_clear_selection (vim);
  gb_editor_vim_select_char (vim);
  gb_editor_vim_move_line_end (vim);
  gb_editor_vim_delete_selection (vim);
}

static void
gb_editor_vim_cmd_forward_word_end (GbEditorVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_end_of_word (vim);
}

static void
gb_editor_vim_cmd_goto (GbEditorVim *vim,
                        guint        count,
                        gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

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
      gb_editor_vim_clear_selection (vim);
      gb_editor_vim_move_to_line_n (vim, 0);
      break;

    default:
      break;
    }
}

static void
gb_editor_vim_cmd_goto_line (GbEditorVim *vim,
                             guint        count,
                             gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  if (count)
    gb_editor_vim_move_to_line_n (vim, count - 1);
  else
    gb_editor_vim_move_to_end (vim);
}

static void
gb_editor_vim_cmd_move_backward (GbEditorVim *vim,
                                 guint        count,
                                 gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_backward (vim);
}

static void
gb_editor_vim_cmd_insert_start (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_clear_selection (vim);
  gb_editor_vim_move_line_start (vim);
}

static void
gb_editor_vim_cmd_insert (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_clear_selection (vim);
}

static void
gb_editor_vim_cmd_move_down (GbEditorVim *vim,
                             guint        count,
                             gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_down (vim);
}

static void
gb_editor_vim_cmd_move_up (GbEditorVim *vim,
                           guint        count,
                           gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_up (vim);
}

static void
gb_editor_vim_cmd_move_forward (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_forward (vim);
}

static void
gb_editor_vim_cmd_insert_before_line (GbEditorVim *vim,
                                      guint        count,
                                      gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_insert_nl_before (vim);
}

static void
gb_editor_vim_cmd_insert_after_line (GbEditorVim *vim,
                                     guint        count,
                                     gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_insert_nl_after (vim, TRUE);
}

static void
gb_editor_vim_cmd_paste_after (GbEditorVim *vim,
                               guint        count,
                               gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_paste (vim);
}

static void
gb_editor_vim_cmd_paste_before (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  /* TODO: Paste Before intead of after. */
  gb_editor_vim_cmd_paste_after (vim, count, modifier);
}

static void
gb_editor_vim_cmd_overwrite (GbEditorVim *vim,
                             guint        count,
                             gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gtk_text_view_set_overwrite (vim->priv->text_view, TRUE);
}

static void
gb_editor_vim_cmd_undo (GbEditorVim *vim,
                        guint        count,
                        gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_undo (vim);
}

static void
gb_editor_vim_cmd_select_line (GbEditorVim *vim,
                               guint        count,
                               gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  gb_editor_vim_select_line (vim);
  for (i = 1; i < count; i++)
    gb_editor_vim_move_down (vim);
}

static void
gb_editor_vim_cmd_select (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
  gb_editor_vim_select_char (vim);
  for (i = 1; i < count; i++)
    gb_editor_vim_move_forward (vim);
  gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_NORMAL);
}

static void
gb_editor_vim_cmd_forward_word (GbEditorVim *vim,
                                guint        count,
                                gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_move_forward_word (vim);
}

static void
gb_editor_vim_cmd_delete_selection (GbEditorVim *vim,
                                    guint        count,
                                    gchar        modifier)
{
  guint i;

  g_assert (GB_IS_EDITOR_VIM (vim));

  count = MAX (1, count);

  for (i = 0; i < count; i++)
    gb_editor_vim_delete_selection (vim);
}

static void
gb_editor_vim_cmd_yank (GbEditorVim *vim,
                        guint        count,
                        gchar        modifier)
{
  g_assert (GB_IS_EDITOR_VIM (vim));

  gb_editor_vim_clear_selection (vim);

  if (modifier == 'y')
    gb_editor_vim_select_line (vim);
  else
    {
      GbEditorVimCommand *cmd;

      cmd = g_hash_table_lookup (gCommands, GINT_TO_POINTER (modifier));
      if (!cmd || (cmd->type != GB_EDITOR_VIM_COMMAND_MOVEMENT))
        return;

      cmd->func (vim, 1, '\0');
    }

  gb_editor_vim_yank (vim);
  gb_editor_vim_clear_selection (vim);
}

static void
gb_editor_vim_cmd_center (GbEditorVim *vim,
                          guint        count,
                          gchar        modifier)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  switch (modifier)
    {
    case 'b':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.0, 1.0);
      break;

    case 't':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.0, 0.0);
      break;

    case 'z':
      gtk_text_view_scroll_to_iter (vim->priv->text_view, &iter, 0.0, TRUE,
                                    0.0, 0.5);
      break;

    default:
      break;
    }
}

static void
gb_editor_vim_class_register_command (GbEditorVimClass       *klass,
                                      gchar                   key,
                                      gboolean                requires_modifier,
                                      GbEditorVimCommandType  type,
                                      GbEditorVimCommandFunc  func)
{
  GbEditorVimCommand *cmd;
  gpointer keyptr = GINT_TO_POINTER ((gint)key);

  g_assert (GB_IS_EDITOR_VIM_CLASS (klass));

  /*
   * TODO: It would be neat to have gCommands be a field in the klass. We
   *       could then just chain up to discover the proper command. This
   *       allows for subclasses to override and add new commands.
   *       To do so will probably take registering the GObjectClass
   *       manually to set base_init().
   */

  if (!gCommands)
    gCommands = g_hash_table_new (g_direct_hash, g_direct_equal);

  cmd = g_new0 (GbEditorVimCommand, 1);
  cmd->type = type;
  cmd->key = key;
  cmd->func = func;
  cmd->requires_modifier = requires_modifier;

  g_hash_table_replace (gCommands, keyptr, cmd);
}

static void
gb_editor_vim_class_init (GbEditorVimClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = gb_editor_vim_finalize;
  object_class->get_property = gb_editor_vim_get_property;
  object_class->set_property = gb_editor_vim_set_property;

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
                       GB_TYPE_EDITOR_VIM_MODE,
                       GB_EDITOR_VIM_NORMAL,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_MODE,
                                   gParamSpecs [PROP_MODE]);

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
   * GbEditorVim::command-visibility-toggled:
   * @visible: If the the command entry should be visible.
   *
   * The "command-visibility-toggled" signal is emitted when the command entry
   * should be shown or hidden. The command entry is used to interact with the
   * VIM style command line.
   */
  gSignals [COMMAND_VISIBILITY_TOGGLED] =
    g_signal_new ("command-visibility-toggled",
                  GB_TYPE_EDITOR_VIM,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_BOOLEAN);

  /*
   * Register all of our internal VIM commands. These can be used directly
   * or via phrases.
   */
  gb_editor_vim_class_register_command (klass, '.', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_repeat);
  gb_editor_vim_class_register_command (klass, '/', FALSE,
                                        GB_EDITOR_VIM_COMMAND_JUMP,
                                        gb_editor_vim_cmd_begin_search);
  gb_editor_vim_class_register_command (klass, '$', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_forward_line_end);
  gb_editor_vim_class_register_command (klass, '0', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_backward_0);
  gb_editor_vim_class_register_command (klass, '^', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_backward_start);
  gb_editor_vim_class_register_command (klass, '#', FALSE,
                                        GB_EDITOR_VIM_COMMAND_JUMP,
                                        gb_editor_vim_cmd_match_backward);
  gb_editor_vim_class_register_command (klass, '*', FALSE,
                                        GB_EDITOR_VIM_COMMAND_JUMP,
                                        gb_editor_vim_cmd_match_forward);
  gb_editor_vim_class_register_command (klass, '>', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_indent);
  gb_editor_vim_class_register_command (klass, '<', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_unindent);
  gb_editor_vim_class_register_command (klass, 'A', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert_end);
  gb_editor_vim_class_register_command (klass, 'a', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert_after);
  gb_editor_vim_class_register_command (klass, 'b', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_backward_word);
  gb_editor_vim_class_register_command (klass, 'd', TRUE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_delete);
  gb_editor_vim_class_register_command (klass, 'D', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_delete_to_end);
  gb_editor_vim_class_register_command (klass, 'e', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_forward_word_end);
  gb_editor_vim_class_register_command (klass, 'G', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_goto_line);
  gb_editor_vim_class_register_command (klass, 'g', TRUE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_goto);
  gb_editor_vim_class_register_command (klass, 'h', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_move_backward);
  gb_editor_vim_class_register_command (klass, 'I', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert_start);
  gb_editor_vim_class_register_command (klass, 'i', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert);
  gb_editor_vim_class_register_command (klass, 'j', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_move_down);
  gb_editor_vim_class_register_command (klass, 'k', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_move_up);
  gb_editor_vim_class_register_command (klass, 'l', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_move_forward);
  gb_editor_vim_class_register_command (klass, 'O', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert_before_line);
  gb_editor_vim_class_register_command (klass, 'o', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_insert_after_line);
  gb_editor_vim_class_register_command (klass, 'P', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_paste_before);
  gb_editor_vim_class_register_command (klass, 'p', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_paste_after);
  gb_editor_vim_class_register_command (klass, 'R', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_overwrite);
  gb_editor_vim_class_register_command (klass, 'u', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_undo);
  gb_editor_vim_class_register_command (klass, 'V', FALSE,
                                        GB_EDITOR_VIM_COMMAND_NOOP,
                                        gb_editor_vim_cmd_select_line);
  gb_editor_vim_class_register_command (klass, 'v', FALSE,
                                        GB_EDITOR_VIM_COMMAND_NOOP,
                                        gb_editor_vim_cmd_select);
  gb_editor_vim_class_register_command (klass, 'w', FALSE,
                                        GB_EDITOR_VIM_COMMAND_MOVEMENT,
                                        gb_editor_vim_cmd_forward_word);
  gb_editor_vim_class_register_command (klass, 'x', FALSE,
                                        GB_EDITOR_VIM_COMMAND_CHANGE,
                                        gb_editor_vim_cmd_delete_selection);
  gb_editor_vim_class_register_command (klass, 'y', TRUE,
                                        GB_EDITOR_VIM_COMMAND_NOOP,
                                        gb_editor_vim_cmd_yank);
  gb_editor_vim_class_register_command (klass, 'z', TRUE,
                                        GB_EDITOR_VIM_COMMAND_NOOP,
                                        gb_editor_vim_cmd_center);
}

static void
gb_editor_vim_init (GbEditorVim *vim)
{
  vim->priv = gb_editor_vim_get_instance_private (vim);
  vim->priv->enabled = FALSE;
  vim->priv->mode = GB_EDITOR_VIM_NORMAL;
  vim->priv->phrase = g_string_new (NULL);
}

GType
gb_editor_vim_mode_get_type (void)
{
  static GType type_id;
  static const GEnumValue values[] = {
    { GB_EDITOR_VIM_NORMAL, "GB_EDITOR_VIM_NORMAL", "NORMAL" },
    { GB_EDITOR_VIM_INSERT, "GB_EDITOR_VIM_INSERT", "INSERT" },
    { GB_EDITOR_VIM_COMMAND, "GB_EDITOR_VIM_COMMAND", "COMMAND" },
    { 0 }
  };

  if (!type_id)
    type_id = g_enum_register_static ("GbEditorVimMode", values);

  return type_id;
}
