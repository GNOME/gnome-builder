/* ide-light-bulb.c
 *
 * Copyright 2021 Georg Vienna <georg.vienna@himbarsoft.com>
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

#define G_LOG_DOMAIN "ide-light-bulb"

#define CURSOR_SETTLE_TIMEOUT_MSEC 250

#include "config.h"

#include <dazzle.h>
#include <libide-code.h>
#include <libide-gui.h>
#include <string.h>

#include "ide-light-bulb-private.h"

struct _IdeLightBulb
{
  GtkEventBox parent_instance;

  /*
   * This is our cancellable to cancel any in-flight requests to the
   * code action providers when the cursor is moved. That could happen
   * before we've even really been displayed to the user.
   */
  GCancellable *cancellable;

  /*
   * Our source which is continually delayed until the cursor moved event has
   * settled somewhere we can potentially query for code actions.
   */
  guint delay_query_source;

  IdeSourceView* source_view;

  GtkWidget* popup_menu;

  GtkTextIter* last_range_start;
  GtkTextIter* last_range_end;
};

G_DEFINE_FINAL_TYPE (IdeLightBulb, ide_light_bulb, GTK_TYPE_EVENT_BOX)

static void
ide_light_bulb_realize (GtkWidget *widget)
{
  GdkWindow *window;
  GdkDisplay *display;

  g_autoptr(GdkCursor) cursor = NULL;

  g_assert (GTK_IS_WIDGET (widget));

  GTK_WIDGET_CLASS (ide_light_bulb_parent_class)->realize (widget);

  window = gtk_widget_get_window (GTK_WIDGET (widget));
  display = gtk_widget_get_display (GTK_WIDGET (widget));
  cursor = gdk_cursor_new_from_name (display, "default");

  gdk_window_set_events (window, GDK_ALL_EVENTS_MASK);
  gdk_window_set_cursor (window, cursor);
}

static void
ide_light_bulb_destroy (GtkWidget *widget)
{
  IdeLightBulb *self = (IdeLightBulb *)widget;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  dzl_clear_source (&self->delay_query_source);

  if (self->last_range_start)
    {
      gtk_text_iter_free (self->last_range_start);
    }
  if (self->last_range_end)
    {
      gtk_text_iter_free (self->last_range_end);
    }

  GTK_WIDGET_CLASS (ide_light_bulb_parent_class)->destroy (widget);
}

static void
ide_light_bulb_class_init (IdeLightBulbClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->realize = ide_light_bulb_realize;
  widget_class->destroy = ide_light_bulb_destroy;
}

static void
button_clicked_cb (IdeLightBulb *self,
                   GtkButton    *button)
{
  gtk_menu_popup_at_widget (GTK_MENU (self->popup_menu),
                            GTK_WIDGET (button),
                            GDK_GRAVITY_SOUTH_EAST,
                            GDK_GRAVITY_NORTH_WEST,
                            NULL);
}

static void
popup_menu_hide_cb (GtkButton    *button,
                    GtkMenu      *menu)
{
  gtk_widget_unset_state_flags (GTK_WIDGET (button), GTK_STATE_FLAG_PRELIGHT);
}

static void
ide_light_bulb_init (IdeLightBulb *self)
{
  GtkStyleContext *style_context;
  GtkWidget* button;
  GtkWidget* box;
  GtkWidget* bulb;
  GtkWidget* arrow;

  self->cancellable = g_cancellable_new ();

  style_context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style_context, "light-bulb");

  button = gtk_button_new ();
  gtk_widget_set_focus_on_click (button, FALSE);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (self), button);

  g_signal_connect_swapped (button,
                            "clicked",
                            G_CALLBACK (button_clicked_cb),
                            self);

  gtk_container_set_border_width (GTK_CONTAINER (self), 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (button), box);

  bulb = gtk_image_new_from_icon_name ("dialog-information-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (bulb), 12);
  gtk_widget_show (bulb);
  gtk_container_add (GTK_CONTAINER (box), bulb);

  arrow = gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_image_set_pixel_size (GTK_IMAGE (arrow), 12);
  gtk_widget_show (arrow);
  gtk_container_add (GTK_CONTAINER (box), arrow);

  self->popup_menu = gtk_menu_new ();
  g_object_set (self->popup_menu,
                "rect-anchor-dx", -10,
                "rect-anchor-dy", -10,
                NULL);
  g_signal_connect_swapped (self->popup_menu, "hide",
                            G_CALLBACK (popup_menu_hide_cb),
                            button);

  gtk_style_context_add_class (gtk_widget_get_style_context (self->popup_menu),
                               GTK_STYLE_CLASS_CONTEXT_MENU);

  gtk_menu_attach_to_widget (GTK_MENU (self->popup_menu),
                             GTK_WIDGET (self),
                             NULL);
}

