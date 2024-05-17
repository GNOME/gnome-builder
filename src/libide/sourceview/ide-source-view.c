/* ide-source-view.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "ide-source-view"

#include "config.h"

#include <glib/gi18n.h>
#include <math.h>

#include "ide-source-view-private.h"

#define MIN_BUBBLE_SCALE -2
#define MAX_BUBBLE_SCALE 3
#define X_PAD 5
#define Y_PAD 3

typedef struct
{
  GtkEventController *controller;
  int priority;
} Controller;

G_DEFINE_TYPE (IdeSourceView, ide_source_view, GTK_SOURCE_TYPE_VIEW)

enum {
  PROP_0,
  PROP_FONT_DESC,
  PROP_FONT_SCALE,
  PROP_INSERT_MATCHING_BRACE,
  PROP_LINE_HEIGHT,
  PROP_OVERWRITE_BRACES,
  PROP_ZOOM_LEVEL,
  N_PROPS,

  /* Property Overrides */
  PROP_HIGHLIGHT_CURRENT_LINE,
};

enum {
  POPULATE_MENU,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
controller_clear (gpointer data)
{
  Controller *c = data;
  g_clear_object (&c->controller);
}

char *
_ide_source_view_generate_css (GtkSourceView              *view,
                               const PangoFontDescription *font_desc,
                               int                         font_scale,
                               double                      line_height)
{
  g_autofree char *font_css = NULL;
  PangoFontDescription *scaled = NULL;
  PangoFontDescription *system_font = NULL;
  GString *str;
  int size = 11; /* 11pt */
  char line_height_str[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

  str = g_string_new (NULL);

  g_string_append (str, "textview {\n");

  /* Get font information to adjust line height and font changes */
  if (font_desc == NULL)
    {
      g_object_get (g_application_get_default (),
                    "system-font", &system_font,
                    NULL);
      font_desc = system_font;
    }

  if (font_desc != NULL &&
      pango_font_description_get_set_fields (font_desc) & PANGO_FONT_MASK_SIZE)
    size = pango_font_description_get_size (font_desc) / PANGO_SCALE;

  if (size + font_scale < 1)
    font_scale = -size + 1;

  size = MAX (1, size + font_scale);

  if (size != 0)
    {
      if (font_desc)
        scaled = pango_font_description_copy (font_desc);
      else
        scaled = pango_font_description_new ();
      pango_font_description_set_size (scaled, size * PANGO_SCALE);
      font_desc = scaled;
    }

  if (font_desc)
    {
      font_css = ide_font_description_to_css (font_desc);
      g_string_append (str, font_css);
    }

  g_ascii_dtostr (line_height_str, sizeof line_height_str, line_height);
  line_height_str[6] = 0;
  g_string_append_printf (str, "\nline-height: %s;\n", line_height_str);

  g_string_append (str, "}\n");

  g_clear_pointer (&scaled, pango_font_description_free);
  g_clear_pointer (&system_font, pango_font_description_free);

  return g_string_free (str, FALSE);
}

static void
ide_source_view_update_css (IdeSourceView *self)
{
  g_autofree char *css = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  if ((css = _ide_source_view_generate_css (GTK_SOURCE_VIEW (self), self->font_desc, self->font_scale, self->line_height)))
    gtk_css_provider_load_from_data (self->css_provider, css, -1);
}

static void
ide_source_view_set_line_height (IdeSourceView *self,
                                 double         line_height)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (line_height >= .5);
  g_return_if_fail (line_height <= 10.);

  if (self->line_height != line_height)
    {
      self->line_height = line_height;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LINE_HEIGHT]);
      ide_source_view_update_css (self);
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

static void
tweak_gutter_spacing (GtkSourceView *view)
{
  GtkSourceGutter *gutter;
  GtkWidget *child;
  guint n = 0;

  g_assert (GTK_SOURCE_IS_VIEW (view));

  /* Ensure we have a line gutter renderer to tweak */
  gutter = gtk_source_view_get_gutter (view, GTK_TEXT_WINDOW_LEFT);

  /* Add margin to first gutter renderer */
  for (child = gtk_widget_get_first_child (GTK_WIDGET (gutter));
       child != NULL;
       child = gtk_widget_get_next_sibling (child), n++)
    {
      if (GTK_SOURCE_IS_GUTTER_RENDERER (child))
        gtk_widget_set_margin_start (child, n == 0 ? 4 : 0);
    }
}

static gboolean
show_menu_from_idle (gpointer data)
{
  IdeSourceView *self = data;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  gtk_widget_activate_action (GTK_WIDGET (self), "menu.popup", NULL);

  return G_SOURCE_REMOVE;
}

static void
ide_source_view_click_pressed_cb (IdeSourceView   *self,
                                  int              n_press,
                                  double           x,
                                  double           y,
                                  GtkGestureClick *click)
{
  GdkEventSequence *sequence;
  GtkTextBuffer *buffer;
  GdkEvent *event;
  GtkTextIter iter;
  int buf_x, buf_y;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_GESTURE_CLICK (click));

  self->click_x = x;
  self->click_y = y;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (click));
  event = gtk_gesture_get_last_event (GTK_GESTURE (click), sequence);

  if (n_press != 1 || !gdk_event_triggers_context_menu (event))
    IDE_EXIT;

  /* Move the cursor position to where the click occurred so that
   * the context menu will be useful for the click location.
   */
  if (!gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self),
                                             GTK_TEXT_WINDOW_WIDGET,
                                             x, y, &buf_x, &buf_y);
      gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self), &iter, buf_x, buf_y);
      gtk_text_buffer_select_range (buffer, &iter, &iter);
    }

  g_signal_emit (self, signals[POPULATE_MENU], 0);

  /* Steal this event and manage showing the popup ourselves without
   * using an event to silence GTK warnings elsewise. Doing this from
   * the idle callback is really what appears to fix the allocation
   * issue within GTK.
   */
  gtk_gesture_set_sequence_state (GTK_GESTURE (click),
                                  sequence,
                                  GTK_EVENT_SEQUENCE_CLAIMED);
  g_idle_add_full (G_PRIORITY_LOW+1000,
                   show_menu_from_idle,
                   g_object_ref (self),
                   g_object_unref);

  IDE_EXIT;
}

