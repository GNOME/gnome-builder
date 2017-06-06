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
#include "editor/ide-editor-perspective.h"
#include "ide-editor-spell-widget.h"
#include "util/ide-gtk.h"

static void
ide_editor_frame_actions_spellcheck (GSimpleAction *action,
                                     GVariant      *variant,
                                     gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  IdeWorkbench *workbench;
  IdePerspective *editor;
  gboolean state;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  state = !!g_variant_get_int32 (variant);
  if (state == TRUE)
    {
  if (IDE_IS_SOURCE_VIEW (self->source_view) &&
      NULL != (workbench = ide_widget_get_workbench (GTK_WIDGET (self))) &&
      NULL != (editor = ide_workbench_get_perspective_by_name (workbench, "editor")))
    ide_editor_perspective_show_spellchecker (IDE_EDITOR_PERSPECTIVE (editor), self->source_view);
    }
  else
    gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
ide_editor_frame_actions_find (GSimpleAction *action,
                              GVariant      *variant,
                              gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  GtkTextBuffer *buffer;
  GtkDirectionType search_direction;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_widget_set_visible (GTK_WIDGET (self->replace_entry), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_button), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_all_button), FALSE);

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
  else
    {
      GtkSourceSearchContext *search_context;
      GtkSourceSearchSettings *search_settings;
      const gchar *search_text;

      search_context = ide_source_view_get_search_context (self->source_view);
      search_settings = gtk_source_search_context_get_settings (search_context);
      search_text = gtk_source_search_settings_get_search_text (search_settings);

      if ((search_text != NULL) && (search_text [0] != '\0'))
        gtk_entry_set_text (GTK_ENTRY (self->search_entry), search_text);
      else if (self->previous_search_string != NULL)
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

static void
ide_editor_frame_actions_toggle_search_replace (GSimpleAction *action,
                                                GVariant      *state,
                                                gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  gboolean visible;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  visible = !gtk_widget_get_visible (GTK_WIDGET (self->replace_entry));

  gtk_widget_set_visible (GTK_WIDGET (self->replace_entry), visible);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_button), visible);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_all_button), visible);
}

static void
ide_editor_frame_actions_find_replace (GSimpleAction *action,
                                       GVariant      *variant,
                                       gpointer       user_data)
{
  GActionGroup *frame_group;
  GAction *replace_options_action;
  g_autoptr (GVariant) replace_options_variant = NULL;
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  if (NULL != (frame_group = gtk_widget_get_action_group (GTK_WIDGET (self->search_frame), "search-entry")) &&
      NULL != (replace_options_action = g_action_map_lookup_action (G_ACTION_MAP (frame_group), "toggle-search-replace")))
    {
      replace_options_variant = g_variant_new_boolean (TRUE);
      ide_editor_frame_actions_find (action, variant, user_data);
      ide_editor_frame_actions_toggle_search_replace (G_SIMPLE_ACTION (replace_options_action), replace_options_variant, user_data);
    }
}

static void
ide_editor_frame_actions_toggle_search_options (GSimpleAction *action,
                                                GVariant      *state,
                                                gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  gboolean visible;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  visible = !gtk_widget_get_visible (GTK_WIDGET (self->search_options));

  gtk_widget_set_visible (GTK_WIDGET (self->search_options), visible);
}

static void
ide_editor_frame_actions_exit_search (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  GtkTextBuffer *buffer;
  GActionGroup *group;
  GAction *replace_action;
  GAction *replace_all_action;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  /* stash the search string for later */
  g_free (self->previous_search_string);
  g_object_get (self->search_entry, "text", &self->previous_search_string, NULL);

  /* disable the replace and replace all actions */
  group = gtk_widget_get_action_group (GTK_WIDGET (self->search_frame), "search-entry");
  replace_action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace");
  replace_all_action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace-all");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (replace_action), FALSE);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (replace_all_action), FALSE);

  /* clear the highlights in the source view */
  ide_source_view_clear_search (self->source_view);

  /* disable rubberbanding and ensure insert mark is on screen */
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
  ide_source_view_set_rubberband_search (self->source_view, FALSE);
  ide_source_view_scroll_mark_onscreen (self->source_view,
                                        gtk_text_buffer_get_insert (buffer),
                                        TRUE,
                                        0.5,
                                        0.5);

  /* finally we can focus the source view */
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
ide_editor_frame_actions_replace (GSimpleAction *action,
                                  GVariant      *state,
                                  gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  const gchar *replace_text;
  gchar *unescaped_replace_text;
  const gchar *search_text;
  GError *error = NULL;
  GtkTextIter start;
  GtkTextIter end;
  GtkTextBuffer *buffer;
  gint occurrence_position;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  search_context = ide_source_view_get_search_context (self->source_view);
  g_assert (search_context != NULL);
  search_settings = gtk_source_search_context_get_settings (search_context);
  search_text = gtk_source_search_settings_get_search_text (search_settings);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  if (ide_str_empty0 (search_text) || replace_text == NULL)
    return;

  unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_text);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
  gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  occurrence_position = gtk_source_search_context_get_occurrence_position (search_context, &start, &end);

  if (occurrence_position > 0)
    {
      /* Temporarily disable updating the search position label to prevent flickering */
      g_signal_handler_block (buffer, self->cursor_moved_handler);

      gtk_source_search_context_replace2 (search_context, &start, &end, unescaped_replace_text, -1, &error);

      /* Re-enable updating the search position label. The next-search-result action
       * below will cause it to update. */
      g_signal_handler_unblock (buffer, self->cursor_moved_handler);

      if (error != NULL)
        {
          g_warning ("%s", error->message);
          g_clear_error (&error);
        }

      dzl_gtk_widget_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
    }

  g_free (unescaped_replace_text);
}