static void
execute_code_action_cb (IdeCodeAction *code_action)
{
  ide_code_action_execute_async (code_action,
                                 NULL,
                                 NULL,
                                 code_action);
}

static void
ide_light_bulb_place_at_iter (IdeLightBulb* self, GtkTextIter* iter_cursor)
{
  g_autoptr(GtkTextIter) iter = NULL;
  g_autoptr(GtkTextIter) iter_end = NULL;
  GdkRectangle rect;
  guint chars_in_line;
  g_autofree gchar* first_chars_in_line = NULL;
  gboolean place_above_iter;
  gint button_height = 24;
  gint y_pos = 0;

  iter = gtk_text_iter_copy (iter_cursor);

  /* move iter to first position */
  gtk_text_iter_set_line_offset (iter, 0);

  chars_in_line = gtk_text_iter_get_chars_in_line (iter);
  iter_end = gtk_text_iter_copy (iter);
  gtk_text_iter_set_line_offset (iter_end, MIN (chars_in_line - 1, 4));
  first_chars_in_line = gtk_text_iter_get_text (iter, iter_end);
  place_above_iter = !ide_str_empty0 (g_strstrip (first_chars_in_line));

  if (place_above_iter)
    {
      gint line = gtk_text_iter_get_line (iter);
      if (line)
        {
          gtk_text_iter_set_line (iter, line - 1);
        }
      else
        {
          gtk_text_iter_set_line (iter, line + 1);
        }
    }

  gtk_text_view_get_iter_location (GTK_TEXT_VIEW (self->source_view), iter, &rect);

  if (place_above_iter)
    {
      /* align with top of the line below */
      y_pos = rect.y + rect.height - button_height;
    }
  else
    {
      /* center verically in line */
      y_pos = rect.y + (rect.height / 2) - (button_height / 2);
    }
  gtk_text_view_move_child (GTK_TEXT_VIEW (self->source_view),
                            GTK_WIDGET (self),
                            0,
                            y_pos);
  gtk_widget_show (GTK_WIDGET (self));
}

static void
ide_light_bulb_code_action_query_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
  IdeBuffer *buffer = (IdeBuffer *)object;

  g_autoptr(IdeLightBulb) self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) code_actions = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_LIGHT_BULB (self));

  code_actions = ide_buffer_code_action_query_finish (buffer, result, &error);
  if (!code_actions)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_warning ("%s", error->message);
      IDE_EXIT;
    }

  if (code_actions->len)
    {
      GtkTextIter iter;
      g_autoptr(IdeContext) context = NULL;

      ide_buffer_get_selection_bounds (buffer, &iter, NULL);

      ide_light_bulb_place_at_iter (self, &iter);

      gtk_container_foreach (GTK_CONTAINER (self->popup_menu),
                             (GtkCallback)gtk_widget_destroy,
                             NULL);
      context = ide_buffer_ref_context (buffer);

      for (gsize i = 0; i < code_actions->len; i++)
        {
          IdeCodeAction* code_action;
          GtkWidget* menu_item;

          code_action = g_ptr_array_index (code_actions, i);
          ide_object_append (IDE_OBJECT (context), IDE_OBJECT (code_action));
          menu_item = gtk_menu_item_new_with_label (ide_code_action_get_title (code_action));

          g_signal_connect_swapped (menu_item,
                                    "activate",
                                    G_CALLBACK (execute_code_action_cb),
                                    code_action);


          gtk_widget_show (menu_item);
          gtk_menu_shell_append (GTK_MENU_SHELL (self->popup_menu), menu_item);
        }
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self));
    }
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
    } while (gtk_text_iter_forward_char (word_end));

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