static gboolean
ide_source_view_key_pressed_cb (IdeSourceView         *self,
                                guint                  keyval,
                                guint                  keycode,
                                GdkModifierType        state,
                                GtkEventControllerKey *key)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  self->in_backspace = keyval == GDK_KEY_BackSpace;
  self->in_key_press = TRUE;

  return FALSE;
}

static gboolean
ide_source_view_key_released_cb (IdeSourceView         *self,
                                 guint                  keyval,
                                 guint                  keycode,
                                 GdkModifierType        state,
                                 GtkEventControllerKey *key)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

  self->in_key_press = FALSE;

  return FALSE;
}

static gboolean
ide_source_view_do_jump_to_insert (IdeSourceView *self)
{
  GdkRectangle area;
  GtkTextIter iter;
  GtkTextIter reverse;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (!self->buffer || IDE_IS_BUFFER (self->buffer));

  if (self->buffer == NULL)
    return FALSE;

  /* To make sure that we can really jump to the target location
   * we get the buffer coordinates of the iter then look up the
   * iter at that location. If they match, then the forward line
   * validation has gone far enough to locate this iter and allow
   * us to scroll to it.
   *
   * Otherwise, we need to delay a bit more until we have enough
   * information to scroll there. We return FALSE in this case
   * so the caller can detect the situation.
   */

  ide_buffer_get_selection_bounds (self->buffer, &iter, NULL);
  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self), &iter, &area);
  gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self),
                                      &reverse,
                                      area.x + (area.width/2),
                                      area.y + (area.height/2));

  /* Not the same line? Must not have forward validated enough. */
  if (gtk_text_iter_get_line (&iter) != gtk_text_iter_get_line (&reverse))
    return FALSE;

  ide_source_view_jump_to_iter (GTK_TEXT_VIEW (self), &iter, 0, TRUE, 1.0, 0.5);

  return TRUE;
}

static gboolean
ide_source_view_pending_scroll_idle_cb (gpointer data)
{
  IdeSourceView *self = data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (ide_source_view_do_jump_to_insert (self))
    {
      self->pending_scroll_source = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
ide_source_view_buffer_request_jump_to_insert_cb (IdeSourceView *self,
                                                  IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->pending_scroll_source == 0)
    self->pending_scroll_source = g_idle_add_full (G_PRIORITY_LOW,
                                                   ide_source_view_pending_scroll_idle_cb,
                                                   self,
                                                   NULL);

  IDE_EXIT;
}

static void
ide_source_view_buffer_notify_language_cb (IdeSourceView *self,
                                           GParamSpec    *pspec,
                                           IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  _ide_source_view_addins_set_language (self,
                                        gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer)));

  IDE_EXIT;
}

static gboolean
can_insert_match (const GtkTextIter *location,
                  gunichar           match)
{
  GtkSourceBuffer *buffer;
  gunichar next;

  g_assert (location != NULL);
  g_assert (match != 0);

  next = gtk_text_iter_get_char (location);

  if (next == match)
    return FALSE;

  /* If the next character is not space/EOL/EOB, then do nothing */
  if (next && !g_unichar_isspace (next))
    return FALSE;

  buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (location));

  if (gtk_source_buffer_iter_has_context_class (buffer, location, "comment") ||
      gtk_source_buffer_iter_has_context_class (buffer, location, "string") ||
      gtk_source_buffer_iter_has_context_class (buffer, location, "path"))
    return FALSE;

  return TRUE;
}

static void
ide_source_view_insert_text_cb (IdeSourceView *self,
                                GtkTextIter   *location,
                                const char    *text,
                                int            length,
                                IdeBuffer     *buffer)
{
  const char *match = NULL;
  GtkTextIter insert;
  gunichar overwrite = 0;
  gunichar ch;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (text != NULL);
  g_assert (location != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  /* Paste happens asynchronously, so do our best to handle that upon the
   * next insertion of text so that we don't try to process it for things
   * like matching braces.
   *
   * See GNOME/gnome-builder#1870.
   */
  if (self->waiting_for_paste)
    return;

  /* Ignore anything odd that we won't be able to process or is outside
   * of a key-press event (such as buffer changes from plugins).
   */
  if (length != 1 || !self->in_key_press)
    return;

  /* Ignore if we are in an undo/redo operation */
  if (self->undo_recurse_count > 0 || self->redo_recurse_count > 0)
    return;

  ide_buffer_get_selection_bounds (buffer, &insert, NULL);

  /* Do we want to allow selection overwrite to "" instead of "? */
  if (gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
    return;

  if (!gtk_text_iter_equal (&insert, location))
    return;

  ch = g_utf8_get_char (text);

  switch (ch)
    {
    case '{': match = "}"; break;
    case '[': match = "]"; break;
    case '(': match = ")"; break;
    case '"': match = "\""; overwrite = ch; break;
    case '\'': match = "'"; overwrite = ch; break;
    case '}': case ']': case ')': overwrite = ch; break;
    default: break;
    }

  /* "Overwrite" the next character if necessary */
  if (overwrite != 0 &&
      self->overwrite_braces &&
      gtk_text_iter_get_char (location) == overwrite)
    {
      GtkTextIter next = *location;
      GtkTextMark *mark;

      /* We can't just step over this character because we need to ensure
       * that indenters are queried. For example, ) in the C indenter can
       * perform auto-indentation and overwrite would skip that.
       */
      mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &next, TRUE);
      gtk_text_iter_forward_char (&next);
      gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), location, &next);
      /* gtk_text_iter_delete() should update the value of location/next
       * to a valid iterator, but other code may respond to it and further
       * mutate it and we do not get notified of that. So instead, just
       * update our iter manually to be sure we leave a valid iter.
       */
      gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), location, mark);
      gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), mark);
      return;
    }

  if (match != NULL &&
      match[0] != 0 &&
      self->insert_matching_brace &&
      can_insert_match (location, match[0]))
    {
      guint offset = gtk_text_iter_get_offset (location);

      self->in_key_press = FALSE;
      gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), location, match, 1);
      gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), location, offset);
      gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), location, location);
      self->in_key_press = TRUE;
    }
}

