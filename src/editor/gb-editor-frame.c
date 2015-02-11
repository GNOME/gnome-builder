/* gb-editor-frame.c
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

#define G_LOG_DOMAIN "editor-frame"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gb-editor-frame.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-workspace.h"
#include "gb-gtk.h"
#include "gb-log.h"
#include "gb-source-formatter.h"
#include "gb-string.h"
#include "gb-widget.h"
#include "gb-workbench.h"

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorFrame, gb_editor_frame, GTK_TYPE_OVERLAY)

enum {
  PROP_0,
  PROP_DOCUMENT,
  PROP_SEARCH_DIRECTION,
  LAST_PROP
};

enum {
  FOCUSED,
  LAST_SIGNAL
};

static GParamSpec *gParamSpecs [LAST_PROP];
static guint       gSignals [LAST_SIGNAL];

static void gb_editor_frame_set_search_direction (GbEditorFrame    *self,
                                                  GtkDirectionType  search_direction);
GtkWidget *
gb_editor_frame_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_FRAME, NULL);
}

/**
 * gb_editor_frame_link:
 * @src: (in): The source frame.
 * @dst: (in): The destination frame.
 *
 * This function is intended to link two #GbEditorFrame instances to use the
 * same backend buffer. This is useful when you want two separate views of the
 * same content.
 */
void
gb_editor_frame_link (GbEditorFrame *src,
                      GbEditorFrame *dst)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (src));
  g_return_if_fail (GB_IS_EDITOR_FRAME (dst));

  g_object_bind_property (src, "document", dst, "document",
                          G_BINDING_SYNC_CREATE);
}

static void
gb_editor_frame_restore_position (GbEditorFrame *self)
{
  GtkTextView *text_view;
  GtkTextIter iter;
  GtkTextBuffer *buffer;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  text_view = GTK_TEXT_VIEW (self->priv->source_view);
  buffer = gtk_text_view_get_buffer (text_view);

  gb_gtk_text_buffer_get_iter_at_line_and_offset (buffer, &iter,
                                                  self->priv->saved_line,
                                                  self->priv->saved_line_offset);
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  if (!gb_gtk_text_view_get_iter_visible (text_view, &iter))
    gb_gtk_text_view_scroll_to_iter (text_view, &iter, 0.25, TRUE, 0.0, 0.5);
}

static void
gb_editor_frame_save_position (GbEditorFrame *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *insert;
  GtkTextIter iter;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  buffer = GTK_TEXT_BUFFER (self->priv->document);
  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

  self->priv->saved_line = gtk_text_iter_get_line (&iter);
  self->priv->saved_line_offset = gtk_text_iter_get_line_offset (&iter);
}

/**
 * gb_editor_frame_match:
 * @self: the #GbEditorFrame
 * @direction: the direction to search through the document
 * @rubberbanding: if %TRUE then use the match closest to the
 * position of cursor at the time gb_editor_frame_save_position()
 * was last called, if %FALSE then use the match closest to
 * the current cursor position.
 *
 * Move to a search match in the direction of @direction and/or
 * select the match.
 */
static void
gb_editor_frame_match (GbEditorFrame    *self,
                       GtkDirectionType  direction,
                       gboolean          rubberbanding)
{
  GbEditorFramePrivate *priv;
  GtkTextBuffer *buffer;
  GtkTextIter select_begin;
  GtkTextIter select_end;
  GtkTextIter match_begin;
  GtkTextIter match_end;
  gboolean has_selection;
  gboolean search_backward;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN);

  priv = self->priv;

  if (direction == GTK_DIR_UP)
    search_backward = TRUE;
  else if (direction == GTK_DIR_DOWN)
    search_backward = FALSE;
  else
    g_assert_not_reached ();

  gb_editor_frame_set_search_direction (self, direction);

  buffer = GTK_TEXT_BUFFER (priv->document);

  /*
   * Start by trying from our current location unless we are rubberbanding, then
   * start from our saved position.
   */
  if (rubberbanding)
    {
      gb_gtk_text_buffer_get_iter_at_line_and_offset (buffer, &select_begin,
                                                      priv->saved_line,
                                                      priv->saved_line_offset);
      select_end = select_begin;
    }
  else
    {
      has_selection = gtk_text_buffer_get_selection_bounds (buffer,
                                                            &select_begin,
                                                            &select_end);

      if (!has_selection)
        {
          if (!search_backward)
            {
              if (!gtk_text_iter_forward_char (&select_end))
                gtk_text_buffer_get_end_iter (buffer, &select_end);
            }
          else
            {
              if (!gtk_text_iter_backward_char (&select_begin))
                gtk_text_buffer_get_start_iter (buffer, &select_begin);
            }
        }
    }

  if (!search_backward)
    {
      if (gtk_source_search_context_forward (priv->search_context, &select_end,
                                             &match_begin, &match_end))
        GOTO (found_match);
    }
  else
    {
      if (gtk_source_search_context_backward (priv->search_context, &select_begin,
                                              &match_begin, &match_end))
        GOTO (found_match);
    }

  gb_editor_frame_restore_position (self);

  EXIT;

found_match:
  gb_source_view_jump_notify (priv->source_view);
  gb_source_view_clear_saved_cursor (priv->source_view);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (priv->document),
                                &match_begin, &match_end);

  if (!gb_gtk_text_view_get_iter_visible (GTK_TEXT_VIEW (priv->source_view),
                                          &match_end))
    gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view),
                                  &match_end, 0.0, TRUE, 1.0, 0.5);

  EXIT;
}

