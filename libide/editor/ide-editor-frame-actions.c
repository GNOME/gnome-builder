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
      GtkTextIter start_sel;
      GtkTextIter end_sel;
      g_autofree gchar *selected_text = NULL;
      g_autofree gchar *escaped_selected_text = NULL;
      GtkSourceSearchContext *search_context;
      GtkSourceSearchSettings *search_settings;

      gtk_text_buffer_get_selection_bounds (buffer, &start_sel, &end_sel);
      selected_text = gtk_text_buffer_get_text (buffer, &start_sel, &end_sel, FALSE);

      search_context = ide_source_view_get_search_context (self->source_view);
      search_settings = gtk_source_search_context_get_settings (search_context);

      if (gtk_source_search_settings_get_regex_enabled (search_settings))
        escaped_selected_text = g_regex_escape_string (selected_text, -1);
      else
        escaped_selected_text = gtk_source_utils_escape_search_text (selected_text);

      gtk_entry_set_text (GTK_ENTRY (self->search_entry), escaped_selected_text);
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
    (self->source_view, GTK_DIR_DOWN, FALSE, TRUE, TRUE, FALSE, -1);
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
    (self->source_view, GTK_DIR_UP, FALSE, TRUE, TRUE, FALSE, -1);
}

static void
ide_editor_frame_actions_cut_clipboard (GSimpleAction *action,
                                        GVariant      *state,
                                        gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_editable_cut_clipboard (GTK_EDITABLE (self->search_entry));
}

static void
ide_editor_frame_actions_copy_clipboard (GSimpleAction *action,
                                         GVariant      *state,
                                         gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_editable_copy_clipboard (GTK_EDITABLE (self->search_entry));
}

static void
ide_editor_frame_actions_paste_clipboard (GSimpleAction *action,
                                          GVariant      *state,
                                          gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_editable_paste_clipboard (GTK_EDITABLE (self->search_entry));
}

static void
ide_editor_frame_actions_delete_selection (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_editable_delete_selection (GTK_EDITABLE (self->search_entry));
}

static void
ide_editor_frame_actions_select_all (GSimpleAction *action,
                                     GVariant      *state,
                                     gpointer       user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_editable_select_region (GTK_EDITABLE (self->search_entry), 0, -1);
}

static const GActionEntry IdeEditorFrameActions[] = {
  { "find", ide_editor_frame_actions_find, "i" },
  { "next-search-result", ide_editor_frame_actions_next_search_result },
  { "previous-search-result", ide_editor_frame_actions_previous_search_result },
};

static const GActionEntry IdeEditorFrameSearchActions[] = {
  { "cut-clipboard", ide_editor_frame_actions_cut_clipboard, },
  { "copy-clipboard", ide_editor_frame_actions_copy_clipboard, },
  { "paste-clipboard", ide_editor_frame_actions_paste_clipboard, },
  { "delete-selection", ide_editor_frame_actions_delete_selection, },
  { "select-all", ide_editor_frame_actions_select_all },
};

void
ide_editor_frame_actions_init (IdeEditorFrame *self)
{
  GSimpleActionGroup *group;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeEditorFrameActions,
                                   G_N_ELEMENTS (IdeEditorFrameActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "frame", G_ACTION_GROUP (group));
  g_object_unref (group);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeEditorFrameSearchActions,
                                   G_N_ELEMENTS (IdeEditorFrameSearchActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self->search_entry), "search-entry", G_ACTION_GROUP (group));
  g_object_unref (group);
}