static void
ide_source_view_delete_range_cb (IdeSourceView *self,
                                 GtkTextIter   *begin,
                                 GtkTextIter   *end,
                                 IdeBuffer     *buffer)
{
  GtkTextIter prev;
  gunichar match = 0;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (!self->in_key_press ||
      !self->in_backspace ||
      !self->insert_matching_brace ||
      self->undo_recurse_count ||
      self->redo_recurse_count)
    return;

  gtk_text_iter_order (begin, end);
  prev = *end;
  gtk_text_iter_backward_char (&prev);

  switch (gtk_text_iter_get_char (&prev))
    {
    case '{': match = '}'; break;
    case '[': match = ']'; break;
    case '(': match = ')'; break;
    case '"': match = '"'; break;
    case '\'': match = '\''; break;
    default: break;
    }

  /* Remove matching }])"' */
  if (match && gtk_text_iter_get_char (end) == match)
    gtk_text_iter_forward_char (end);
}

static void
ide_source_view_buffer_before_undo_cb (IdeSourceView *self,
                                       IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->undo_recurse_count++;
}

static void
ide_source_view_buffer_after_undo_cb (IdeSourceView *self,
                                      IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->undo_recurse_count--;
}

static void
ide_source_view_buffer_before_redo_cb (IdeSourceView *self,
                                       IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->redo_recurse_count++;
}

static void
ide_source_view_buffer_after_redo_cb (IdeSourceView *self,
                                      IdeBuffer     *buffer)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->redo_recurse_count--;
}

static void
ide_source_view_buffer_paste_done_cb (IdeSourceView *self,
                                      GdkClipboard  *clipboard,
                                      IdeBuffer     *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GDK_IS_CLIPBOARD (clipboard));
  g_assert (IDE_IS_BUFFER (buffer));

  self->waiting_for_paste = FALSE;

  IDE_EXIT;
}

static void
ide_source_view_connect_buffer (IdeSourceView *self,
                                IdeBuffer     *buffer)
{
  GtkSourceLanguage *language;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (self->buffer == NULL);

  g_set_object (&self->buffer, buffer);

  /* There are cases where we need to force a scroll to insert
   * from just an IdeBuffer pointer. Respond to that appropriately
   * (which generally just happens on load).
   */
  g_signal_connect_object (buffer,
                           "request-scroll-to-insert",
                           G_CALLBACK (ide_source_view_buffer_request_jump_to_insert_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Get the current language for the buffer and be notified of
   * changes in the future so that we can update providers.
   */
  g_signal_connect_object (buffer,
                           "notify::language",
                           G_CALLBACK (ide_source_view_buffer_notify_language_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Update CSS when style changes */
  g_signal_connect_object (buffer,
                           "notify::style-scheme",
                           G_CALLBACK (ide_source_view_update_css),
                           self,
                           G_CONNECT_SWAPPED);

  /* Insert matching braces features */
  g_signal_connect_object (buffer,
                           "insert-text",
                           G_CALLBACK (ide_source_view_insert_text_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (buffer,
                           "delete-range",
                           G_CALLBACK (ide_source_view_delete_range_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Track when a paste operation has completed */
  g_signal_connect_object (buffer,
                           "paste-done",
                           G_CALLBACK (ide_source_view_buffer_paste_done_cb),
                           self,
                           G_CONNECT_SWAPPED);

  /* Track when we're within an undo/redo operation */
  g_signal_connect_object (buffer,
                           "undo",
                           G_CALLBACK (ide_source_view_buffer_before_undo_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (buffer,
                           "undo",
                           G_CALLBACK (ide_source_view_buffer_after_undo_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  g_signal_connect_object (buffer,
                           "redo",
                           G_CALLBACK (ide_source_view_buffer_before_redo_cb),
                           self,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (buffer,
                           "redo",
                           G_CALLBACK (ide_source_view_buffer_after_redo_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  /* Load addins immediately */
  language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
  _ide_source_view_addins_init (self, language);
}

static void
ide_source_view_disconnect_buffer (IdeSourceView *self)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (self->buffer == NULL)
    return;

  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (ide_source_view_buffer_paste_done_cb),
                                        self);
  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (ide_source_view_buffer_before_undo_cb),
                                        self);
  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (ide_source_view_buffer_after_undo_cb),
                                        self);
  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (ide_source_view_buffer_before_redo_cb),
                                        self);
  g_signal_handlers_disconnect_by_func (self->buffer,
                                        G_CALLBACK (ide_source_view_buffer_after_redo_cb),
                                        self);

  _ide_source_view_addins_shutdown (self);

  g_clear_object (&self->buffer);
}

static void
ide_source_view_notify_buffer_cb (IdeSourceView *self,
                                  GParamSpec    *pspec,
                                  gpointer       user_data)
{
  GtkTextBuffer *buffer;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (user_data == NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  if (buffer == GTK_TEXT_BUFFER (self->buffer))
    IDE_EXIT;

  ide_source_view_disconnect_buffer (self);

  if (IDE_IS_BUFFER (buffer))
    ide_source_view_connect_buffer (self, IDE_BUFFER (buffer));

  IDE_EXIT;
}

static gboolean
ide_source_view_jump_to_insert_in_idle_cb (gpointer user_data)
{
  IdeSourceView *self = user_data;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

  ide_source_view_jump_to_iter (GTK_TEXT_VIEW (self), &iter, .25, TRUE, 1.0, 0.5);

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_source_view_root (GtkWidget *widget)
{
  IdeSourceView *self = (IdeSourceView *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  GTK_WIDGET_CLASS (ide_source_view_parent_class)->root (widget);

  g_idle_add_full (G_PRIORITY_LOW,
                   ide_source_view_jump_to_insert_in_idle_cb,
                   g_object_ref (self),
                   g_object_unref);

  IDE_EXIT;
}

static void
ide_source_view_menu_popup_action (GtkWidget  *widget,
                                   const char *action_name,
                                   GVariant   *param)
{
  IdeSourceView *self = (IdeSourceView *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (ide_str_equal0 (action_name, "menu.popup"));
  g_assert (param == NULL);

  if (self->popup_menu == NULL)
    {
      GMenuModel *model;

      model = G_MENU_MODEL (self->joined_menu);
      self->popup_menu = GTK_POPOVER (gtk_popover_menu_new_from_model (model));
      gtk_widget_set_parent (GTK_WIDGET (self->popup_menu), widget);
      gtk_popover_set_position (self->popup_menu, GTK_POS_RIGHT);
      gtk_popover_set_has_arrow (self->popup_menu, FALSE);
      gtk_widget_set_halign (GTK_WIDGET (self->popup_menu), GTK_ALIGN_START);
      gtk_widget_set_valign (GTK_WIDGET (self->popup_menu), GTK_ALIGN_START);
    }

  gtk_popover_set_pointing_to (self->popup_menu,
                               &(GdkRectangle) { self->click_x, self->click_y, 1, 1 });

  gtk_popover_popup (self->popup_menu);

  IDE_EXIT;
}

static void
ide_source_view_focus_enter_cb (IdeSourceView           *self,
                                GtkEventControllerFocus *focus)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  if (self->highlight_current_line)
    gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self), TRUE);

  IDE_EXIT;
}

static void
ide_source_view_focus_leave_cb (IdeSourceView           *self,
                                GtkEventControllerFocus *focus)
{
  IDE_ENTRY;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (focus));

  gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self), FALSE);

  IDE_EXIT;
}

static gboolean
ide_source_view_update_overscroll (gpointer user_data)
{
  IdeSourceView *self = user_data;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  self->overscroll_source = 0;

  if (gtk_widget_get_mapped (GTK_WIDGET (self)))
    {
      GdkRectangle visible_rect;

      gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self), &visible_rect);
      gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self), visible_rect.height * .75);
    }

  return G_SOURCE_REMOVE;
}

static void
ide_source_view_size_allocate (GtkWidget *widget,
                               int        width,
                               int        height,
                               int        baseline)
{
  IdeSourceView *self = (IdeSourceView *)widget;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_IS_WIDGET (widget));

  GTK_WIDGET_CLASS (ide_source_view_parent_class)->size_allocate (widget, width, height, baseline);

  if (self->popup_menu != NULL)
    gtk_popover_present (self->popup_menu);

  if (self->overscroll_source == 0)
    self->overscroll_source = g_idle_add (ide_source_view_update_overscroll, self);
}

static void
ide_source_view_selection_sort (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  GtkSourceSortFlags sort_flags = GTK_SOURCE_SORT_FLAGS_NONE;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean ignore_case;
  gboolean reverse;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE ("(bb)")));

  g_variant_get (param, "(bb)", &ignore_case, &reverse);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
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
ide_source_view_selection_join (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

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
ide_source_view_set_font_scale (IdeSourceView *self,
                                int            font_scale)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (self->font_scale != font_scale)
    {
      /* If we're zooming in from zoom out, make sure we're actually
       * making progress zooming.
       */
      if (self->font_scale < 0 && self->font_scale+1 == font_scale)
        {
          int size = pango_font_description_get_size (self->font_desc) / PANGO_SCALE;

          if (self->font_scale <= -size)
            {
              /* We want to make progress, and -size that requires
               * incrementing twice.
               */
              self->font_scale = -size + 1 + 1;
              goto update;
            }
        }

      self->font_scale = font_scale;

    update:
      ide_source_view_update_css (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_SCALE]);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);
    }
}