/**
 * gb_editor_frame_move_next_match:
 *
 * Move to the next search match after the cursor position.
 */
static void
gb_editor_frame_move_next_match (GbEditorFrame *self,
                                 gboolean       rubberbanding)
{
  gb_editor_frame_match (self, GTK_DIR_DOWN, rubberbanding);
}

/**
 * gb_editor_frame_move_previous_match:
 *
 * Move to the first match before the cursor position.
 */
static void
gb_editor_frame_move_previous_match (GbEditorFrame *self,
                                     gboolean       rubberbanding)
{
  gb_editor_frame_match (self, GTK_DIR_UP, rubberbanding);
}

static void
gb_editor_frame_set_position_label (GbEditorFrame *self,
                                    const gchar   *text)
{
  GbEditorFramePrivate *priv;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  priv = self->priv;

  if (!text || !*text)
    {
      if (priv->search_entry_tag)
        {
          gd_tagged_entry_remove_tag (priv->search_entry,
                                      priv->search_entry_tag);
          g_clear_object (&priv->search_entry_tag);
        }
      return;
    }

  if (!priv->search_entry_tag)
    {
      priv->search_entry_tag = gd_tagged_entry_tag_new ("");
      gd_tagged_entry_tag_set_style (priv->search_entry_tag,
                                     "gb-search-entry-occurrences-tag");
      gd_tagged_entry_add_tag (priv->search_entry,
                               priv->search_entry_tag);
    }

  gd_tagged_entry_tag_set_label (priv->search_entry_tag, text);
}

static void
gb_editor_frame_update_search_position_label (GbEditorFrame *self)
{
  GbEditorFramePrivate *priv;
  GtkStyleContext *context;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  gchar *text;
  gint count;
  gint pos;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  priv = self->priv;

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (priv->document),
                                        &begin, &end);
  pos = gtk_source_search_context_get_occurrence_position (
    priv->search_context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (
    priv->search_context);

  if ((pos == -1) || (count == -1))
    {
      /*
       * We are not yet done scanning the buffer.
       * We will be updated when we know more, so just hide it for now.
       */
      gb_editor_frame_set_position_label (self, NULL);
      return;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (priv->search_entry));
  search_text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));

  if ((count == 0) && !gb_str_empty0 (search_text))
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_ERROR);

  text = g_strdup_printf (_("%u of %u"), pos, count);
  gb_editor_frame_set_position_label (self, text);
  g_free (text);
}

static void
gb_editor_frame_on_search_occurrences_notify (GbEditorFrame          *self,
                                              GParamSpec             *pspec,
                                              GtkSourceSearchContext *search_context)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  gb_editor_frame_update_search_position_label (self);
}

void
gb_editor_frame_reformat (GbEditorFrame *self)
{
  GbEditorFramePrivate *priv;
  GbSourceFormatter *formatter;
  GtkSourceLanguage *language;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GtkTextIter iter;
  GtkTextMark *insert;
  gboolean fragment = TRUE;
  GError *error = NULL;
  gchar *input = NULL;
  gchar *output = NULL;
  guint line_number;
  guint char_offset;

  ENTRY;

  /*
   * TODO: Do this asynchronously, add tab state, propagate errors.
   */

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  priv = self->priv;

  buffer = GTK_TEXT_BUFFER (priv->document);

  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

  if (gtk_text_iter_compare (&begin, &end) == 0)
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &end);
      fragment = FALSE;
    }

  input = gtk_text_buffer_get_text (buffer, &begin, &end, TRUE);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  char_offset = gtk_text_iter_get_line_offset (&iter);
  line_number = gtk_text_iter_get_line (&iter);

  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  formatter = gb_source_formatter_new_from_language (language);

  if (!gb_source_formatter_format (formatter, input, fragment, NULL, &output,
                                   &error))
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      GOTO (cleanup);
    }

  gtk_text_buffer_begin_user_action (buffer);

  /* TODO: Keep the cursor on same CXCursor from Clang instead of the
   *       same character offset within the buffer. We probably want
   *       to defer this to the formatter API since it will be language
   *       specific.
   */

  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_insert (buffer, &begin, output, -1);

  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_bounds (buffer, &begin, &iter);
      goto select_range;
    }

  gtk_text_buffer_get_iter_at_line (buffer, &iter, line_number);
  gtk_text_iter_forward_to_line_end (&iter);

  if (gtk_text_iter_get_line (&iter) != line_number)
    gtk_text_iter_backward_char (&iter);
  else if (gtk_text_iter_get_line_offset (&iter) > char_offset)
    gtk_text_buffer_get_iter_at_line_offset (buffer, &iter, line_number, char_offset);

select_range:
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gtk_text_buffer_end_user_action (buffer);

  gb_gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->source_view), &iter,
                                   0.25, TRUE, 0.5, 0.5);

cleanup:
  g_free (input);
  g_free (output);
  g_clear_object (&formatter);

  EXIT;
}

/**
 * gb_editor_frame_on_cursor_moved:
 *
 * Update cursor ruler in the floating bar upon changing of insert text mark.
 */
