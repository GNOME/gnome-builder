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
#include <stdlib.h>

#include "gb-editor-vim.h"
#include "gb-log.h"
#include "gb-source-view.h"

struct _GbEditorVimPrivate
{
  GtkTextView     *text_view;
  GbEditorVimMode  mode;
  gulong           key_press_event_handler;
  gulong           focus_in_event_handler;
  gulong           mark_set_handler;
  gulong           delete_range_handler;
  guint            target_line_offset;
  guint            enabled : 1;
  guint            connected : 1;
};

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
gb_editor_vim_move_line_start (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextIter selection;
  gboolean has_selection;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (vim->priv->text_view));
  has_selection = gb_editor_vim_get_selection_bounds (vim, &iter, &selection);

  gtk_text_buffer_get_iter_at_line (buffer, &iter,
                                    gtk_text_iter_get_line (&iter));

  while (!gtk_text_iter_ends_line (&iter) &&
         g_unichar_isspace (gtk_text_iter_get_char (&iter)))
    if (!gtk_text_iter_forward_char (&iter))
      break;

  if (has_selection)
    gb_editor_vim_select_range (vim, &iter, &selection);
  else
    gtk_text_buffer_select_range (buffer, &iter, &iter);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
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
  gtk_text_view_move_mark_onscreen (vim->priv->text_view, insert);
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
gb_editor_vim_delete_to_line_end (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
  gtk_text_iter_assign (&end, &begin);

  /*
   * Move forward to the end of the line, excluding the \n.
   */
  while (!gtk_text_iter_ends_line (&end))
    if (!gtk_text_iter_forward_char (&end))
      break;

  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_end_user_action (buffer);

  vim->priv->target_line_offset = gb_editor_vim_get_line_offset (vim);
}

static void
gb_editor_vim_delete_to_line_start (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter begin;
  GtkTextIter end;

  g_return_if_fail (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &begin, insert);
  gtk_text_iter_assign (&end, &begin);

  /*
   * Move backward to the start of the line. If we are at the start of a line
   * already, we actually just want to remove the \n.
   */
  if (!gtk_text_iter_starts_line (&begin))
    {
      while (!gtk_text_iter_starts_line (&begin))
        if (!gtk_text_iter_backward_char (&begin))
          break;
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

  gtk_text_view_move_mark_onscreen (vim->priv->text_view, insert);
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
       * And it also selects the word, and VIM does not (it just leaves us
       * on the word). So let's clear the selection too.
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
       * And it also selects the word, and VIM does not (it just leaves us
       * on the word). So let's clear the selection too.
       */
#if 0
      gb_editor_vim_clear_selection (vim);
#endif
    }
}

static gboolean
gb_editor_vim_get_has_selection (GbEditorVim *vim)
{
  GtkTextBuffer *buffer;

  g_assert (GB_IS_EDITOR_VIM (vim));

  buffer = gtk_text_view_get_buffer (vim->priv->text_view);
  return gtk_text_buffer_get_has_selection (buffer);
}