static void
ide_source_view_zoom_in_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (widget);
  ide_source_view_set_font_scale (self, self->font_scale + 1);
}

static void
ide_source_view_zoom_out_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (widget);
  ide_source_view_set_font_scale (self, self->font_scale - 1);
}

static void
ide_source_view_zoom_one_action (GtkWidget  *widget,
                                 const char *action_name,
                                 GVariant   *param)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (widget);

  ide_source_view_set_font_scale (self, 0);
}

static void
ide_source_view_zoom (IdeSourceView *self,
                      int            amount)
{
  g_assert (IDE_IS_SOURCE_VIEW (self));

  if (amount == 0)
    ide_source_view_set_font_scale (self, 0);
  else
    ide_source_view_set_font_scale (self, self->font_scale + amount);

  ide_source_view_update_css (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_SCALE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);
}

static gboolean
on_scroll_scrolled_cb (GtkEventControllerScroll *scroll,
                       double                    dx,
                       double                    dy,
                       IdeSourceView            *self)
{
  GdkModifierType mods;

  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  mods = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  if ((mods & GDK_CONTROL_MASK) != 0)
    {
      ide_source_view_zoom (self, dy < 0 ? 1 : -1);
      return TRUE;
    }

  return FALSE;
}

static void
on_scroll_begin_cb (GtkEventControllerScroll *scroll,
                    IdeSourceView            *self)
{
  GdkModifierType state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (scroll));

  if (state & GDK_CONTROL_MASK)
    gtk_event_controller_scroll_set_flags (scroll,
                                           GTK_EVENT_CONTROLLER_SCROLL_VERTICAL |
                                           GTK_EVENT_CONTROLLER_SCROLL_DISCRETE);
}

static void
on_scroll_end_cb (GtkEventControllerScroll *scroll,
                  IdeSourceView            *self)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (scroll));
  g_assert (IDE_IS_SOURCE_VIEW (self));

  gtk_event_controller_scroll_set_flags (scroll, GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
}

static void
ide_source_view_push_snippet (GtkSourceView    *source_view,
                              GtkSourceSnippet *snippet,
                              GtkTextIter      *location)
{
  IdeSourceView *self = (IdeSourceView *)source_view;
  GtkSourceSnippetContext *context;
  GFile *file = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));
  g_assert (GTK_SOURCE_IS_SNIPPET (snippet));
  g_assert (location != NULL);

  context = gtk_source_snippet_get_context (snippet);

  if (!gtk_source_snippet_context_get_variable (context, "fullname"))
    gtk_source_snippet_context_set_variable (context, "fullname", g_get_real_name ());

  if (self->buffer != NULL)
    {
      if ((file = ide_buffer_get_file (self->buffer)))
        {
          g_autoptr(GFile) parent = g_file_get_parent (file);
          g_autofree gchar *basename = g_file_get_basename (file);
          IdeContext *ide_context;

          gtk_source_snippet_context_set_constant (context, "filename", basename);
          gtk_source_snippet_context_set_constant (context, "TM_FILENAME", basename);
          gtk_source_snippet_context_set_constant (context, "dirname", g_file_peek_path (parent));
          gtk_source_snippet_context_set_constant (context, "path", g_file_peek_path (file));

          if ((ide_context = ide_buffer_ref_context (self->buffer)))
            {
              g_autoptr(GFile) workdir = ide_context_ref_workdir (ide_context);
              g_autofree gchar *relative_path = g_file_get_relative_path (workdir, file);
              g_autofree gchar *relative_dirname = g_file_get_relative_path (workdir, parent);

              gtk_source_snippet_context_set_constant (context, "relative_path", relative_path);
              gtk_source_snippet_context_set_constant (context, "relative_dirname", relative_dirname);
            }
        }
    }

  GTK_SOURCE_VIEW_CLASS (ide_source_view_parent_class)->push_snippet (source_view, snippet, location);
}