static void
ide_editor_frame_actions_replace_all (GSimpleAction *action,
                                      GVariant      *state,
                                      gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  const gchar *replace_text;
  gchar *unescaped_replace_text;
  const gchar *search_text;
  GError *error = NULL;
  GtkSourceCompletion *completion;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  search_context = ide_source_view_get_search_context (self->source_view);
  g_assert (search_context != NULL);
  search_settings = gtk_source_search_context_get_settings (search_context);
  search_text = gtk_source_search_settings_get_search_text (search_settings);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  if (ide_str_empty0 (search_text) || replace_text == NULL)
    return;

  /* Temporarily disabling auto completion makes replace more efficient. */
  completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (self->source_view));
  gtk_source_completion_block_interactive (completion);

  unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_text);

  gtk_source_search_context_replace_all (search_context, unescaped_replace_text, -1, &error);

  gtk_source_completion_unblock_interactive (completion);

  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
    }

  g_free (unescaped_replace_text);
}

static void
ide_editor_frame_actions_replace_confirm (GSimpleAction *action,
                                          GVariant      *state,
                                          gpointer       user_data)
{
  IdeEditorFrame *self = user_data;
  g_autofree const gchar **strv = NULL;
  gsize array_length;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (state != NULL);
  g_assert (g_variant_is_of_type (state, G_VARIANT_TYPE_STRING_ARRAY));

  strv = g_variant_get_strv (state, &array_length);
  g_assert (array_length >= 2);

  gtk_entry_set_text (GTK_ENTRY (self->search_entry), strv[0]);
  gtk_entry_set_text (GTK_ENTRY (self->replace_entry), strv[1]);

  gtk_widget_show (GTK_WIDGET (self->replace_entry));
  gtk_widget_show (GTK_WIDGET (self->replace_button));
  gtk_widget_show (GTK_WIDGET (self->replace_all_button));

  /* increment pending_replace_confirm so that search_revealer_on_child_revealed_changed
   * will know to go to the next search result (the occurrence only stays selected after
   * search_entry has been mapped).
   */
  self->pending_replace_confirm++;

  gtk_revealer_set_reveal_child (self->search_revealer, TRUE);

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static const GActionEntry IdeEditorFrameActions[] = {
  { "find", ide_editor_frame_actions_find, "i" },
  { "find-replace", ide_editor_frame_actions_find_replace, "i" },
  { "next-search-result", ide_editor_frame_actions_next_search_result },
  { "previous-search-result", ide_editor_frame_actions_previous_search_result },
  { "replace-confirm", ide_editor_frame_actions_replace_confirm, "as" },
  { "show-spellcheck", ide_editor_frame_actions_spellcheck, "i" },
};

static const GActionEntry IdeEditorFrameSearchActions[] = {
  { "cut-clipboard", ide_editor_frame_actions_cut_clipboard, },
  { "copy-clipboard", ide_editor_frame_actions_copy_clipboard, },
  { "paste-clipboard", ide_editor_frame_actions_paste_clipboard, },
  { "delete-selection", ide_editor_frame_actions_delete_selection, },
  { "select-all", ide_editor_frame_actions_select_all },
  { "toggle-search-replace", NULL, "b", "false", ide_editor_frame_actions_toggle_search_replace },
  { "toggle-search-options", NULL, "b", "false", ide_editor_frame_actions_toggle_search_options },
  { "exit-search", ide_editor_frame_actions_exit_search },
  { "replace", ide_editor_frame_actions_replace },
  { "replace-all", ide_editor_frame_actions_replace_all },
};

void
ide_editor_frame_actions_init (IdeEditorFrame *self)
{
  GSimpleActionGroup *group;
  GAction *action;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeEditorFrameActions,
                                   G_N_ELEMENTS (IdeEditorFrameActions), self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "frame", G_ACTION_GROUP (group));
  g_object_unref (group);

  group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (group), IdeEditorFrameSearchActions,
                                   G_N_ELEMENTS (IdeEditorFrameSearchActions), self);

  /* Disable replace and replace-all by default; they should only be enabled
   * when the corresponding operations would make sense.
   */
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace-all");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  gtk_widget_insert_action_group (GTK_WIDGET (self->search_frame), "search-entry", G_ACTION_GROUP (group));
  g_object_unref (group);
}