static void
gb_editor_frame_on_cursor_moved (GbEditorFrame    *self,
                                 GbEditorDocument *document)
{
  GtkSourceView *source_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GtkTextMark *mark;
  gchar *text;
  guint ln;
  guint col;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  source_view = GTK_SOURCE_VIEW (self->priv->source_view);
  buffer = GTK_TEXT_BUFFER (document);

  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ln = gtk_text_iter_get_line (&iter);
  col = gtk_source_view_get_visual_column (source_view, &iter);

  text = g_strdup_printf (_("Line %u, Column %u"), ln + 1, col + 1);
  nautilus_floating_bar_set_primary_label (self->priv->floating_bar, text);
  g_free (text);

  gb_editor_frame_update_search_position_label (self);
}

static void
gb_editor_frame_on_file_mark_set (GbEditorFrame *self,
                                  GtkTextIter   *location,
                                  GtkTextBuffer *buffer)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (!gtk_widget_has_focus (GTK_WIDGET (self->priv->source_view)))
    return;

  gb_gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->priv->source_view),
                                   location, 0.0, TRUE, 0.5, 0.5);
}

static void
gb_editor_frame_document_saved (GbEditorFrame    *self,
                                GbEditorDocument *document)
{
  GdkWindow *window;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  window = gtk_text_view_get_window (GTK_TEXT_VIEW (self->priv->source_view),
                                     GTK_TEXT_WINDOW_WIDGET);
  gdk_window_invalidate_rect (window, NULL, TRUE);
}

/**
 * gb_editor_frame_connect:
 *
 * Attach to dynamic signals for the #GtkTextBuffer. Create any objects that
 * are dependent on the buffer.
 */
static void
gb_editor_frame_connect (GbEditorFrame    *self,
                         GbEditorDocument *document)
{
  GbEditorFramePrivate *priv;
  GbSourceChangeMonitor *monitor;
  GbSourceCodeAssistant *code_assistant;
  GtkTextIter iter;
  GtkTextMark *insert;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));
  g_return_if_fail (!self->priv->document);

  priv = self->priv;

  /*
   * Save the document for later.
   */
  priv->document = g_object_ref (document);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (priv->source_view),
                            GTK_TEXT_BUFFER (priv->document));

  /*
   * Look the saved signal so that we can invalidate the window afterwards.
   * This could happen since gutter content could change (like if it is a
   * new file in a git repo).
   */
  g_signal_connect_object (priv->document,
                           "saved",
                           G_CALLBACK (gb_editor_frame_document_saved),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Connect change monitor to gutter.
   */
  monitor = gb_editor_document_get_change_monitor (document);
  g_object_set (priv->diff_renderer,
                "change-monitor", monitor,
                NULL);

  /*
   * Connect code assistance to gutter and spinner.
   */
  code_assistant = gb_editor_document_get_code_assistant (document);
  g_object_set (priv->code_assistant_renderer,
                "code-assistant", code_assistant,
                NULL);
  g_object_bind_property (code_assistant, "active",
                          priv->busy_spinner, "active",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (code_assistant, "active",
                          priv->busy_spinner, "visible",
                          G_BINDING_SYNC_CREATE);

  /*
   * Don't allow editing if the buffer is read-only.
   */
  g_object_bind_property (priv->document, "read-only",
                          priv->source_view, "editable",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  /*
   * Create search defaults for this frame.
   */
  priv->search_context = g_object_new (GTK_SOURCE_TYPE_SEARCH_CONTEXT,
                                       "buffer", priv->document,
                                       "settings", priv->search_settings,
                                       "highlight", TRUE,
                                       NULL);
  g_object_set (priv->search_highlighter,
                "search-context", priv->search_context,
                NULL);

  g_signal_connect_object (priv->search_context,
                           "notify::occurrences-count",
                           G_CALLBACK (gb_editor_frame_on_search_occurrences_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->document,
                           "file-mark-set",
                           G_CALLBACK (gb_editor_frame_on_file_mark_set),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Connect to cursor-moved signal to update cursor position label.
   */
  if (GB_IS_EDITOR_DOCUMENT (priv->document))
    {
      priv->cursor_moved_handler =
        g_signal_connect_swapped (priv->document,
                                  "cursor-moved",
                                  G_CALLBACK (gb_editor_frame_on_cursor_moved),
                                  self);
    }

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (document));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (document), &iter, insert);
  gb_gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->priv->source_view),
                                   &iter, 0.0, TRUE, 0.5, 0.0);

  EXIT;
}

/**
 * gb_editor_frame_disconnect:
 *
 * Cleanup any signals or objects that are related to the #GtkTextBuffer.
 */
static void
gb_editor_frame_disconnect (GbEditorFrame *self)
{
  GbEditorFramePrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  priv = self->priv;

  if (priv->document)
    {
      g_signal_handler_disconnect (priv->document, priv->cursor_moved_handler);
      priv->cursor_moved_handler = 0;
    }

  g_object_set (priv->diff_renderer,
                "change-monitor", NULL,
                NULL);

  g_object_set (priv->code_assistant_renderer,
                "code-assistant", NULL,
                NULL);

  g_object_set (priv->search_highlighter,
                "search-context", NULL,
                NULL);

  g_clear_object (&priv->document);
  g_clear_object (&priv->search_context);

  EXIT;
}

/**
 * gb_editor_frame_get_document:
 *
 * Gets the #GbEditorDocument associated with the #GbEditorFrame.
 *
 * Returns: (transfer none): A #GbEditorDocument.
 */