static gboolean
gb_editor_vim_handle_normal (GbEditorVim *vim,
                             GdkEventKey *event)
{
  g_assert (GB_IS_EDITOR_VIM (vim));
  g_assert (event);

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      /*
       * Escape any selections we currently have.
       */
      gb_editor_vim_clear_selection (vim);
      break;

    case GDK_KEY_e:
      /*
       * Move to the end of the current word if there is one. Otherwise
       * the end of the next word.
       */
      gb_editor_vim_move_end_of_word (vim);
      return TRUE;

    case GDK_KEY_I:
      /*
       * Start insert mode at the beginning of the line.
       */
      gb_editor_vim_move_line_start (vim);
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      return TRUE;

    case GDK_KEY_i:
      /*
       * Start insert mode at the current line position.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      return TRUE;

    case GDK_KEY_A:
      /*
       * Start insert mode at the end of the line.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gb_editor_vim_move_line_end (vim);
      return TRUE;

    case GDK_KEY_a:
      /*
       * Start insert mode after the current character.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gb_editor_vim_move_forward (vim);
      return TRUE;

    case GDK_KEY_D:
      /*
       * Delete from the current position to the end of the line.
       * Stay in NORMAL mode.
       */
      gb_editor_vim_delete_to_line_end (vim);
      return TRUE;

    case GDK_KEY_l:
      /*
       * Move forward in the buffer one character, but stay on the
       * same line.
       */
      gb_editor_vim_move_forward (vim);
      return TRUE;

    case GDK_KEY_G:
      /*
       * Move to the end of the buffer.
       */
      gb_editor_vim_move_to_end (vim);
      return TRUE;

    case GDK_KEY_h:
      /*
       * Move backward in the buffer one character, but stay on the
       * same line.
       */
      gb_editor_vim_move_backward (vim);
      return TRUE;

    case GDK_KEY_j:
      /*
       * Move down in the buffer one line, and try to stay on the same column.
       */
      gb_editor_vim_move_down (vim);
      return TRUE;

    case GDK_KEY_k:
      /*
       * Move down in the buffer one line, and try to stay on the same column.
       */
      gb_editor_vim_move_up (vim);
      return TRUE;

    case GDK_KEY_V:
      /*
       * Select the current line.
       */
      gb_editor_vim_select_line (vim);
      return TRUE;

    case GDK_KEY_v:
      /*
       * Advance the selection to the next character. This needs to be able
       * to be composted so things like 10v selectio the next 10 characters.
       * However, `vvvvvvvvvv` does not select the next 10 characters.
       */
      gb_editor_vim_select_char (vim);
      return TRUE;

    case GDK_KEY_w:
      /*
       * Move forward by one word.
       */
      gb_editor_vim_move_forward_word (vim);
      return TRUE;

    case GDK_KEY_b:
      /*
       * Move backward by one word.
       */
      gb_editor_vim_move_backward_word (vim);
      return TRUE;

    case GDK_KEY_x:
      /*
       * Delete the current selection.
       */
      gb_editor_vim_delete_selection (vim);
      break;

    case GDK_KEY_u:
      /*
       * Undo the last operation if we can.
       */
      gb_editor_vim_undo (vim);
      break;

    case GDK_KEY_O:
      /*
       * Insert a newline before the current line, and start editing.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gb_editor_vim_insert_nl_before (vim);
      return TRUE;

    case GDK_KEY_o:
      /*
       * Insert a new line, and then begin insertion.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gb_editor_vim_insert_nl_after (vim, TRUE);
      return TRUE;

    case GDK_KEY_p:
      /*
       * Paste the current clipboard selection.
       */
      gb_editor_vim_paste (vim);
      return TRUE;

    case GDK_KEY_r:
      /*
       * Try to redo a previously undone operation if we can.
       */
      if ((event->state & GDK_CONTROL_MASK))
        {
          gb_editor_vim_redo (vim);
          return TRUE;
        }

      break;

    case GDK_KEY_R:
      /*
       * Go into insert mode with overwrite.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_INSERT);
      gtk_text_view_set_overwrite (vim->priv->text_view, TRUE);
      return TRUE;

    case GDK_KEY_y:
      /*
       * Yank (copy) the current selection and then remove the selection
       * leaving the cursor on the first character.
       */
      if (gb_editor_vim_get_has_selection (vim))
        {
          gb_editor_vim_yank (vim);
          return TRUE;
        }

        break;

    case GDK_KEY_greater:
      /*
       * If we have a selection, try to indent it.
       */
      if (gb_editor_vim_get_has_selection (vim) &&
          GB_IS_SOURCE_VIEW (vim->priv->text_view))
        {
          GbSourceView *view = GB_SOURCE_VIEW (vim->priv->text_view);
          gb_source_view_indent_selection (view);
          return TRUE;
        }

      break;

    case GDK_KEY_less:
      /*
       * If we have a selection, try to unindent it.
       */
      if (gb_editor_vim_get_has_selection (vim) &&
          GB_IS_SOURCE_VIEW (vim->priv->text_view))
        {
          GbSourceView *view = GB_SOURCE_VIEW (vim->priv->text_view);
          gb_source_view_unindent_selection (view);
          return TRUE;
        }

      break;

    case GDK_KEY_slash:
      /*
       * Focus the search entry for the source view and clear the current
       * search. It would be nice to not clear the current search, but
       * the focus/editable selection process is being a bit annoying.
       */
      if (GB_IS_SOURCE_VIEW (vim->priv->text_view))
        {
          gb_source_view_begin_search (GB_SOURCE_VIEW (vim->priv->text_view),
                                       GTK_DIR_DOWN, "");
          return TRUE;
        }

      break;

    case GDK_KEY_dollar:
      /*
       * Move to the end of the line.
       */
      gb_editor_vim_move_line_end (vim);
      return TRUE;

    case GDK_KEY_0:
      /*
       * Move to the first offset (even if it is whitespace) on the current
       * line.
       */
      gb_editor_vim_move_line0 (vim);
      return TRUE;

    case GDK_KEY_asciicircum:
      /*
       * Move to the first word in the line.
       */
      gb_editor_vim_move_line_start (vim);
      return TRUE;

    case GDK_KEY_asterisk:
      /*
       * Start search in the forward direction for the word that is under
       * the cursor. If we are over a space, we move ot the next word.
       */
      gb_editor_vim_search (vim);
      return TRUE;

    case GDK_KEY_numbersign:
      /*
       * Start searching in the reverse direction for the word that is
       * under the cursor. If we are over a space, we move to the next
       * word.
       */
      gb_editor_vim_reverse_search (vim);
      return TRUE;

    case GDK_KEY_colon:
      /*
       * Switch to command mode.
       */
      gb_editor_vim_set_mode (vim, GB_EDITOR_VIM_COMMAND);
      return TRUE;

    default:
      break;
    }

  gtk_bindings_activate_event (G_OBJECT (vim->priv->text_view), event);

  return TRUE;
}

static gboolean
gb_editor_vim_handle_insert (GbEditorVim *vim,
                             GdkEventKey *event)
{
  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      /*
       * Escape back into NORMAL mode.
       */
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

  ENTRY;

  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);

  switch (vim->priv->mode)
    {
    case GB_EDITOR_VIM_NORMAL:
      ret = gb_editor_vim_handle_normal (vim, event);
      RETURN (ret);

    case GB_EDITOR_VIM_INSERT:
      ret = gb_editor_vim_handle_insert (vim, event);
      RETURN (ret);

    case GB_EDITOR_VIM_COMMAND:
      ret = gb_editor_vim_handle_command (vim, event);
      RETURN (ret);

    default:
      g_assert_not_reached();
    }

  RETURN (FALSE);
}

static gboolean
gb_editor_vim_focus_in_event_cb (GtkTextView *text_view,
                                 GdkEvent    *event,
                                 GbEditorVim *vim)
{
  g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), FALSE);
  g_return_val_if_fail (event, FALSE);
  g_return_val_if_fail (GB_IS_EDITOR_VIM (vim), FALSE);

  if (vim->priv->mode != GB_EDITOR_VIM_NORMAL)
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
}

static void
gb_editor_vim_init (GbEditorVim *vim)
{
  vim->priv = gb_editor_vim_get_instance_private (vim);
  vim->priv->enabled = FALSE;
  vim->priv->mode = GB_EDITOR_VIM_NORMAL;
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
