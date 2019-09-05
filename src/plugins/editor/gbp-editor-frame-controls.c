/* gbp-editor-frame-controls.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-editor-frame-controls"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>

#define IDE_EDITOR_INSIDE
#include "ide-editor-private.h"
#undef IDE_EDITOR_INSIDE

#include "gbp-editor-frame-controls.h"


G_DEFINE_TYPE (GbpEditorFrameControls, gbp_editor_frame_controls, GTK_TYPE_BOX)

static void
document_cursor_moved (GbpEditorFrameControls *self,
                       const GtkTextIter      *iter,
                       GtkTextBuffer          *buffer)
{
  IdeSourceView *source_view;
  GtkTextIter bounds;
  GtkTextMark *mark;
  gchar str[32];
  guint line;
  gint column;
  gint column2;

  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (iter != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->page == NULL)
    return;

  if (ide_buffer_get_loading (IDE_BUFFER (buffer)))
    return;

  source_view = ide_editor_page_get_view (self->page);

  ide_source_view_get_visual_position (source_view, &line, (guint *)&column);

  mark = gtk_text_buffer_get_selection_bound (buffer);
  gtk_text_buffer_get_iter_at_mark (buffer, &bounds, mark);

  g_snprintf (str, sizeof str, "%d", line + 1);
  dzl_simple_label_set_label (self->line_label, str);

  g_snprintf (str, sizeof str, "%d", column + 1);
  dzl_simple_label_set_label (self->column_label, str);

  if (!gtk_widget_has_focus (GTK_WIDGET (source_view)) ||
      gtk_text_iter_equal (&bounds, iter) ||
      (gtk_text_iter_get_line (iter) != gtk_text_iter_get_line (&bounds)))
    {
      gtk_widget_set_visible (GTK_WIDGET (self->range_label), FALSE);
      return;
    }

  /* We have a selection that is on the same line.
   * Lets give some detail as to how long the selection is.
   */
  column2 = gtk_source_view_get_visual_column (GTK_SOURCE_VIEW (source_view), &bounds);

  g_snprintf (str, sizeof str, "%u", ABS (column2 - column));
  gtk_label_set_label (self->range_label, str);
  gtk_widget_set_visible (GTK_WIDGET (self->range_label), TRUE);
}


static void
goto_line_activate (GbpEditorFrameControls *self,
                    const gchar            *text,
                    DzlSimplePopover       *popover)
{
  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (DZL_IS_SIMPLE_POPOVER (popover));

  if (self->page == NULL)
    return;

  if (!dzl_str_empty0 (text))
    {
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (self->page->buffer);
      IdeSourceView *source_view = self->page->source_view;
      GtkTextIter iter;
      gint64 value;
      gint line;

      gtk_text_buffer_get_end_iter (buffer, &iter);
      line = gtk_text_iter_get_line (&iter) + 1;
      value = g_ascii_strtoll (text, NULL, 10);

      if (value == 0)
        {
          return;
        }
      else if (value > 0)
        {
          line = MIN (line, value);
        }
      else if (value < 0)
        {
          value++;
          line = MAX (line + value, 1);
        }

      gtk_widget_grab_focus (GTK_WIDGET (self->page));
      gtk_text_buffer_get_iter_at_line (buffer, &iter, line - 1);
      gtk_text_buffer_select_range (buffer, &iter, &iter);
      ide_source_view_scroll_to_iter (source_view, &iter, 0.25, TRUE, 1.0, 0.5, TRUE);
    }
}

static gboolean
goto_line_insert_text (GbpEditorFrameControls *self,
                       guint                   position,
                       const gchar            *chars,
                       guint                   n_chars,
                       DzlSimplePopover       *popover)
{
  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (DZL_IS_SIMPLE_POPOVER (popover));
  g_assert (chars != NULL);

  for (; *chars; chars = g_utf8_next_char (chars))
    {
      gunichar ch = g_utf8_get_char (chars);

      if (!g_unichar_isdigit (ch) && ch != '-')
        return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
goto_line_changed (GbpEditorFrameControls *self,
                   DzlSimplePopover       *popover)
{
  g_autofree gchar *message = NULL;
  const gchar *text;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (DZL_IS_SIMPLE_POPOVER (popover));

  if (self->page == NULL)
    return;

  text = dzl_simple_popover_get_text (popover);

  gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (self->page->buffer), &begin, &end);

  if (!dzl_str_empty0 (text))
    {
      gint64 value = g_ascii_strtoll (text, NULL, 10);

      if (value != 0)
        {
          dzl_simple_popover_set_message (popover, NULL);
          dzl_simple_popover_set_ready (popover, TRUE);
          return;
        }
    }

  /* translators: the user selected a number outside the value range for the document. */
  message = g_strdup_printf (_("Provide a number between 1 and %u"),
                             gtk_text_iter_get_line (&end) + 1);
  dzl_simple_popover_set_message (popover, message);
  dzl_simple_popover_set_ready (popover, FALSE);
}

static void
warning_button_clicked (GbpEditorFrameControls *self,
                        GtkButton              *button)
{
  IdeSourceView *source_view;

  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (GTK_IS_BUTTON (button));

  if (self->page == NULL)
    return;

  source_view = ide_editor_page_get_view (self->page);
  gtk_widget_grab_focus (GTK_WIDGET (source_view));
  g_signal_emit_by_name (source_view, "move-error", GTK_DIR_DOWN);
}

static void
show_goto_line (GSimpleAction          *action,
                GVariant               *param,
                GbpEditorFrameControls *self)
{
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->goto_line_button), TRUE);
}