GbEditorDocument *
gb_editor_frame_get_document (GbEditorFrame *self)
{
  g_return_val_if_fail (GB_IS_EDITOR_FRAME (self), NULL);

  return self->priv->document;
}

/**
 * gb_editor_frame_set_document:
 *
 * Set the #GbEditorDocument to be displayed by the #GbEditorFrame.
 */
void
gb_editor_frame_set_document (GbEditorFrame    *self,
                              GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  if (document != self->priv->document)
    {
      gb_editor_frame_disconnect (self);
      if (document)
        gb_editor_frame_connect (self, document);
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_DOCUMENT]);
    }
}

/**
 * gb_editor_frame_get_search_direction:
 * @self: a #GbEditorFrame
 *
 * Gets the #GtkDirectionType associated with the last search.
 * Will only be %GTK_DIR_DOWN or %GTK_DIR_UP
 *
 * Returns: A #GtkDirectionType.
 */
GtkDirectionType
gb_editor_frame_get_search_direction (GbEditorFrame *self)
{
  g_return_val_if_fail (GB_IS_EDITOR_FRAME (self), GTK_DIR_DOWN);

  return self->priv->search_direction;
}

static void
gb_editor_frame_set_search_direction (GbEditorFrame    *self,
                                      GtkDirectionType  search_direction)
{
  if (self->priv->search_direction == search_direction)
    return;

  self->priv->search_direction = search_direction;

  g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SEARCH_DIRECTION]);
}

/**
 * gb_editor_frame_on_focus_in_event:
 *
 * Handle the "focus-in-event" on the #GbSourceView. Ensure the search entry
 * is hidden and we are no longer highlighting search results.
 */
static gboolean
gb_editor_frame_on_focus_in_event (GbEditorFrame *self,
                                   GdkEvent      *event,
                                   GbSourceView  *source_view)
{
  g_return_val_if_fail (GB_IS_EDITOR_FRAME (self), FALSE);
  g_return_val_if_fail (GB_IS_SOURCE_VIEW (source_view), FALSE);

  if (gtk_revealer_get_reveal_child (self->priv->search_revealer))
    gtk_revealer_set_reveal_child (self->priv->search_revealer, FALSE);

  if (gtk_source_search_context_get_highlight (self->priv->search_context))
    gtk_source_search_context_set_highlight (self->priv->search_context, FALSE);

  gb_editor_document_check_externally_modified (self->priv->document);

  g_signal_emit (self, gSignals [FOCUSED], 0);

  return GDK_EVENT_PROPAGATE;
}

/**
 * gb_editor_frame_on_populate_popup:
 *
 * Update the popup menu to include choices for language highlight.
 */
static void
gb_editor_frame_on_populate_popup (GbEditorFrame *self,
                                   GtkWidget     *popup,
                                   GtkTextView   *text_view)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (GTK_IS_WIDGET (popup));

  /* TODO: Highlight Language Widget */
}

/**
 * gb_editor_frame_on_push_snippet:
 *
 * Update snippet context with the filename of the current document.
 */
static void
gb_editor_frame_on_push_snippet (GbEditorFrame          *self,
                                 GbSourceSnippet        *snippet,
                                 GbSourceSnippetContext *context,
                                 GtkTextIter            *iter,
                                 GbSourceView           *source_view)
{
  GtkSourceFile *source_file;
  GFile *file;

  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET (snippet));
  g_return_if_fail (GB_IS_SOURCE_SNIPPET_CONTEXT (context));
  g_return_if_fail (iter);
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  source_file = gb_editor_document_get_file (self->priv->document);
  file = gtk_source_file_get_location (source_file);
  g_assert (!file || G_IS_FILE (file));

  if (file)
    {
      gchar *name;

      name = g_file_get_basename (file);
      gb_source_snippet_context_add_variable (context, "filename", name);
      g_free (name);
    }
}

static gboolean
gb_editor_frame_on_search_entry_key_press (GbEditorFrame *self,
                                           GdkEventKey   *event,
                                           GdTaggedEntry *entry)
{
  gint begin;
  gint end;

  ENTRY;

  g_assert (GD_IS_TAGGED_ENTRY (entry));
  g_assert (GB_IS_EDITOR_FRAME (self));

  /*
   * WORKAROUND:
   *
   * There is some weird stuff going on with key-press when we have a selection.
   * We want to overwrite the text, but sometimes it doesn't. So we can just
   * force it if the string field is set.
   */
  if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &begin, &end) &&
      g_unichar_isprint (gdk_keyval_to_unicode (event->keyval)))
    {
      gtk_editable_delete_selection (GTK_EDITABLE (entry));
    }

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      gtk_revealer_set_reveal_child (self->priv->search_revealer, FALSE);
      gb_source_view_set_show_shadow (self->priv->source_view, FALSE);
      gb_editor_frame_restore_position (self);
      gtk_widget_grab_focus (GTK_WIDGET (self->priv->source_view));
      RETURN (GDK_EVENT_STOP);

    case GDK_KEY_Down:
      gb_editor_frame_move_next_match (self, FALSE);
      gb_editor_frame_save_position (self);
      RETURN (GDK_EVENT_STOP);

    case GDK_KEY_Up:
      gb_editor_frame_move_previous_match (self, FALSE);
      gb_editor_frame_save_position (self);
      RETURN (GDK_EVENT_STOP);

    default:
      break;
    }

  RETURN (GDK_EVENT_PROPAGATE);
}