static void
ide_source_view_action_select_line (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);

  /* Move beginning of selection/cursor to position 0 of first */
  if (!gtk_text_iter_starts_line (&begin))
    gtk_text_iter_set_line_offset (&begin, 0);

  /* Move end of selection/cursor to end of line */
  if (!gtk_text_iter_ends_line (&end))
    gtk_text_iter_forward_to_line_end (&end);

  /* Swallow the \n with the selection */
  if (!gtk_text_iter_is_end (&end))
    gtk_text_iter_forward_char (&end);

  gtk_text_buffer_select_range (buffer, &begin, &end);

  /* NOTE: This shouldn't be needed, but due to some invalidation issues in
   *       the line display cache, seems to improve chances we get proper
   *       invalidation lines within cache.
   */
  gtk_widget_queue_draw (widget);
}

static void
ide_source_view_action_delete_line (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *param)
{
  IdeSourceView *self = (IdeSourceView *)widget;
  GtkTextBuffer *buffer;
  GtkTextIter begin, end;
  g_autofree char *text = NULL;

  g_assert (IDE_IS_SOURCE_VIEW (self));

  ide_source_view_action_select_line (widget, NULL, NULL);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  /* If we're at the end of the buffer, then we need to remove the
   * leading \n instead of the trailing \n to make it appear to the
   * user that a line was removed.
   */
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  gtk_text_iter_order (&begin, &end);
  if (gtk_text_iter_is_end (&end) &&
      gtk_text_iter_get_line (&begin) == gtk_text_iter_get_line (&end))
    gtk_text_iter_backward_char (&begin);
  text = gtk_text_iter_get_slice (&begin, &end);
  gtk_text_buffer_delete (buffer, &begin, &end);
  gtk_text_buffer_end_user_action (buffer);

  /* now move the cursor to the beginning of the new line */
  gtk_text_iter_set_line_offset (&begin, 0);
  while (!gtk_text_iter_ends_line (&begin) &&
         g_unichar_isspace (gtk_text_iter_get_char (&begin)))
    gtk_text_iter_forward_char (&begin);
  gtk_text_buffer_select_range (buffer, &begin, &begin);

  /* it's nice to place the text into the primary selection so that
   * the user can paste it in other places.
   */
  gdk_clipboard_set_text (gtk_widget_get_primary_clipboard (widget), text);
}

static void
ide_source_view_action_duplicate_line (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *param)
{
  g_autofree char *text = NULL;
  g_autofree char *duplicate_line = NULL;
  GtkTextIter begin, end;
  GtkTextMark *cursor;
  GtkTextBuffer *buffer;
  gboolean selected;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (widget));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));
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

      if (!gtk_text_iter_ends_line (&end))
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
ide_source_view_paste_clipboard (GtkTextView *text_view)
{
  IdeSourceView *self = (IdeSourceView *)text_view;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_SOURCE_VIEW (self));

  self->waiting_for_paste = TRUE;

  GTK_TEXT_VIEW_CLASS (ide_source_view_parent_class)->paste_clipboard (text_view);
}

static void
ide_source_view_dispose (GObject *object)
{
  IdeSourceView *self = (IdeSourceView *)object;

  IDE_ENTRY;

  g_clear_handle_id (&self->overscroll_source, g_source_remove);
  g_clear_handle_id (&self->pending_scroll_source, g_source_remove);

  ide_source_view_disconnect_buffer (self);

  g_clear_object (&self->search_context);
  g_clear_object (&self->joined_menu);
  g_clear_object (&self->css_provider);
  g_clear_pointer (&self->font_desc, pango_font_description_free);
  g_clear_pointer (&self->controllers, g_array_unref);
  g_clear_pointer ((GtkWidget **)&self->popup_menu, gtk_widget_unparent);

  g_assert (self->completion_providers == NULL);
  g_assert (self->hover_providers == NULL);
  g_assert (self->indenter == NULL);

  G_OBJECT_CLASS (ide_source_view_parent_class)->dispose (object);

  IDE_EXIT;
}