static gboolean
ide_light_bulb_get_trigger_bound (IdeBuffer    *buffer,
                                  GtkTextIter **trigger_start,
                                  GtkTextIter **trigger_end)
{
  GtkTextIter insert;
  GtkTextIter selection;
  GtkTextIter insert_word_start;
  GtkTextIter insert_word_end;

  ide_buffer_get_selection_bounds (buffer, &insert, &selection);

  if (gtk_text_iter_equal (&insert, &selection))
    {
      gint chars_in_line;
      gunichar character;

      chars_in_line = gtk_text_iter_get_chars_in_line (&insert);
      if (chars_in_line == 0)
        {
          return FALSE;
        }
      character = gtk_text_iter_get_char (&insert);
      if (g_unichar_isspace (character))
        {
          return FALSE;
        }

      if (ide_source_get_word_from_iter (&insert, &insert_word_start, &insert_word_end))
        {
          *trigger_start = gtk_text_iter_copy (&insert_word_start);
          *trigger_end = gtk_text_iter_copy (&insert_word_end);
          return TRUE;
        }
    }


  if (ide_source_get_word_from_iter (&insert, &insert_word_start, &insert_word_end))
    {
      GtkTextIter selection_word_start;
      GtkTextIter selection_word_end;

      insert = insert_word_start;

      if (ide_source_get_word_from_iter (&selection, &selection_word_start, &selection_word_end))
        {
          selection = selection_word_end;
        }
    }

  *trigger_start = gtk_text_iter_copy (&insert);
  *trigger_end = gtk_text_iter_copy (&selection);

  return TRUE;
}

static gboolean
ide_light_bulb_delay_timeout_cb (gpointer data)
{
  IdeLightBulb *self = data;
  IdeBuffer* buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view)));

  self->delay_query_source = 0;

  ide_buffer_code_action_query_async (buffer,
                                      self->cancellable,
                                      ide_light_bulb_code_action_query_cb,
                                      g_object_ref (self));
  return G_SOURCE_REMOVE;
}

static
void ide_light_bulb_cancel (IdeLightBulb *self)
{
  g_autoptr(GCancellable) cancellable = NULL;

  cancellable = g_steal_pointer (&self->cancellable);
  if (!g_cancellable_is_cancelled (cancellable))
    {
      g_cancellable_cancel (cancellable);
    }
  if (self->delay_query_source)
    {
      g_source_remove (self->delay_query_source);
    }
}

void
_ide_light_bulb_show (IdeLightBulb *self)
{
  IdeBuffer* buffer;

  g_autoptr(GtkTextIter) trigger_start = NULL;
  g_autoptr(GtkTextIter) trigger_end = NULL;
  gboolean contains_word = FALSE;

  g_return_if_fail (IDE_IS_LIGHT_BULB (self));
  g_return_if_fail (GTK_IS_TEXT_VIEW (self->source_view));

  buffer = IDE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view)));

  contains_word = ide_light_bulb_get_trigger_bound (buffer, &trigger_start, &trigger_end);

  /* ignore and cancel if triggered in whitespace */
  if (!contains_word)
    {
      ide_light_bulb_cancel (self);
      if (self->last_range_start)
        {
          gtk_text_iter_free (self->last_range_start);
          self->last_range_start = NULL;
        }
      if (self->last_range_end)
        {
          gtk_text_iter_free (self->last_range_end);
          self->last_range_end = NULL;
        }
      gtk_widget_hide (GTK_WIDGET (self));
      return;
    }

  /* ignore if triggered in the same range/word again */
  if (self->last_range_start &&
      gtk_text_iter_equal (self->last_range_start, trigger_start) &&
      self->last_range_end &&
      gtk_text_iter_equal (self->last_range_end, trigger_end))
    {
      return;
    }

  /* cancel old request */
  ide_light_bulb_cancel (self);
  self->cancellable = g_cancellable_new ();

  /* safe current trigger range */
  if (self->last_range_start)
    {
      gtk_text_iter_free (self->last_range_start);
    }
  self->last_range_start = gtk_text_iter_copy (trigger_start);
  if (self->last_range_end)
    {
      gtk_text_iter_free (self->last_range_end);
    }
  self->last_range_end = gtk_text_iter_copy (trigger_end);


  self->delay_query_source =
    gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                  CURSOR_SETTLE_TIMEOUT_MSEC,
                                  ide_light_bulb_delay_timeout_cb,
                                  self,
                                  NULL);
}

IdeLightBulb *
_ide_light_bulb_new (IdeSourceView* source_view)
{
  IdeLightBulb* bulb = g_object_new (IDE_TYPE_LIGHT_BULB, NULL);

  bulb->source_view = source_view;

  gtk_text_view_add_child_in_window (GTK_TEXT_VIEW (source_view),
                                     GTK_WIDGET (g_object_ref (GTK_WIDGET (bulb))),
                                     GTK_TEXT_WINDOW_TEXT,
                                     0,
                                     100);
  return bulb;
}