static void
gb_editor_frame_on_search_entry_changed (GbEditorFrame *self,
                                         GtkEntry      *entry)
{
  const gchar *search_text;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GD_IS_TAGGED_ENTRY (entry));

  search_text = gtk_entry_get_text (entry);

  if (!gb_str_empty0 (search_text))
    {
      if (self->priv->search_direction == GTK_DIR_DOWN)
        gb_editor_frame_move_next_match (self, TRUE);
      else if (self->priv->search_direction == GTK_DIR_UP)
        gb_editor_frame_move_previous_match (self, TRUE);
      else
        g_assert_not_reached ();
    }
}

static void
gb_editor_frame_on_search_entry_activate (GbEditorFrame *self,
                                          GdTaggedEntry *entry)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  ENTRY;

  g_assert (GD_IS_TAGGED_ENTRY (entry));
  g_assert (GB_IS_EDITOR_FRAME (self));

  if (self->priv->search_direction == GTK_DIR_DOWN)
    gb_editor_frame_move_next_match (self, TRUE);
  else if (self->priv->search_direction == GTK_DIR_UP)
    gb_editor_frame_move_previous_match (self, TRUE);
  else
    g_assert_not_reached ();

  buffer = GTK_TEXT_BUFFER (self->priv->document);

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);

      if (gtk_text_iter_compare (&begin, &end) <= 0)
        gtk_text_buffer_select_range (buffer, &begin, &begin);
      else
        gtk_text_buffer_select_range (buffer, &end, &end);
    }

  gtk_widget_grab_focus (GTK_WIDGET (self->priv->source_view));

  EXIT;
}

static void
gb_editor_frame_on_forward_search_clicked (GbEditorFrame *self,
                                           GtkButton     *button)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GTK_IS_BUTTON (button));

  gb_editor_frame_move_next_match (self, FALSE);
}

static void
gb_editor_frame_on_backward_search_clicked (GbEditorFrame *self,
                                            GtkButton     *button)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GTK_IS_BUTTON (button));

  gb_editor_frame_move_previous_match (self, FALSE);
}

/**
 * gb_editor_frame_on_begin_search:
 *
 * Show the search machinery when a request to begin a search has occurred.
 */
static void
gb_editor_frame_on_begin_search (GbEditorFrame    *self,
                                 GtkDirectionType  direction,
                                 const gchar      *search_text,
                                 GbSourceView     *source_view)
{
  GbEditorFramePrivate *priv;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));

  priv = self->priv;

  gb_editor_frame_save_position (self);

  if (search_text)
    gtk_entry_set_text (GTK_ENTRY (priv->search_entry), search_text);

  gtk_revealer_set_reveal_child (priv->search_revealer, TRUE);
  gtk_source_search_context_set_highlight (priv->search_context, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (priv->search_entry));

  if (search_text)
    {
      if (direction == GTK_DIR_DOWN)
        gb_editor_frame_move_next_match (self, TRUE);
      else if (direction == GTK_DIR_UP)
        gb_editor_frame_move_previous_match (self, TRUE);
    }
  else
    {
      const gchar *text;
      guint len;

      if (direction == GTK_DIR_DOWN)
        gb_editor_frame_move_next_match (self, TRUE);
      else if (direction == GTK_DIR_UP)
        gb_editor_frame_move_previous_match (self, TRUE);
      else
        g_assert_not_reached ();

      /*
       * We manually get the string length instead of passing -1 for length
       * because -1 doesn't seem to work as documented.
       */
      text = gtk_entry_get_text (GTK_ENTRY (priv->search_entry));
      len = g_utf8_strlen (text, -1);
      gtk_editable_select_region (GTK_EDITABLE (priv->search_entry), 0, len);
    }

  EXIT;
}

void
gb_editor_frame_find (GbEditorFrame *self,
                      const gchar   *search_text)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gb_editor_frame_on_begin_search (self, GTK_DIR_DOWN, search_text,
                                   self->priv->source_view);
}

static void
gb_editor_frame_find_activate (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  GbEditorFrame *self = user_data;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gb_editor_frame_find (self, NULL);
}

static gboolean
gb_editor_frame_on_query_tooltip (GbEditorFrame *self,
                                  gint           x,
                                  gint           y,
                                  gboolean       keyboard_mode,
                                  GtkTooltip    *tooltip,
                                  GbSourceView  *source_view)
{
  GbEditorFramePrivate *priv;
  GbSourceCodeAssistant *code_assistant;
  GtkTextIter iter;
  GArray *ar;
  gboolean ret = FALSE;
  guint line;
  guint i;

  g_assert (GB_IS_SOURCE_VIEW (source_view));
  g_assert (GB_IS_EDITOR_FRAME (self));

  priv = self->priv;

  code_assistant = gb_editor_document_get_code_assistant (priv->document);
  if (!code_assistant)
    return FALSE;

  ar = gb_source_code_assistant_get_diagnostics (code_assistant);
  if (!ar)
    return FALSE;

  gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (source_view),
                                         GTK_TEXT_WINDOW_WIDGET,
                                         x, y, &x, &y);

  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (source_view),
                                      &iter, x, y);

  line = gtk_text_iter_get_line (&iter);

  for (i = 0; i < ar->len; i++)
    {
      GcaDiagnostic *diag;
      guint j;

      diag = &g_array_index (ar, GcaDiagnostic, i);

      for (j = 0; j < diag->locations->len; j++)
        {
          GcaSourceRange *loc;

          loc = &g_array_index (diag->locations, GcaSourceRange, j);

          if ((loc->begin.line <= line) && (loc->end.line >= line))
            {
              gtk_tooltip_set_text (tooltip, diag->message);
              ret = TRUE;
              goto cleanup;
            }
        }
    }