static void
ide_source_view_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeSourceView *self = IDE_SOURCE_VIEW (object);

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      g_value_set_boxed (value, ide_source_view_get_font_desc (self));
      break;

    case PROP_FONT_SCALE:
      g_value_set_int (value, self->font_scale);
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      g_value_set_boolean (value, ide_source_view_get_highlight_current_line (self));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      g_value_set_boolean (value, ide_source_view_get_insert_matching_brace (self));
      break;

    case PROP_LINE_HEIGHT:
      g_value_set_double (value, self->line_height);
      break;

    case PROP_OVERWRITE_BRACES:
      g_value_set_boolean (value, ide_source_view_get_overwrite_braces (self));
      break;

    case PROP_ZOOM_LEVEL:
      g_value_set_double (value, ide_source_view_get_zoom_level (self));
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

  switch (prop_id)
    {
    case PROP_FONT_DESC:
      ide_source_view_set_font_desc (self, g_value_get_boxed (value));
      break;

    case PROP_FONT_SCALE:
      self->font_scale = g_value_get_int (value);
      ide_source_view_update_css (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);
      break;

    case PROP_HIGHLIGHT_CURRENT_LINE:
      ide_source_view_set_highlight_current_line (self, g_value_get_boolean (value));
      break;

    case PROP_INSERT_MATCHING_BRACE:
      ide_source_view_set_insert_matching_brace (self, g_value_get_boolean (value));
      break;

    case PROP_LINE_HEIGHT:
      ide_source_view_set_line_height (self, g_value_get_double (value));
      break;

    case PROP_OVERWRITE_BRACES:
      ide_source_view_set_overwrite_braces (self, g_value_get_boolean (value));
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

  object_class->dispose = ide_source_view_dispose;
  object_class->get_property = ide_source_view_get_property;
  object_class->set_property = ide_source_view_set_property;

  widget_class->root = ide_source_view_root;
  widget_class->size_allocate = ide_source_view_size_allocate;

  text_view_class->paste_clipboard = ide_source_view_paste_clipboard;

  source_view_class->push_snippet = ide_source_view_push_snippet;

  g_object_class_override_property (object_class,
                                    PROP_HIGHLIGHT_CURRENT_LINE,
                                    "highlight-current-line");

  properties [PROP_LINE_HEIGHT] =
    g_param_spec_double ("line-height",
                         "Line height",
                         "The line height of all lines",
                         0.5, 10.0, 1.2,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_DESC] =
    g_param_spec_boxed ("font-desc",
                         "Font Description",
                         "The font to use for text within the editor",
                         PANGO_TYPE_FONT_DESCRIPTION,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_FONT_SCALE] =
    g_param_spec_int ("font-scale",
                      "Font Scale",
                      "The font scale",
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_INSERT_MATCHING_BRACE] =
    g_param_spec_boolean ("insert-matching-brace",
                          "Insert Matching Brace",
                          "Insert a matching brace/bracket/quotation/parenthesis",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_OVERWRITE_BRACES] =
    g_param_spec_boolean ("overwrite-braces",
                          "Overwrite Braces",
                          "Overwrite a matching brace/bracket/quotation/parenthesis",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_ZOOM_LEVEL] =
    g_param_spec_double ("zoom-level",
                         "Zoom Level",
                         "Zoom Level",
                         -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * IdeSourceView::populate-menu:
   * @self: an #IdeSourceView
   *
   * The "populate-menu" signal is emitted before the context meu is shown
   * to the user. Handlers of this signal should update any menu items they
   * have which have been connected using ide_source_view_append_menu() or
   * simmilar.
   */
  signals[POPULATE_MENU] =
    g_signal_new_class_handler ("populate-menu",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                NULL,
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 0);

  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, ide_source_view_menu_popup_action);
  gtk_widget_class_install_action (widget_class, "zoom.in", NULL, ide_source_view_zoom_in_action);
  gtk_widget_class_install_action (widget_class, "zoom.out", NULL, ide_source_view_zoom_out_action);
  gtk_widget_class_install_action (widget_class, "zoom.one", NULL, ide_source_view_zoom_one_action);
  gtk_widget_class_install_action (widget_class, "selection.sort", "(bb)", ide_source_view_selection_sort);
  gtk_widget_class_install_action (widget_class, "selection.join", NULL, ide_source_view_selection_join);
  gtk_widget_class_install_action (widget_class, "buffer.select-line", NULL, ide_source_view_action_select_line);
  gtk_widget_class_install_action (widget_class, "buffer.delete-line", NULL, ide_source_view_action_delete_line);
  gtk_widget_class_install_action (widget_class, "buffer.duplicate-line", NULL, ide_source_view_action_duplicate_line);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_plus, GDK_CONTROL_MASK, "zoom.in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Add, GDK_CONTROL_MASK, "zoom.in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_equal, GDK_CONTROL_MASK, "zoom.in", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_minus, GDK_CONTROL_MASK, "zoom.out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Subtract, GDK_CONTROL_MASK, "zoom.out", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_0, GDK_CONTROL_MASK, "zoom.one", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_j, GDK_CONTROL_MASK, "selection.join", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_j, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "selection.sort", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_l, GDK_CONTROL_MASK, "buffer.select-line", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_d, GDK_CONTROL_MASK, "buffer.delete-line", NULL);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_d, GDK_CONTROL_MASK|GDK_ALT_MASK, "buffer.duplicate-line", NULL);
}

