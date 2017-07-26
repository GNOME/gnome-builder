/* ide-editor-search-bar-actions.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-search-bar-actions"

#include <libgd/gd-tagged-entry.h>

#include "editor/ide-editor-search-bar.h"
#include "editor/ide-editor-private.h"

static void
ide_editor_search_bar_actions_toggle_search_options (GSimpleAction *action,
                                                     GVariant      *state,
                                                     gpointer       user_data)
{
  IdeEditorSearchBar *self = user_data;
  gboolean visible;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  visible = !gtk_widget_get_visible (GTK_WIDGET (self->search_options));
  gtk_widget_set_visible (GTK_WIDGET (self->search_options), visible);
}

static void
ide_editor_search_bar_actions_toggle_search_replace (GSimpleAction *action,
                                                     GVariant      *state,
                                                     gpointer       user_data)
{
  IdeEditorSearchBar *self = user_data;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  ide_editor_search_bar_set_replace_mode (self, !ide_editor_search_bar_get_replace_mode (self));
}

static void
ide_editor_search_bar_actions_replace (GSimpleAction *action,
                                       GVariant      *state,
                                       gpointer       user_data)
{
  IdeEditorSearchBar *self = user_data;
  g_autofree gchar *unescaped_replace_text = NULL;
  g_autoptr(GError) error = NULL;
  GtkSourceBuffer *buffer;
  const gchar *replace_text;
  const gchar *search_text;
  GtkTextIter begin;
  GtkTextIter end;
  gint position;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->settings == NULL || self->context == NULL)
    return;

  search_text = gtk_source_search_settings_get_search_text (self->settings);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  if (ide_str_empty0 (search_text) || replace_text == NULL)
    return;

  unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_text);

  buffer = gtk_source_search_context_get_buffer (self->context);
  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  position = gtk_source_search_context_get_occurrence_position (self->context, &begin, &end);

  if (position > 0)
    {
      /* Temporarily disable updating the search position label to prevent flickering */
      dzl_signal_group_block (self->buffer_signals);

      gtk_source_search_context_replace2 (self->context, &begin, &end,
                                          unescaped_replace_text, -1, &error);

      /* Re-enable updating the search position label. The next-search-result action
       * below will cause it to update. */
      dzl_signal_group_unblock (self->buffer_signals);

      if (error != NULL)
        g_warning ("%s", error->message);

      dzl_gtk_widget_action (GTK_WIDGET (self), "editor-view", "move-next-search-result", NULL);
    }
}

static void
ide_editor_search_bar_actions_replace_all (GSimpleAction *action,
                                           GVariant      *state,
                                           gpointer       user_data)
{
  IdeEditorSearchBar *self = user_data;
  g_autofree gchar *unescaped_replace_text = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *replace_text;
  const gchar *search_text;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->settings == NULL || self->context == NULL)
    return;

  search_text = gtk_source_search_settings_get_search_text (self->settings);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  if (ide_str_empty0 (search_text) || replace_text == NULL)
    return;

  unescaped_replace_text = gtk_source_utils_unescape_search_text (replace_text);
  gtk_source_search_context_replace_all (self->context, unescaped_replace_text, -1, &error);

  if (error != NULL)
    g_warning ("%s", error->message);
}

static const GActionEntry search_bar_actions[] = {
  { "toggle-search-options", NULL, "b", "false",
    ide_editor_search_bar_actions_toggle_search_options },
  { "toggle-search-replace", NULL, "b", "false",
    ide_editor_search_bar_actions_toggle_search_replace },
  { "replace", ide_editor_search_bar_actions_replace },
  { "replace-all", ide_editor_search_bar_actions_replace_all },
};

void
_ide_editor_search_bar_init_actions (IdeEditorSearchBar *self)
{
  g_autoptr(GSimpleActionGroup) actions = NULL;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                   search_bar_actions,
                                   G_N_ELEMENTS (search_bar_actions),
                                   self);
  gtk_widget_insert_action_group (GTK_WIDGET (self), "search-bar", G_ACTION_GROUP (actions));
}