cleanup:
  g_array_unref (ar);

  return ret;
}

static void
gb_editor_frame_on_switch_to_file (GbEditorFrame *self,
                                   GFile         *file,
                                   GbSourceVim   *vim)
{
  GbWorkspace *workspace;
  GbWorkbench *workbench;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));
  workspace = gb_workbench_get_workspace (workbench, GB_TYPE_EDITOR_WORKSPACE);
  gb_editor_workspace_open (GB_EDITOR_WORKSPACE (workspace), file);
}

static void
gb_editor_frame_on_command_toggled (GbEditorFrame *self,
                                    gboolean       visible,
                                    GbSourceVim   *vim)
{
  GbWorkbench *workbench;
  GAction *action;
  GVariant *params;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_SOURCE_VIM (vim));

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));
  if (!workbench)
    EXIT;

  action = g_action_map_lookup_action (G_ACTION_MAP (workbench),
                                       "toggle-command-bar");
  if (!action)
    EXIT;

  params = g_variant_new_boolean (visible);
  g_action_activate (action, params);

  EXIT;
}

static void
gb_editor_frame_on_jump_to_doc (GbEditorFrame *self,
                                const gchar   *search_text,
                                GbSourceView  *source_view)
{
  GActionGroup *action_group;
  GbWorkbench *workbench;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));
  g_return_if_fail (search_text);

  workbench = gb_widget_get_workbench (GTK_WIDGET (self));
  action_group = gtk_widget_get_action_group (GTK_WIDGET (workbench),
                                              "workspace");
  g_action_group_activate_action (action_group, "jump-to-doc",
                                  g_variant_new_string (search_text));

  EXIT;
}

static void
gb_editor_frame_on_drop_uris (GbEditorFrame  *self,
                              const gchar   **uri_list,
                              GbSourceView   *source_view)
{
  GVariantBuilder *builder;
  GVariant *variant;
  guint i;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_SOURCE_VIEW (source_view));
  g_return_if_fail (uri_list);

  builder = g_variant_builder_new (G_VARIANT_TYPE_STRING_ARRAY);
  for (i = 0; uri_list [i]; i++)
    g_variant_builder_add (builder, "s", uri_list[i]);
  variant = g_variant_builder_end (builder);
  g_variant_builder_unref (builder);

  gb_widget_activate_action (GTK_WIDGET (self),
                             "workspace", "open-uri-list",
                             variant);

  EXIT;
}

static void
gb_editor_frame_grab_focus (GtkWidget *widget)
{
  GbEditorFrame *self = (GbEditorFrame *)widget;

  ENTRY;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->priv->source_view));

  EXIT;
}

static void
gb_editor_frame_scroll_to_line (GbEditorFrame *self,
                                guint          line,
                                guint          offset)
{
  GtkTextBuffer *buffer;
  GtkTextView *text_view;
  GtkTextIter iter;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  buffer = GTK_TEXT_BUFFER (self->priv->document);
  text_view = GTK_TEXT_VIEW (self->priv->source_view);

  gb_gtk_text_buffer_get_iter_at_line_and_offset (buffer, &iter, line, offset);
  gtk_text_buffer_select_range (buffer, &iter, &iter);
  gb_gtk_text_view_scroll_to_iter (text_view, &iter, 0.0, TRUE, 0.0, 0.5);
}

static void
gb_editor_frame_next_diagnostic (GbEditorFrame *self)
{
  GbSourceCodeAssistant *assistant;
  GtkTextMark *mark;
  GtkTextIter iter;
  guint current_line;
  GArray *ar;
  guint i;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  assistant = gb_editor_document_get_code_assistant (self->priv->document);
  ar = gb_source_code_assistant_get_diagnostics (assistant);
  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->priv->document));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->priv->document),
                                    &iter, mark);
  current_line = gtk_text_iter_get_line (&iter);

  if (ar)
    {
      for (i = 0; i < ar->len; i++)
        {
          GcaDiagnostic *diag;
          guint j;

          diag = &g_array_index (ar, GcaDiagnostic, i);

          for (j = 0; j < diag->locations->len; j++)
            {
              GcaSourceRange *range;

              range = &g_array_index (diag->locations, GcaSourceRange, j);

              if (range->begin.line > current_line)
                {
                  gb_editor_frame_scroll_to_line (self,
                                                  range->begin.line,
                                                  range->begin.column);
                  goto cleanup;
                }
            }
        }

      /* wrap around to first diagnostic */
      if (ar->len > 0)
        {
          GcaDiagnostic *diag;

          diag = &g_array_index (ar, GcaDiagnostic, 0);

          if (diag->locations->len > 0)
            {
              GcaSourceRange *range;

              range = &g_array_index (diag->locations, GcaSourceRange, 0);
              gb_editor_frame_scroll_to_line (self, range->begin.line,
                                              range->begin.column);
            }
        }

cleanup:
      g_array_unref (ar);
    }
}