static void
ide_source_view_init (IdeSourceView *self)
{
  GtkStyleContext *style_context;
  GtkEventController *click;
  GtkEventController *focus;
  GtkEventController *scroll;
  GtkEventController *key;

  self->controllers = g_array_new (FALSE, FALSE, sizeof (Controller));
  g_array_set_clear_func (self->controllers, controller_clear);

  g_signal_connect (self,
                    "notify::buffer",
                    G_CALLBACK (ide_source_view_notify_buffer_cb),
                    NULL);

  /* Setup our extra menu so that consumers can use
   * ide_source_view_append_menu() or similar to update menus.
   */
  self->joined_menu = ide_joined_menu_new ();
  gtk_text_view_set_extra_menu (GTK_TEXT_VIEW (self),
                                G_MENU_MODEL (self->joined_menu));

  /* Setup a handler to emit ::populate-menu */
  click = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  gtk_event_controller_set_name (click, "ide-source-view-click");
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), 0);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (click),
                                              GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (click,
                            "pressed",
                            G_CALLBACK (ide_source_view_click_pressed_cb),
                            self);
  ide_source_view_add_controller (self, 0, click);

  /* Key handler for internal features like insert-matching-braces */
  key = gtk_event_controller_key_new ();
  gtk_event_controller_set_name (key, "ide-source-view-key");
  gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
  g_signal_connect_swapped (key,
                            "key-pressed",
                            G_CALLBACK (ide_source_view_key_pressed_cb),
                            self);
  g_signal_connect_swapped (key,
                            "key-released",
                            G_CALLBACK (ide_source_view_key_released_cb),
                            self);
  ide_source_view_add_controller (self, 0, key);

  /* Setup focus tracking */
  focus = gtk_event_controller_focus_new ();
  gtk_event_controller_set_name (focus, "ide-source-view-focus");
  g_signal_connect_swapped (focus,
                            "enter",
                            G_CALLBACK (ide_source_view_focus_enter_cb),
                            self);
  g_signal_connect_swapped (focus,
                            "leave",
                            G_CALLBACK (ide_source_view_focus_leave_cb),
                            self);
  ide_source_view_add_controller (self, 0, focus);

  /* Setup ctrl+scroll zoom */
  scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
  gtk_event_controller_set_name (scroll, "ide-source-view-zoom");
  gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
  g_signal_connect (scroll,
                    "scroll",
                    G_CALLBACK (on_scroll_scrolled_cb),
                    self);
  g_signal_connect (scroll,
                    "scroll-begin",
                    G_CALLBACK (on_scroll_begin_cb),
                    self);
  g_signal_connect (scroll,
                    "scroll-end",
                    G_CALLBACK (on_scroll_end_cb),
                    self);
  ide_source_view_add_controller (self, 0, scroll);

  /* This is sort of a layer vioaltion, but it's helpful for us to
   * get the system font name and manage it invisibly.
   */
  g_signal_connect_object (g_application_get_default (),
                           "notify::system-font-name",
                           G_CALLBACK (ide_source_view_update_css),
                           self,
                           G_CONNECT_SWAPPED);

  /* Setup the CSS provider for the custom font/scale/etc. */
  self->css_provider = gtk_css_provider_new ();
  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_provider (style_context,
                                  GTK_STYLE_PROVIDER (self->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  tweak_gutter_spacing (GTK_SOURCE_VIEW (self));
}

void
ide_source_view_jump_to_insert (IdeSourceView *self)
{
  GtkTextBuffer *buffer;
  GtkTextMark *mark;
  GtkTextIter iter;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  mark = gtk_text_buffer_get_insert (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
  ide_source_view_jump_to_iter (GTK_TEXT_VIEW (self), &iter, .25, TRUE, 1.0, 0.5);
}

void
ide_source_view_scroll_to_insert (IdeSourceView    *self,
                                  GtkDirectionType  dir)
{
  GtkTextMark *insert;
  GdkRectangle visible_rect;
  GtkTextIter search_result_iter, last_line_iter;
  int search_result_line, last_visible_line;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (self->buffer));

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), &search_result_iter, insert);

  gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (self), &visible_rect);
  gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (self), &last_line_iter,
                               visible_rect.y + visible_rect.height, NULL);

  search_result_line = gtk_text_iter_get_line (&search_result_iter);
  last_visible_line = gtk_text_iter_get_line (&last_line_iter);

  if (dir == GTK_DIR_TAB_FORWARD && search_result_line > last_visible_line)
    gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self), &search_result_iter, 0.0, TRUE, 0.5, 0.15);
  else
    gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self), &search_result_iter, 0.15, FALSE, .0, .0);
}

void
ide_source_view_get_visual_position (IdeSourceView *self,
                                     guint         *line,
                                     guint         *line_column)
{
  GtkTextIter insert;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  ide_buffer_get_selection_bounds (self->buffer, &insert, NULL);

  if (line)
    *line = gtk_text_iter_get_line (&insert);

  if (line_column)
    *line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self), &insert);
}

void
ide_source_view_get_visual_position_range (IdeSourceView *self,
                                           guint         *line,
                                           guint         *line_column,
                                           guint         *range)
{
  GtkTextIter insert;
  GtkTextIter selection;
  int insert_line;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  ide_buffer_get_selection_bounds (self->buffer, &insert, &selection);

  insert_line = gtk_text_iter_get_line (&insert);

  if (line)
    *line = insert_line;

  if (line_column)
    *line_column = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (self), &insert);

  if (range != NULL)
    {
      int selection_line;
      int insert_offset = gtk_text_iter_get_offset (&insert);
      int selection_offset = gtk_text_iter_get_offset (&selection);

      /* Since insert or selection iterator is be located one symbol away from selection
       * we have to adjust it before calculating multiline range
       */
      if (insert_offset > selection_offset)
        {
          gtk_text_iter_backward_char (&insert);
        }
      else if (insert_offset < selection_offset)
        {
          gtk_text_iter_backward_char (&selection);
        }
      selection_line = gtk_text_iter_get_line (&selection);
      insert_line = gtk_text_iter_get_line (&insert);

      if (insert_line != selection_line)
        {
          *range = ABS (selection_line - insert_line) + 1;
        }
      else
        {
          *range = ABS (selection_offset - insert_offset);
        }
    }
}

char *
ide_source_view_dup_position_label (IdeSourceView *self)
{
  guint line;
  guint column;
  guint range;

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  ide_source_view_get_visual_position_range (self, &line, &column, &range);

  if (range == 0)
    return g_strdup_printf (_("Ln %u, Col %u"), line + 1, column + 1);
  else
    return g_strdup_printf (_("Ln %u, Col %u (Sel: %u)"), line + 1, column + 1, range);
}

const PangoFontDescription *
ide_source_view_get_font_desc (IdeSourceView *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), NULL);

  return self->font_desc;
}

void
ide_source_view_set_font_desc (IdeSourceView              *self,
                               const PangoFontDescription *font_desc)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  if (self->font_desc == font_desc ||
      (self->font_desc != NULL && font_desc != NULL &&
       pango_font_description_equal (self->font_desc, font_desc)))
    return;

  g_clear_pointer (&self->font_desc, pango_font_description_free);

  if (font_desc)
    self->font_desc = pango_font_description_copy (font_desc);

  self->font_scale = 0;

  ide_source_view_update_css (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_DESC]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FONT_SCALE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ZOOM_LEVEL]);

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

double
ide_source_view_get_zoom_level (IdeSourceView *self)
{
  int alt_size;
  int size = 11; /* 11pt */

  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), 0);

  if (self->font_desc != NULL &&
      pango_font_description_get_set_fields (self->font_desc) & PANGO_FONT_MASK_SIZE)
    size = pango_font_description_get_size (self->font_desc) / PANGO_SCALE;

  alt_size = MAX (1, size + self->font_scale);

  return (double)alt_size / (double)size;
}

void
ide_source_view_prepend_menu (IdeSourceView *self,
                              GMenuModel    *menu_model)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model));

  ide_joined_menu_prepend_menu (self->joined_menu, menu_model);
}

void
ide_source_view_append_menu (IdeSourceView *self,
                             GMenuModel    *menu_model)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model));

  ide_joined_menu_append_menu (self->joined_menu, menu_model);
}

