/* ide-editor-frame-actions.c
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

#include <gtksourceview/gtksource.h>

#include "ide-editor-frame-actions.h"
#include "ide-editor-frame-private.h"

static void
ide_editor_frame_actions_find (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  GtkTextBuffer *buffer;
  GtkTextIter start_sel;
  GtkTextIter end_sel;
  GtkDirectionType search_direction;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  search_direction = (GtkDirectionType) g_variant_get_int32 (variant);
  ide_source_view_set_search_direction (self->source_view,
                                        search_direction);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  /*
   * If the buffer currently has a selection, we prime the search entry with the
   * selected text. If not, we use our previous search text in the case that it was
   * cleared by the IdeSourceView internal state.
   */

  if (gtk_text_buffer_get_has_selection (buffer))
    {
      gtk_text_buffer_get_selection_bounds (buffer, &start_sel, &end_sel);

      if (gtk_text_iter_get_line (&start_sel) == gtk_text_iter_get_line (&end_sel))
        {
          const gchar *selected_text;

          selected_text = gtk_text_buffer_get_text (buffer, &start_sel, &end_sel, FALSE);
          gtk_entry_set_text (GTK_ENTRY (self->search_entry), selected_text);
        }
    }
  else if (self->previous_search_string != NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (self->search_entry), self->previous_search_string);
    }

  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
ide_editor_frame_actions_next_search_result (GSimpleAction *action,
                                            GVariant      *variant,
                                            gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  ide_source_view_set_rubberband_search (self->source_view, FALSE);

  IDE_SOURCE_VIEW_GET_CLASS (self->source_view)->move_search
    (self->source_view, GTK_DIR_DOWN, FALSE, TRUE, TRUE, FALSE, FALSE);
}

static void
ide_editor_frame_actions_previous_search_result (GSimpleAction *action,
                                                GVariant      *variant,
                                                gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  ide_source_view_set_rubberband_search (self->source_view, FALSE);

  IDE_SOURCE_VIEW_GET_CLASS (self->source_view)->move_search
    (self->source_view, GTK_DIR_UP, FALSE, TRUE, TRUE, FALSE, FALSE);
}

static const GActionEntry IdeEditorFrameActions[] = {
  { "find", ide_editor_frame_actions_find, "i" },
  { "next-search-result", ide_editor_frame_actions_next_search_result },
  { "previous-search-result", ide_editor_frame_actions_previous_search_result },
};

void
ide_editor_frame_actions_init (IdeEditorFrame *self)
{
  g_autoptr(GSimpleActionGroup) group = NULL;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeEditorFrameActions,
                                   G_N_ELEMENTS (IdeEditorFrameActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "frame", G_ACTION_GROUP (group));
}