static void
gb_editor_frame_previous_diagnostic (GbEditorFrame *self)
{
  GbSourceCodeAssistant *assistant;
  GtkTextMark *mark;
  GtkTextIter iter;
  guint current_line;
  GArray *ar;
  gint i;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  assistant = gb_editor_document_get_code_assistant (self->priv->document);
  ar = gb_source_code_assistant_get_diagnostics (assistant);
  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->priv->document));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->priv->document),
                                    &iter, mark);
  current_line = gtk_text_iter_get_line (&iter);

  if (ar)
    {
      for (i = 0; i < ar->len; i++)
        {
          GcaDiagnostic *diag;
          guint j;

          diag = &g_array_index (ar, GcaDiagnostic, ar->len-i-1);

          for (j = 0; j < diag->locations->len; j++)
            {
              GcaSourceRange *range;

              range = &g_array_index (diag->locations, GcaSourceRange, j);

              if (range->begin.line < current_line)
                {
                  gb_editor_frame_scroll_to_line (self,
                                                  range->begin.line,
                                                  range->begin.column);
                  goto cleanup;
                }
            }
        }

      /* wrap around to last diagnostic */
      if (ar->len > 0)
        {
          GcaDiagnostic *diag;

          diag = &g_array_index (ar, GcaDiagnostic, ar->len-1);

          if (diag->locations->len > 0)
            {
              GcaSourceRange *range;

              range = &g_array_index (diag->locations, GcaSourceRange, 0);
              gb_editor_frame_scroll_to_line (self, range->begin.line,
                                              range->begin.column);
            }
        }
    }

cleanup:
  g_clear_pointer (&ar, g_array_unref);
}

static void
gb_editor_frame_reformat_activate (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
  GbEditorFrame *self = user_data;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gb_editor_frame_reformat (self);
}

static void
gb_editor_frame_next_diagnostic_activate (GSimpleAction *action,
                                          GVariant      *parameter,
                                          gpointer       user_data)
{
  GbEditorFrame *self = user_data;
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  gb_editor_frame_next_diagnostic (self);
}

static void
gb_editor_frame_previous_diagnostic_activate (GSimpleAction *action,
                                              GVariant      *parameter,
                                              gpointer       user_data)
{
  GbEditorFrame *self = user_data;
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  gb_editor_frame_previous_diagnostic (self);
}

static void
gb_editor_frame_scroll (GbEditorFrame    *self,
                        GtkDirectionType  dir)
{
  GtkAdjustment *vadj;
  GtkScrolledWindow *scroller;
  GtkTextMark *insert;
  GtkTextView *view;
  GtkTextBuffer *buffer;
  GdkRectangle rect;
  GtkTextIter iter;
  gdouble amount;
  gdouble value;
  gdouble upper;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  scroller = self->priv->scrolled_window;
  view = GTK_TEXT_VIEW (self->priv->source_view);
  buffer = GTK_TEXT_BUFFER (self->priv->document);

  insert = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);
  gtk_text_view_get_iter_location (view, &iter, &rect);

  amount = (dir == GTK_DIR_UP) ? -rect.height : rect.height;

  vadj = gtk_scrolled_window_get_vadjustment (scroller);
  value = gtk_adjustment_get_value (vadj);
  upper = gtk_adjustment_get_upper (vadj);
  gtk_adjustment_set_value (vadj, CLAMP (value + amount, 0, upper));
}

static void
gb_editor_frame_scroll_down (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       user_data)
{
  GbEditorFrame *self = user_data;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gb_editor_frame_scroll (self, GTK_DIR_DOWN);
  gtk_text_view_place_cursor_onscreen (GTK_TEXT_VIEW (self->priv->source_view));
}

static void
gb_editor_frame_scroll_up (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       user_data)
{
  GbEditorFrame *self = user_data;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

  gb_editor_frame_scroll (self, GTK_DIR_UP);
  gtk_text_view_place_cursor_onscreen (GTK_TEXT_VIEW (self->priv->source_view));
}

static void
gb_editor_frame_finalize (GObject *object)
{
  GbEditorFrame *self = GB_EDITOR_FRAME (object);

  gb_editor_frame_disconnect (self);

  g_clear_object (&self->priv->code_assistant_renderer);
  g_clear_object (&self->priv->diff_renderer);
  g_clear_object (&self->priv->search_settings);
  g_clear_object (&self->priv->search_highlighter);

  G_OBJECT_CLASS (gb_editor_frame_parent_class)->finalize (object);
}