static void
gbp_editor_frame_controls_bind (GbpEditorFrameControls *self,
                                GtkTextBuffer          *buffer,
                                DzlSignalGroup         *buffer_signals)
{
  GtkTextIter iter;

  g_assert (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_assert (IDE_IS_BUFFER (buffer));
  g_assert (DZL_IS_SIGNAL_GROUP (buffer_signals));

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, gtk_text_buffer_get_insert (buffer));
  document_cursor_moved (self, &iter, buffer);
}

static void
gbp_editor_frame_controls_finalize (GObject *object)
{
  GbpEditorFrameControls *self = (GbpEditorFrameControls *)object;

  g_clear_object (&self->buffer_bindings);
  g_clear_object (&self->buffer_signals);
  g_clear_object (&self->goto_line_action);

  self->page = NULL;

  G_OBJECT_CLASS (gbp_editor_frame_controls_parent_class)->finalize (object);
}

static void
gbp_editor_frame_controls_class_init (GbpEditorFrameControlsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gbp_editor_frame_controls_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/editor/gbp-editor-frame-controls.ui");
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, column_label);
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, goto_line_popover);
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, goto_line_button);
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, line_label);
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, range_label);
  gtk_widget_class_bind_template_child (widget_class, GbpEditorFrameControls, warning_button);
}

static void
gbp_editor_frame_controls_init (GbpEditorFrameControls *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->goto_line_popover,
                           "activate",
                           G_CALLBACK (goto_line_activate),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->goto_line_popover,
                           "insert-text",
                           G_CALLBACK (goto_line_insert_text),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->goto_line_popover,
                           "changed",
                           G_CALLBACK (goto_line_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->warning_button,
                           "clicked",
                           G_CALLBACK (warning_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);

  self->buffer_bindings = dzl_binding_group_new ();

  dzl_binding_group_bind (self->buffer_bindings, "has-diagnostics",
                          self->warning_button, "visible",
                          G_BINDING_SYNC_CREATE);

  self->buffer_signals = dzl_signal_group_new (IDE_TYPE_BUFFER);

  g_signal_connect_swapped (self->buffer_signals,
                            "bind",
                            G_CALLBACK (gbp_editor_frame_controls_bind),
                            self);

  dzl_signal_group_connect_object (self->buffer_signals,
                                   "cursor-moved",
                                   G_CALLBACK (document_cursor_moved),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->goto_line_action = g_simple_action_new ("goto-line", NULL);
  g_signal_connect_object (self->goto_line_action,
                           "activate",
                           G_CALLBACK (show_goto_line),
                           self,
                           0);
}

void
gbp_editor_frame_controls_set_page (GbpEditorFrameControls *self,
                                    IdeEditorPage          *page)
{
  GActionGroup *editor_page_group;

  g_return_if_fail (GBP_IS_EDITOR_FRAME_CONTROLS (self));
  g_return_if_fail (!page || IDE_IS_EDITOR_PAGE (page));

  if (self->page == page)
    return;

  dzl_binding_group_set_source (self->buffer_bindings, NULL);
  dzl_signal_group_set_target (self->buffer_signals, NULL);

  if (self->page != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->page,
                                            G_CALLBACK (gtk_widget_destroyed),
                                            &self->page);
      self->page = NULL;
    }

  if (page != NULL)
    {
      self->page = page;
      g_signal_connect (page,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->page);
      dzl_binding_group_set_source (self->buffer_bindings, page->buffer);
      dzl_signal_group_set_target (self->buffer_signals, page->buffer);

      if (NULL != (editor_page_group = gtk_widget_get_action_group (GTK_WIDGET (page), "editor-page")))
        g_action_map_add_action (G_ACTION_MAP (editor_page_group), G_ACTION (self->goto_line_action));
    }
}