void
ide_source_view_remove_menu (IdeSourceView *self,
                             GMenuModel    *menu_model)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (G_IS_MENU_MODEL (menu_model));

  ide_joined_menu_remove_menu (self->joined_menu, menu_model);
}

/**
 * ide_source_view_jump_to_iter:
 *
 * The goal of this function is to be like gtk_text_view_scroll_to_iter() but
 * without any of the scrolling animation. We use it to move to a position
 * when animations would cause additional distractions.
 */
void
ide_source_view_jump_to_iter (GtkTextView       *text_view,
                              const GtkTextIter *iter,
                              double             within_margin,
                              gboolean           use_align,
                              double             xalign,
                              double             yalign)
{
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  GdkRectangle rect;
  GdkRectangle screen;
  int xvalue = 0;
  int yvalue = 0;
  int scroll_dest;
  int screen_bottom;
  int screen_right;
  int screen_xoffset;
  int screen_yoffset;
  int current_x_scroll;
  int current_y_scroll;
  int top_margin;

  /*
   * Many parts of this function were taken from gtk_text_view_scroll_to_iter ()
   * https://developer.gnome.org/gtk3/stable/GtkTextView.html#gtk-text-view-scroll-to-iter
   */

  g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (within_margin >= 0.0 && within_margin <= 0.5);
  g_return_if_fail (xalign >= 0.0 && xalign <= 1.0);
  g_return_if_fail (yalign >= 0.0 && yalign <= 1.0);

  g_object_get (text_view, "top-margin", &top_margin, NULL);

  hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (text_view));
  vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (text_view));

  gtk_text_view_get_iter_location (text_view, iter, &rect);
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

  gtk_adjustment_set_value (hadj, xvalue);
  gtk_adjustment_set_value (vadj, yvalue + top_margin);
}

gboolean
ide_source_view_get_highlight_current_line (IdeSourceView *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return self->highlight_current_line;
}

void
ide_source_view_set_highlight_current_line (IdeSourceView *self,
                                            gboolean       highlight_current_line)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  highlight_current_line = !!highlight_current_line;

  if (highlight_current_line != self->highlight_current_line)
    {
      self->highlight_current_line = highlight_current_line;

      if (gtk_widget_has_focus (GTK_WIDGET (self)))
        gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self),
                                                    highlight_current_line);
      else
        g_object_notify (G_OBJECT (self), "highlight-current-line");
    }
}

void
ide_source_view_get_iter_at_visual_position (IdeSourceView *self,
                                             GtkTextIter   *iter,
                                             guint          line,
                                             guint          line_offset)
{
  guint tab_width;
  guint pos = 0;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (iter != NULL);

  tab_width = gtk_source_view_get_tab_width (GTK_SOURCE_VIEW (self));
  gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (self->buffer), iter, line);

  while (pos < line_offset)
    {
      gunichar ch = gtk_text_iter_get_char (iter);

      switch (ch)
        {
        case '\t':
          pos += tab_width - (pos % tab_width);
          break;

        case 0:
        case '\n':
          return;

        default:
          pos++;
          break;
        }

      gtk_text_iter_forward_char (iter);
    }
}

gboolean
ide_source_view_get_insert_matching_brace (IdeSourceView *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return self->insert_matching_brace;
}

void
ide_source_view_set_insert_matching_brace (IdeSourceView *self,
                                           gboolean       insert_matching_brace)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  insert_matching_brace = !!insert_matching_brace;

  if (insert_matching_brace != self->insert_matching_brace)
    {
      self->insert_matching_brace = insert_matching_brace;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_INSERT_MATCHING_BRACE]);
    }
}

gboolean
ide_source_view_get_overwrite_braces (IdeSourceView *self)
{
  g_return_val_if_fail (IDE_IS_SOURCE_VIEW (self), FALSE);

  return self->overwrite_braces;
}

void
ide_source_view_set_overwrite_braces (IdeSourceView *self,
                                      gboolean       overwrite_braces)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));

  overwrite_braces = !!overwrite_braces;

  if (overwrite_braces != self->overwrite_braces)
    {
      self->overwrite_braces = overwrite_braces;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_OVERWRITE_BRACES]);
    }
}

static int
sort_by_priority (gconstpointer a,
                  gconstpointer b)
{
  const Controller *ca = a;
  const Controller *cb = b;

  if (ca->priority < cb->priority)
    return -1;
  else if (ca->priority > cb->priority)
    return 1;
  else
    return 0;
}

/**
 * ide_source_view_add_controller:
 * @self: a #IdeSourceView
 * @priority: the sort priority
 * @controller: (transfer full): a #GtkEventController
 *
 * Adds a controller with a priority so that capture/bubble can be
 * applied in a known order.
 */
void
ide_source_view_add_controller (IdeSourceView      *self,
                                int                 priority,
                                GtkEventController *controller)
{
  Controller to_add = { controller, priority };

  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  for (guint i = 0; i < self->controllers->len; i++)
    {
      const Controller *c = &g_array_index (self->controllers, Controller, i);
      gtk_widget_remove_controller (GTK_WIDGET (self), c->controller);
    }

  g_array_append_val (self->controllers, to_add);
  g_array_sort (self->controllers, sort_by_priority);

  for (guint i = 0; i < self->controllers->len; i++)
    {
      const Controller *c = &g_array_index (self->controllers, Controller, i);
      gtk_widget_add_controller (GTK_WIDGET (self), g_object_ref (c->controller));
    }
}

void
ide_source_view_remove_controller (IdeSourceView      *self,
                                   GtkEventController *controller)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  gtk_widget_remove_controller (GTK_WIDGET (self), controller);

  if (self->controllers == NULL)
    return;

  for (guint i = 0; i < self->controllers->len; i++)
    {
      const Controller *c = &g_array_index (self->controllers, Controller, i);

      if (c->controller == controller)
        {
          g_array_remove_index (self->controllers, i);
          break;
        }
    }
}

void
_ide_source_view_set_search_context (IdeSourceView          *self,
                                     GtkSourceSearchContext *search_context)
{
  g_return_if_fail (IDE_IS_SOURCE_VIEW (self));
  g_return_if_fail (!search_context || GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  if (g_set_object (&self->search_context, search_context))
    gtk_widget_queue_draw (GTK_WIDGET (self));
}
