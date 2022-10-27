/* ide-editor-search-bar.c
 *
 * Copyright 2020-2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-editor-search-bar"

#include "config.h"

#include <libide-gui.h>

#include "ide-editor-enums-private.h"
#include "ide-editor-page-private.h"
#include "ide-editor-search-bar-private.h"

G_DEFINE_FINAL_TYPE (IdeEditorSearchBar, ide_editor_search_bar, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_CAN_MOVE,
  PROP_CAN_REPLACE,
  PROP_CAN_REPLACE_ALL,
  PROP_CASE_SENSITIVE,
  PROP_MODE,
  PROP_USE_REGEX,
  PROP_WHOLE_WORDS,
  N_PROPS
};

enum {
  MOVE_NEXT_SEARCH,
  MOVE_PREVIOUS_SEARCH,
  N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
update_properties (IdeEditorSearchBar *self)
{
  gboolean can_move = _ide_editor_search_bar_get_can_move (self);
  gboolean can_replace = _ide_editor_search_bar_get_can_replace (self);
  gboolean can_replace_all = _ide_editor_search_bar_get_can_replace_all (self);
  int occurrence_position = -1;

  if (can_move != self->can_move)
    {
      self->can_move = can_move;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_MOVE]);
    }

  if (can_replace != self->can_replace)
    {
      self->can_replace = can_replace;
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.replace-one", self->can_replace);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_REPLACE]);
    }

  if (can_replace_all != self->can_replace_all)
    {
      self->can_replace_all = can_replace_all;
      gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.replace-all", self->can_replace_all);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_REPLACE_ALL]);
    }

  if (self->context != NULL)
    {
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (self->context));
      GtkTextIter begin, end;

      if (gtk_text_buffer_get_selection_bounds (buffer, &begin, &end))
        occurrence_position = gtk_source_search_context_get_occurrence_position (self->context, &begin, &end);
    }

  ide_search_entry_set_occurrence_position (self->search_entry, occurrence_position);
}

static void
ide_editor_search_bar_scroll_to_insert (IdeEditorSearchBar *self,
                                        GtkDirectionType    dir)
{
  GtkWidget *page;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if ((page = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_EDITOR_PAGE)))
    ide_editor_page_scroll_to_insert (IDE_EDITOR_PAGE (page), dir);
}

static void
ide_editor_search_bar_move_next_forward_cb (GObject      *object,
                                            GAsyncResult *result,
                                            gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorSearchBar) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped = FALSE;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));

  if (!gtk_source_search_context_forward_finish (context, result, &begin, &end, &has_wrapped, &error))
    {
      if (error != NULL)
        g_debug ("Search forward error: %s", error->message);
      return;
    }

  buffer = gtk_source_search_context_get_buffer (context);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &begin, &end);
  ide_editor_search_bar_scroll_to_insert (self, GTK_DIR_TAB_FORWARD);

  if (self->hide_after_move)
    gtk_widget_activate_action (GTK_WIDGET (self), "page.search.hide", NULL);
}

void
_ide_editor_search_bar_move_next (IdeEditorSearchBar *self,
                                  gboolean            hide_after_move)
{
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->context == NULL)
    return;

  self->hide_after_move = !!hide_after_move;
  self->jump_back_on_hide = FALSE;

  buffer = gtk_source_search_context_get_buffer (self->context);
  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_iter_order (&begin, &end);

  gtk_source_search_context_forward_async (self->context,
                                           &end,
                                           NULL,
                                           ide_editor_search_bar_move_next_forward_cb,
                                           g_object_ref (self));
}

static void
ide_editor_search_bar_move_previous_backward_cb (GObject      *object,
                                                 GAsyncResult *result,
                                                 gpointer      user_data)
{
  GtkSourceSearchContext *context = (GtkSourceSearchContext *)object;
  g_autoptr(IdeEditorSearchBar) self = user_data;
  g_autoptr(GError) error = NULL;
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  gboolean has_wrapped = FALSE;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));

  if (!gtk_source_search_context_backward_finish (context, result, &begin, &end, &has_wrapped, &error))
    {
      if (error != NULL)
        g_debug ("Search backward error: %s", error->message);
      return;
    }

  buffer = gtk_source_search_context_get_buffer (context);
  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &begin, &end);
  ide_editor_search_bar_scroll_to_insert (self, GTK_DIR_TAB_BACKWARD);

  if (self->hide_after_move)
    gtk_widget_activate_action (GTK_WIDGET (self), "page.search.hide", NULL);
}

void
_ide_editor_search_bar_move_previous (IdeEditorSearchBar *self,
                                      gboolean            hide_after_move)
{
  GtkSourceBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->context == NULL)
    return;

  self->hide_after_move = !!hide_after_move;
  self->jump_back_on_hide = FALSE;

  buffer = gtk_source_search_context_get_buffer (self->context);
  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
  gtk_text_iter_order (&begin, &end);

  gtk_source_search_context_backward_async (self->context,
                                            &begin,
                                            NULL,
                                            ide_editor_search_bar_move_previous_backward_cb,
                                            g_object_ref (self));
}

static void
search_replace_all (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *param)
{
  _ide_editor_search_bar_replace_all (IDE_EDITOR_SEARCH_BAR (widget));
}

static void
search_replace_one (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *param)
{
  _ide_editor_search_bar_replace (IDE_EDITOR_SEARCH_BAR (widget));
  _ide_editor_search_bar_move_next (IDE_EDITOR_SEARCH_BAR (widget), FALSE);
}

static void
move_next_action (GtkWidget  *widget,
                  const char *action_name,
                  GVariant   *param)
{
  _ide_editor_search_bar_move_next (IDE_EDITOR_SEARCH_BAR (widget),
                                    g_variant_get_boolean (param));
}

static void
move_previous_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *param)
{
  _ide_editor_search_bar_move_previous (IDE_EDITOR_SEARCH_BAR (widget),
                                        g_variant_get_boolean (param));
}

static gboolean
text_to_search_text (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  IdeEditorSearchBar *self = user_data;
  const gchar *str = g_value_get_string (from_value);

  if (!str || gtk_source_search_settings_get_regex_enabled (self->settings))
    g_value_set_string (to_value, str);
  else
    g_value_take_string (to_value, gtk_source_utils_unescape_search_text (str));

  return TRUE;
}

static gboolean
search_text_to_text (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  IdeEditorSearchBar *self = user_data;
  const gchar *str = g_value_get_string (from_value);

  if (str == NULL)
    str = "";

  if (gtk_source_search_settings_get_regex_enabled (self->settings))
    g_value_take_string (to_value, gtk_source_utils_escape_search_text (str));
  else
    g_value_set_string (to_value, str);

  return TRUE;
}

static gboolean
mode_to_boolean (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  if (g_value_get_enum (from_value) == IDE_EDITOR_SEARCH_BAR_MODE_REPLACE)
    g_value_set_boolean (to_value, TRUE);
  else
    g_value_set_boolean (to_value, FALSE);
  return TRUE;
}

static gboolean
boolean_to_mode (GBinding     *binding,
                 const GValue *from_value,
                 GValue       *to_value,
                 gpointer      user_data)
{
  if (g_value_get_boolean (from_value))
    g_value_set_enum (to_value, IDE_EDITOR_SEARCH_BAR_MODE_REPLACE);
  else
    g_value_set_enum (to_value, IDE_EDITOR_SEARCH_BAR_MODE_SEARCH);
  return TRUE;
}

void
_ide_editor_search_bar_grab_focus (IdeEditorSearchBar *self)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

static void
on_notify_replace_text_cb (IdeEditorSearchBar *self,
                           GParamSpec      *pspec,
                           GtkEntry        *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_IS_ENTRY (entry));

  update_properties (self);
}

static void
on_notify_search_text_cb (IdeEditorSearchBar   *self,
                          GParamSpec        *pspec,
                          IdeSearchEntry *entry)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (IDE_IS_SEARCH_ENTRY (entry));

  self->scroll_to_first_match = TRUE;
}

static gboolean
on_search_key_pressed_cb (GtkEventControllerKey *key,
                          guint                  keyval,
                          guint                  keycode,
                          GdkModifierType        state,
                          IdeEditorSearchBar    *self)
{
  g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  if ((state & (GDK_CONTROL_MASK | GDK_ALT_MASK)) == 0)
    {
      switch (keyval)
        {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
          _ide_editor_search_bar_move_previous (self, FALSE);
          return TRUE;

        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
          _ide_editor_search_bar_move_next (self, FALSE);
          return TRUE;

        default:
          break;
        }
    }

  return FALSE;
}

static void
on_settings_notify_cb (IdeEditorSearchBar      *self,
                       GParamSpec              *pspec,
                       GtkSourceSearchSettings *settings)
{
  if (g_strcmp0 (pspec->name, "at-word-boundaries") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WHOLE_WORDS]);
  else if (g_strcmp0 (pspec->name, "regex-enabled") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_USE_REGEX]);
  else if (g_strcmp0 (pspec->name, "case-sensitive") == 0)
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CASE_SENSITIVE]);
}

static void
ide_editor_search_bar_dispose (GObject *object)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)object;

  g_clear_pointer ((GtkWidget **)&self->grid, gtk_widget_unparent);

  G_OBJECT_CLASS (ide_editor_search_bar_parent_class)->dispose (object);
}

static void
ide_editor_search_bar_finalize (GObject *object)
{
  IdeEditorSearchBar *self = (IdeEditorSearchBar *)object;

  g_clear_object (&self->context);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_editor_search_bar_parent_class)->finalize (object);
}

static void
ide_editor_search_bar_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  IdeEditorSearchBar *self = IDE_EDITOR_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_MODE:
      g_value_set_enum (value,
                        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->replace_mode_button)) ?
                        IDE_EDITOR_SEARCH_BAR_MODE_REPLACE :
                        IDE_EDITOR_SEARCH_BAR_MODE_SEARCH);
      break;

    case PROP_CAN_MOVE:
      g_value_set_boolean (value, _ide_editor_search_bar_get_can_move (self));
      break;

    case PROP_CAN_REPLACE:
      g_value_set_boolean (value, _ide_editor_search_bar_get_can_replace (self));
      break;

    case PROP_CAN_REPLACE_ALL:
      g_value_set_boolean (value, _ide_editor_search_bar_get_can_replace_all (self));
      break;

    case PROP_CASE_SENSITIVE:
      g_value_set_boolean (value, gtk_source_search_settings_get_case_sensitive (self->settings));
      break;

    case PROP_WHOLE_WORDS:
      g_value_set_boolean (value, gtk_source_search_settings_get_at_word_boundaries (self->settings));
      break;

    case PROP_USE_REGEX:
      g_value_set_boolean (value, gtk_source_search_settings_get_regex_enabled (self->settings));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_bar_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  IdeEditorSearchBar *self = IDE_EDITOR_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_MODE:
      _ide_editor_search_bar_set_mode (self, g_value_get_enum (value));
      break;

    case PROP_CASE_SENSITIVE:
      gtk_source_search_settings_set_case_sensitive (self->settings, g_value_get_boolean (value));
      break;

    case PROP_WHOLE_WORDS:
      gtk_source_search_settings_set_at_word_boundaries (self->settings, g_value_get_boolean (value));
      break;

    case PROP_USE_REGEX:
      gtk_source_search_settings_set_regex_enabled (self->settings, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_search_bar_class_init (IdeEditorSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ide_editor_search_bar_dispose;
  object_class->finalize = ide_editor_search_bar_finalize;
  object_class->get_property = ide_editor_search_bar_get_property;
  object_class->set_property = ide_editor_search_bar_set_property;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "searchbar");
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/libide-editor/ide-editor-search-bar.ui");
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, grid);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, replace_mode_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorSearchBar, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_search_key_pressed_cb);

  signals [MOVE_NEXT_SEARCH] =
    g_signal_new_class_handler ("move-next-search",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (_ide_editor_search_bar_move_next),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  signals [MOVE_PREVIOUS_SEARCH] =
    g_signal_new_class_handler ("move-previous-search",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (_ide_editor_search_bar_move_previous),
                                NULL, NULL,
                                NULL,
                                G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  properties [PROP_MODE] =
    g_param_spec_enum ("mode",
                       "Mode",
                       "The mode for the search bar",
                       IDE_TYPE_EDITOR_SEARCH_BAR_MODE,
                       IDE_EDITOR_SEARCH_BAR_MODE_SEARCH,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_CAN_MOVE] =
    g_param_spec_boolean ("can-move",
                          "Can Move",
                          "If there are search results",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CAN_REPLACE] =
    g_param_spec_boolean ("can-replace",
                          "Can Replace",
                          "If search is ready to replace a single result",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CAN_REPLACE_ALL] =
    g_param_spec_boolean ("can-replace-all",
                          "Can Replace All",
                          "If search is ready to replace all results",
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_CASE_SENSITIVE] =
    g_param_spec_boolean ("case-sensitive", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_USE_REGEX] =
    g_param_spec_boolean ("use-regex", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_WHOLE_WORDS] =
    g_param_spec_boolean ("whole-words", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_install_property_action (widget_class, "search.case-sensitive", "case-sensitive");
  gtk_widget_class_install_property_action (widget_class, "search.whole-words", "whole-words");
  gtk_widget_class_install_property_action (widget_class, "search.use-regex", "use-regex");
  gtk_widget_class_install_action (widget_class, "search.move-next", "b", move_next_action);
  gtk_widget_class_install_action (widget_class, "search.move-previous", "b", move_previous_action);
  gtk_widget_class_install_action (widget_class, "search.replace-one", NULL, search_replace_one);
  gtk_widget_class_install_action (widget_class, "search.replace-all", NULL, search_replace_all);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "page.search.hide", NULL);

  g_type_ensure (IDE_TYPE_SEARCH_ENTRY);
}

static void
ide_editor_search_bar_init (IdeEditorSearchBar *self)
{
  self->settings = gtk_source_search_settings_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->replace_entry,
                           "notify::text",
                           G_CALLBACK (on_notify_replace_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "notify::text",
                           G_CALLBACK (on_notify_search_text_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->settings,
                           "notify",
                           G_CALLBACK (on_settings_notify_cb),
                           self,
                           G_CONNECT_SWAPPED);

  gtk_source_search_settings_set_wrap_around (self->settings, TRUE);

  g_object_bind_property_full (self->settings, "search-text",
                               self->search_entry, "text",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               search_text_to_text, text_to_search_text, self, NULL);
  g_object_bind_property_full (self->replace_mode_button, "active",
                               self, "mode",
                               G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
                               boolean_to_mode, mode_to_boolean, NULL, NULL);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.replace-one", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.replace-all", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.move-next", FALSE);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.move-previous", FALSE);
}

void
_ide_editor_search_bar_set_mode (IdeEditorSearchBar     *self,
                                 IdeEditorSearchBarMode  mode)
{
  gboolean is_replace;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  is_replace = mode == IDE_EDITOR_SEARCH_BAR_MODE_REPLACE;

  gtk_widget_set_visible (GTK_WIDGET (self->replace_entry), is_replace);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_button), is_replace);
  gtk_widget_set_visible (GTK_WIDGET (self->replace_all_button), is_replace);
  gtk_toggle_button_set_active (self->replace_mode_button, is_replace);

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODE]);
}

static void
scroll_to_first_match (IdeEditorSearchBar        *self,
                       GtkSourceSearchContext *context)
{
  GtkTextIter iter, match_begin, match_end;
  GtkTextBuffer *buffer;
  GtkWidget *page;
  gboolean wrapped;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));

  if (!(page = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_EDITOR_PAGE)))
    return;

  buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (context));
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, self->offset_when_shown);
  if (gtk_source_search_context_forward (context, &iter, &match_begin, &match_end, &wrapped))
    {
      GdkRectangle visible_rect;
      GtkTextIter last_line_iter;
      int search_result_line, last_visible_line;

      gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (IDE_EDITOR_PAGE (page)->view), &visible_rect);
      gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (IDE_EDITOR_PAGE (page)->view), &last_line_iter,
                                   visible_rect.y + visible_rect.height, NULL);

      search_result_line = gtk_text_iter_get_line (&match_begin);
      last_visible_line = gtk_text_iter_get_line (&last_line_iter);

      if (search_result_line > last_visible_line)
        {
          gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (IDE_EDITOR_PAGE (page)->view),
                                        &match_begin, 0.0, TRUE, 0.5, 0.15);
        }
      else
        {
          gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (IDE_EDITOR_PAGE (page)->view),
                                        &match_begin, 0.15, FALSE, .0, .0);
        }

      self->jump_back_on_hide = TRUE;
    }

  self->scroll_to_first_match = FALSE;
}

static void
ide_editor_search_bar_notify_occurrences_count_cb (IdeEditorSearchBar     *self,
                                                   GParamSpec             *pspec,
                                                   GtkSourceSearchContext *context)
{
  guint occurrence_count;

  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (context));

  occurrence_count = gtk_source_search_context_get_occurrences_count (context);
  ide_search_entry_set_occurrence_count (self->search_entry, occurrence_count);

  if (self->scroll_to_first_match && occurrence_count > 0)
    scroll_to_first_match (self, context);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.move-next", occurrence_count > 0);
  gtk_widget_action_set_enabled (GTK_WIDGET (self), "search.move-previous", occurrence_count > 0);

  update_properties (self);
}

static void
ide_editor_search_bar_cursor_moved_cb (IdeEditorSearchBar *self,
                                       IdeBuffer          *buffer)
{
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));
  g_assert (IDE_IS_BUFFER (buffer));

  update_properties (self);
}

void
_ide_editor_search_bar_attach (IdeEditorSearchBar *self,
                               IdeBuffer          *buffer)
{
  GtkTextIter begin, end, insert;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
                                    &insert,
                                    gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
  self->offset_when_shown = gtk_text_iter_get_offset (&insert);

  if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end))
    {
      g_autofree gchar *text = gtk_text_iter_get_slice (&begin, &end);
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), text);
    }

  if (self->context != NULL)
    return;

  self->context = gtk_source_search_context_new (GTK_SOURCE_BUFFER (buffer), self->settings);

  g_signal_connect_object (self->context,
                           "notify::occurrences-count",
                           G_CALLBACK (ide_editor_search_bar_notify_occurrences_count_cb),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (buffer,
                           "cursor-moved",
                           G_CALLBACK (ide_editor_search_bar_cursor_moved_cb),
                           self,
                           G_CONNECT_SWAPPED);
}

void
_ide_editor_search_bar_detach (IdeEditorSearchBar *self)
{
  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (self->context != NULL)
    {
      IdeBuffer *buffer = IDE_BUFFER (gtk_source_search_context_get_buffer (self->context));

      if (self->jump_back_on_hide)
        ide_editor_search_bar_scroll_to_insert (self, GTK_DIR_TAB_BACKWARD);

      g_signal_handlers_disconnect_by_func (self->context,
                                            G_CALLBACK (ide_editor_search_bar_notify_occurrences_count_cb),
                                            self);
      g_signal_handlers_disconnect_by_func (buffer,
                                            G_CALLBACK (ide_editor_search_bar_cursor_moved_cb),
                                            self);

      g_clear_object (&self->context);
    }

  self->hide_after_move = FALSE;
  self->jump_back_on_hide = FALSE;
}

gboolean
_ide_editor_search_bar_get_can_move (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), FALSE);

  return self->context != NULL &&
         gtk_source_search_context_get_occurrences_count (self->context) > 0;
}

gboolean
_ide_editor_search_bar_get_can_replace (IdeEditorSearchBar *self)
{
  GtkTextIter begin, end;
  GtkTextBuffer *buffer;

  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), FALSE);

  if (self->context == NULL)
    return FALSE;

  buffer = GTK_TEXT_BUFFER (gtk_source_search_context_get_buffer (self->context));

  return _ide_editor_search_bar_get_can_move (self) &&
         gtk_text_buffer_get_selection_bounds (buffer, &begin, &end) &&
         gtk_source_search_context_get_occurrence_position (self->context, &begin, &end) > 0;
}

gboolean
_ide_editor_search_bar_get_can_replace_all (IdeEditorSearchBar *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self), FALSE);

  return _ide_editor_search_bar_get_can_move (self);
}

void
_ide_editor_search_bar_replace (IdeEditorSearchBar *self)
{
  g_autoptr(GError) error = NULL;
  GtkSourceBuffer *buffer;
  GtkTextIter begin, end;
  const char *replace;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (!_ide_editor_search_bar_get_can_replace (self))
    return;

  buffer = gtk_source_search_context_get_buffer (self->context);
  replace = gtk_editable_get_text (GTK_EDITABLE (self->replace_entry));

  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);

  if (!gtk_source_search_context_replace (self->context, &begin, &end, replace, -1, &error))
    {
      g_warning ("Failed to replace match: %s", error->message);
      return;
    }

  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &end, &end);
  _ide_editor_search_bar_move_next (self, FALSE);
}

void
_ide_editor_search_bar_replace_all (IdeEditorSearchBar *self)
{
  g_autoptr(GError) error = NULL;
  g_autofree char *unescaped = NULL;
  const char *replace;

  g_return_if_fail (IDE_IS_EDITOR_SEARCH_BAR (self));

  if (!_ide_editor_search_bar_get_can_replace_all (self))
    return;

  replace = gtk_editable_get_text (GTK_EDITABLE (self->replace_entry));
  unescaped = gtk_source_utils_unescape_search_text (replace);

  if (!gtk_source_search_context_replace_all (self->context, unescaped, -1, &error))
    g_warning ("Failed to replace all matches: %s", error->message);
}