static void
gb_editor_frame_constructed (GObject *object)
{
  GbSourceChangeMonitor *monitor = NULL;
  GbEditorFramePrivate *priv;
  GtkSourceGutter *gutter;
  GbEditorFrame *self = (GbEditorFrame *)object;
  GbSourceVim *vim;
  GSettings *settings;

  G_OBJECT_CLASS (gb_editor_frame_parent_class)->constructed (object);

  priv = self->priv;

  settings = g_settings_new ("org.gnome.builder.editor");

  if (priv->document)
    monitor = gb_editor_document_get_change_monitor (priv->document);

  gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (priv->source_view),
                                       GTK_TEXT_WINDOW_LEFT);

  priv->diff_renderer = g_object_new (GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER,
                                      "change-monitor", monitor,
                                      "size", 2,
                                      "visible", TRUE,
                                      "xpad", 1,
                                      NULL);
  priv->diff_renderer = g_object_ref (priv->diff_renderer);
  gtk_source_gutter_insert (gutter,
                            GTK_SOURCE_GUTTER_RENDERER (priv->diff_renderer),
                            0);
  g_settings_bind (settings, "show-diff",
                   priv->diff_renderer, "visible",
                   G_SETTINGS_BIND_GET);

  priv->code_assistant_renderer =
    g_object_new (GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER,
                  "code-assistant", NULL,
                  "size", 16,
                  "visible", TRUE,
                  NULL);
  priv->code_assistant_renderer = g_object_ref (priv->code_assistant_renderer);
  gtk_source_gutter_insert (gutter,
                            GTK_SOURCE_GUTTER_RENDERER (priv->code_assistant_renderer),
                            -50);

  priv->search_settings = g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS,
                                        "wrap-around", TRUE,
                                        NULL);
  g_object_bind_property (priv->search_entry, "text",
                          priv->search_settings, "search-text",
                          G_BINDING_SYNC_CREATE);

  priv->search_highlighter =
    g_object_new (GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER,
                  "search-settings", priv->search_settings,
                  NULL);
  g_object_set (priv->source_view,
                "search-highlighter", priv->search_highlighter,
                NULL);
  g_object_bind_property (priv->search_revealer, "reveal-child",
                          priv->source_view, "show-shadow",
                          G_BINDING_SYNC_CREATE);

  vim = gb_source_view_get_vim (priv->source_view);
  g_signal_connect_object (vim,
                           "command-visibility-toggled",
                           G_CALLBACK (gb_editor_frame_on_command_toggled),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (vim,
                           "switch-to-file",
                           G_CALLBACK (gb_editor_frame_on_switch_to_file),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "display-documentation",
                           G_CALLBACK (gb_editor_frame_on_jump_to_doc),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "drop-uris",
                           G_CALLBACK (gb_editor_frame_on_drop_uris),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "focus-in-event",
                           G_CALLBACK (gb_editor_frame_on_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "populate-popup",
                           G_CALLBACK (gb_editor_frame_on_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "push-snippet",
                           G_CALLBACK (gb_editor_frame_on_push_snippet),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "begin-search",
                           G_CALLBACK (gb_editor_frame_on_begin_search),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->source_view,
                           "query-tooltip",
                           G_CALLBACK (gb_editor_frame_on_query_tooltip),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->search_entry,
                           "key-press-event",
                           G_CALLBACK (gb_editor_frame_on_search_entry_key_press),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->search_entry,
                           "changed",
                           G_CALLBACK (gb_editor_frame_on_search_entry_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->search_entry,
                           "activate",
                           G_CALLBACK (gb_editor_frame_on_search_entry_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->forward_search,
                           "clicked",
                           G_CALLBACK (gb_editor_frame_on_forward_search_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (priv->backward_search,
                           "clicked",
                           G_CALLBACK (gb_editor_frame_on_backward_search_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_unref (settings);
}

static void
gb_editor_frame_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbEditorFrame *self = GB_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_frame_get_document (self));
      break;

    case PROP_SEARCH_DIRECTION:
      g_value_set_enum (value, gb_editor_frame_get_search_direction (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_frame_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbEditorFrame *self = GB_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_DOCUMENT:
      gb_editor_frame_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_frame_class_init (GbEditorFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_editor_frame_constructed;
  object_class->finalize = gb_editor_frame_finalize;
  object_class->get_property = gb_editor_frame_get_property;
  object_class->set_property = gb_editor_frame_set_property;

  widget_class->grab_focus = gb_editor_frame_grab_focus;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The document for the editor."),
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT,
                                   gParamSpecs [PROP_DOCUMENT]);

  gParamSpecs [PROP_SEARCH_DIRECTION] =
    g_param_spec_enum ("search-direction",
                       _("Search Direction"),
                       _("The direction of the last text searched for."),
                       GTK_TYPE_DIRECTION_TYPE,
                       GTK_DIR_DOWN,
                       (G_PARAM_READABLE |
                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SEARCH_DIRECTION,
                                   gParamSpecs [PROP_SEARCH_DIRECTION]);


  gSignals [FOCUSED] =
    g_signal_new ("focused",
                  GB_TYPE_EDITOR_FRAME,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-frame.ui");
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, busy_spinner);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, floating_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, forward_search);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, backward_search);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, scrolled_window);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_entry);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, source_view);

  g_type_ensure (GB_TYPE_SOURCE_VIEW);
  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
  g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
}

static void
gb_editor_frame_init (GbEditorFrame *self)
{
  const GActionEntry entries[] = {
    { "find", gb_editor_frame_find_activate },
    { "reformat", gb_editor_frame_reformat_activate },
    { "scroll-up", gb_editor_frame_scroll_up },
    { "scroll-down", gb_editor_frame_scroll_down },
    { "next-diagnostic", gb_editor_frame_next_diagnostic_activate },
    { "previous-diagnostic", gb_editor_frame_previous_diagnostic_activate },
  };
  GSimpleActionGroup *actions;

  self->priv = gb_editor_frame_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   entries, G_N_ELEMENTS (entries),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "editor-frame",
                                  G_ACTION_GROUP (actions));
  g_object_unref (actions);

  self->priv->search_direction = GTK_DIR_DOWN;
}
