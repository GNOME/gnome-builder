/* ide-editor-frame.c
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

#define G_LOG_DOMAIN "ide-editor-frame"

#include <glib/gi18n.h>

#include "ide-debug.h"

#include "application/ide-application.h"
#include "diagnostics/ide-source-location.h"
#include "editor/ide-editor-frame-actions.h"
#include "editor/ide-editor-frame-private.h"
#include "editor/ide-editor-frame.h"
#include "editor/ide-editor-map-bin.h"
#include "editor/ide-editor-perspective.h"
#include "history/ide-back-forward-list.h"
#include "util/ide-dnd.h"
#include "util/ide-gtk.h"
#include "workbench/ide-layout-stack.h"
#include "workbench/ide-workbench.h"

#define MINIMAP_HIDE_DURATION 1000
#define MINIMAP_SHOW_DURATION 250

G_DEFINE_TYPE (IdeEditorFrame, ide_editor_frame, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_AUTO_HIDE_MAP,
  PROP_BACK_FORWARD_LIST,
  PROP_DOCUMENT,
  PROP_SHOW_MAP,
  PROP_SHOW_RULER,
  LAST_PROP
};

enum {
  TARGET_URI_LIST = 100
};

static GParamSpec *properties [LAST_PROP];

static void
update_replace_actions_sensitivity (IdeEditorFrame *self)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextBuffer *buffer;
  GtkTextIter start;
  GtkTextIter end;
  const gchar *search_text;
  const gchar *replace_text;
  gint pos;
  gint count;
  gboolean enable_replace;
  gboolean enable_replace_all;
  gboolean replace_regex_valid;
  g_autoptr(GError) regex_error = NULL;
  g_autoptr(GError) replace_regex_error = NULL;
  GActionGroup *group;
  GAction *replace_action;
  GAction *replace_all_action;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  search_context = ide_source_view_get_search_context (self->source_view);
  search_settings = gtk_source_search_context_get_settings (search_context);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
  gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
  replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

  /* Gather enough info to determine if Replace or Replace All would make sense */
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));
  pos = gtk_source_search_context_get_occurrence_position (search_context, &start, &end);
  count = gtk_source_search_context_get_occurrences_count (search_context);
  regex_error = gtk_source_search_context_get_regex_error (search_context);
  replace_regex_valid = gtk_source_search_settings_get_regex_enabled (search_settings) ?
                        g_regex_check_replacement (replace_text, NULL, &replace_regex_error) :
                        TRUE;

  enable_replace = (!ide_str_empty0 (search_text) &&
                    regex_error == NULL &&
                    replace_regex_valid &&
                    pos > 0);

  enable_replace_all = (!ide_str_empty0 (search_text) &&
                        regex_error == NULL &&
                        replace_regex_valid &&
                        count > 0);

  group = gtk_widget_get_action_group (GTK_WIDGET (self->search_frame), "search-entry");
  replace_action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace");
  replace_all_action = g_action_map_lookup_action (G_ACTION_MAP (group), "replace-all");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (replace_action), enable_replace);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (replace_all_action), enable_replace_all);
}

static void
ide_editor_frame_update_ruler (IdeEditorFrame *self)
{
  const gchar *mode_display_name;
  const gchar *mode_name;
  GtkTextBuffer *buffer;
  gboolean visible = FALSE;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  if (!IDE_IS_BUFFER (buffer))
    return;

  /* update line/column text */
  if (self->show_ruler)
    {
      g_autofree gchar *text = NULL;
      guint ln = 0;
      guint col = 0;

      ide_source_view_get_visual_position (self->source_view, &ln, &col);
      text = g_strdup_printf (_("Line %u, Column %u"), ln + 1, col + 1);
      nautilus_floating_bar_set_primary_label (self->floating_bar, text);

      visible = TRUE;
    }
  else
    {
      nautilus_floating_bar_set_primary_label (self->floating_bar, NULL);
    }

  /* update current mode */
  mode_display_name = ide_source_view_get_mode_display_name (self->source_view);
  gtk_label_set_label (self->mode_name_label, mode_display_name);
  gtk_widget_set_visible (GTK_WIDGET (self->mode_name_label), !!mode_display_name);
  if (mode_display_name != NULL)
    visible = TRUE;

  /*
   * Update overwrite label.
   *
   * XXX: Hack for 3.18 to ensure we don't have "OVR Replace" in vim mode.
   */
  mode_name = ide_source_view_get_mode_name (self->source_view);
  if (ide_source_view_get_overwrite (self->source_view) &&
      !ide_str_equal0 (mode_name, "vim-replace"))
    {
      gtk_widget_set_visible (GTK_WIDGET (self->overwrite_label), TRUE);
      visible = TRUE;
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->overwrite_label), FALSE);
    }

  if (gtk_widget_get_visible (GTK_WIDGET (self->mode_name_label)))
    visible = TRUE;

  if (ide_buffer_get_busy (IDE_BUFFER (buffer)))
    {
      nautilus_floating_bar_set_show_spinner (self->floating_bar, TRUE);
      visible = TRUE;
    }
  else
    {
      nautilus_floating_bar_set_show_spinner (self->floating_bar, FALSE);
    }

  /* we don't fade while hiding because we likely won't have
   * any text labels set anyway.
   */
  if (!visible && gtk_widget_get_visible (GTK_WIDGET (self->floating_bar)))
    gtk_widget_hide (GTK_WIDGET (self->floating_bar));
  else if (visible && !gtk_widget_get_visible (GTK_WIDGET (self->floating_bar)))
    gtk_widget_show (GTK_WIDGET (self->floating_bar));
}

static void
ide_editor_frame_set_show_ruler (IdeEditorFrame *self,
                                 gboolean        show_ruler)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  if (show_ruler != self->show_ruler)
    {
      self->show_ruler = show_ruler;
      ide_editor_frame_update_ruler (self);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_RULER]);
    }
}

static void
ide_editor_frame_animate_map (IdeEditorFrame *self,
                              gboolean        visible)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_revealer_set_reveal_child (self->map_revealer, visible);
}

static void
ide_editor_frame_show_map (IdeEditorFrame *self,
                           IdeSourceMap   *source_map)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_MAP (source_map));

  ide_editor_frame_animate_map (self, TRUE);
}

static void
ide_editor_frame_hide_map (IdeEditorFrame *self,
                           IdeSourceMap   *source_map)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_MAP (source_map));

  /* ignore hide request if auto-hide is disabled */
  if ((self->source_map != NULL) && !self->auto_hide_map)
    return;

  ide_editor_frame_animate_map (self, FALSE);
}

static void
ide_editor_frame_set_position_label (IdeEditorFrame *self,
                                     const gchar    *text)
{
  g_return_if_fail (IDE_IS_EDITOR_FRAME (self));

  if (!text || !*text)
    {
      if (self->search_entry_tag)
        {
          gd_tagged_entry_remove_tag (self->search_entry, self->search_entry_tag);
          g_clear_object (&self->search_entry_tag);
        }
      return;
    }

  if (!self->search_entry_tag)
    {
      self->search_entry_tag = gd_tagged_entry_tag_new ("");
      gd_tagged_entry_add_tag (self->search_entry, self->search_entry_tag);
      gd_tagged_entry_tag_set_style (self->search_entry_tag,
                                     "gb-search-entry-occurrences-tag");
    }

  gd_tagged_entry_tag_set_label (self->search_entry_tag, text);
}

static void
ide_editor_frame_update_search_position_label (IdeEditorFrame *self)
{
  GtkSourceSearchContext *search_context;
  GtkStyleContext *context;
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  const gchar *search_text;
  gchar *text;
  gint count;
  gint pos;

  g_return_if_fail (IDE_IS_EDITOR_FRAME (self));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
  search_context = ide_source_view_get_search_context (self->source_view);
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  pos = gtk_source_search_context_get_occurrence_position (search_context, &begin, &end);
  count = gtk_source_search_context_get_occurrences_count (search_context);

  if ((pos == -1) || (count == -1))
    {
      /*
       * We are not yet done scanning the buffer.
       * We will be updated when we know more, so just hide it for now.
       */
      ide_editor_frame_set_position_label (self, NULL);
      return;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (self->search_entry));
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  /* We use our own error class because we don't want to colide with styling
   * from GTK+ themes.
   */
  if ((count == 0) && !ide_str_empty0 (search_text))
    gtk_style_context_add_class (context, "search-missing");
  else
    gtk_style_context_remove_class (context, "search-missing");

  text = g_strdup_printf (_("%u of %u"), pos, count);
  ide_editor_frame_set_position_label (self, text);
  g_free (text);
}

static void
ide_editor_frame_on_search_occurrences_notify (IdeEditorFrame          *self,
                                               GParamSpec              *pspec,
                                               GtkSourceSearchContext  *search_context)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  ide_editor_frame_update_search_position_label (self);

  update_replace_actions_sensitivity (self);
}

static void
on_cursor_moved (IdeBuffer         *buffer,
                 const GtkTextIter *location,
                 IdeEditorFrame    *self)
{
  GtkSourceSearchContext *search_context;
  gint count;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_BUFFER (buffer));

  search_context = ide_source_view_get_search_context (self->source_view);
  count = gtk_source_search_context_get_occurrences_count (search_context);

  /* This prevents flickering when the search is briefly invalidated */
  if (count != -1)
    {
      ide_editor_frame_update_ruler (self);
      ide_editor_frame_update_search_position_label (self);
      update_replace_actions_sensitivity (self);
    }
}

static void
on_regex_error_changed (IdeEditorFrame         *self,
                        GParamSpec             *pspec,
                        GtkSourceSearchContext *search_context)
{
  g_autoptr(GError) error = NULL;
  PangoAttrList *attrs;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  /*
   * If the regular expression is invalid, add a white squiggly underline;
   * otherwise remove it.
   */
  attrs = pango_attr_list_new ();
  error = gtk_source_search_context_get_regex_error (search_context);

  if (error != NULL)
    {
      pango_attr_list_insert (attrs, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
      pango_attr_list_insert (attrs, pango_attr_underline_color_new (65535, 65535, 65535));
    }

  gtk_entry_set_attributes (GTK_ENTRY (self->search_entry), attrs);
  pango_attr_list_unref (attrs);

  update_replace_actions_sensitivity (self);
}

/**
 * ide_editor_frame_get_source_view:
 *
 * Gets the #IdeEditorFrame:document property.
 *
 * Returns: (transfer none) (nullable): An #IdeSourceView or %NULL.
 */
IdeSourceView *
ide_editor_frame_get_source_view (IdeEditorFrame *self)
{
  g_return_val_if_fail (IDE_IS_EDITOR_FRAME (self), NULL);

  return self->source_view;
}

/**
 * ide_editor_frame_get_document:
 *
 * Gets the #IdeEditorFrame:document property.
 *
 * Returns: (transfer none) (nullable): An #IdeBuffer or %NULL.
 */
IdeBuffer *
ide_editor_frame_get_document (IdeEditorFrame *self)
{
  GtkTextBuffer *buffer;

  g_return_val_if_fail (IDE_IS_EDITOR_FRAME (self), NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  return IDE_IS_BUFFER (buffer) ? IDE_BUFFER (buffer) : NULL;
}

static gboolean
search_text_transform_to (GBinding     *binding,
                          const GValue *from_value,
                          GValue       *to_value,
                          gpointer      user_data)
{
  IdeEditorFrame *self = user_data;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  if (g_value_get_string (from_value) == NULL)
    {
      g_value_set_string (to_value, "");
    }
  else
    {
      const gchar *entry_text = g_value_get_string (from_value);
      GtkSourceSearchContext *search_context;
      GtkSourceSearchSettings *search_settings;

      search_context = ide_source_view_get_search_context (self->source_view);
      search_settings = gtk_source_search_context_get_settings (search_context);

      if (gtk_source_search_settings_get_regex_enabled (search_settings))
        {
          g_value_set_string (to_value, entry_text);
        }
      else
        {
          gchar *unescaped_entry_text;

          unescaped_entry_text = gtk_source_utils_unescape_search_text (entry_text);
          g_value_set_string (to_value, unescaped_entry_text);

          g_free (unescaped_entry_text);
        }
    }

  return TRUE;
}

static gboolean
search_text_transform_from (GBinding     *binding,
                            const GValue *from_value,
                            GValue       *to_value,
                            gpointer      user_data)
{
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  if (g_value_get_string (from_value) == NULL)
    g_value_set_string (to_value, "");
  else
    g_value_copy (from_value, to_value);

  return TRUE;
}

static void
ide_editor_frame_add_search_actions (IdeEditorFrame *self,
                                     GActionGroup   *group)
{
  GPropertyAction *prop_action;
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (G_IS_ACTION_GROUP (group));

  search_context = ide_source_view_get_search_context (self->source_view);
  search_settings = gtk_source_search_context_get_settings (search_context);

  prop_action = g_property_action_new ("change-case-sensitive", search_settings, "case-sensitive");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (prop_action));
  g_object_unref (prop_action);

  prop_action = g_property_action_new ("change-word-boundaries", search_settings, "at-word-boundaries");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (prop_action));
  g_object_unref (prop_action);

  prop_action = g_property_action_new ("change-regex-enabled", search_settings, "regex-enabled");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (prop_action));
  g_object_unref (prop_action);

  prop_action = g_property_action_new ("change-wrap-around", search_settings, "wrap-around");
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (prop_action));
  g_object_unref (prop_action);
}

static void
on_search_text_changed (IdeEditorFrame          *self,
                        GParamSpec              *pspec,
                        GtkSourceSearchSettings *search_settings)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  update_replace_actions_sensitivity (self);
}

static void
check_replace_text (IdeEditorFrame *self)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  g_autoptr(GError) error = NULL;
  PangoAttrList *attrs;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  search_context = ide_source_view_get_search_context (self->source_view);
  search_settings = gtk_source_search_context_get_settings (search_context);

  attrs = pango_attr_list_new ();

  /*
   * If the replace expression is invalid, add a white squiggly underline;
   * otherwise remove it.
   */
  if (gtk_source_search_settings_get_regex_enabled (search_settings))
    {
      const gchar *replace_text;

      replace_text = gtk_entry_get_text (GTK_ENTRY (self->replace_entry));

      if (!g_regex_check_replacement (replace_text, NULL, &error))
        {
          pango_attr_list_insert (attrs, pango_attr_underline_new (PANGO_UNDERLINE_ERROR));
          pango_attr_list_insert (attrs, pango_attr_underline_color_new (65535, 65535, 65535));
        }
    }

  gtk_entry_set_attributes (GTK_ENTRY (self->replace_entry), attrs);
  pango_attr_list_unref (attrs);
}

static void
on_regex_enabled_changed (IdeEditorFrame          *self,
                          GParamSpec              *pspec,
                          GtkSourceSearchSettings *search_settings)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_SETTINGS (search_settings));

  check_replace_text (self);
}

static void
on_replace_text_changed (IdeEditorFrame *self,
                         GParamSpec     *pspec,
                         GtkSearchEntry *replace_entry)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_SEARCH_ENTRY (replace_entry));

  check_replace_text (self);
  update_replace_actions_sensitivity (self);
}

static void
search_revealer_on_child_revealed_changed (IdeEditorFrame *self,
                                           GParamSpec     *pspec,
                                           GtkRevealer    *search_revealer)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_REVEALER (search_revealer));

  if (self->pending_replace_confirm == 0 ||
      !gtk_revealer_get_child_revealed (search_revealer))
    return;

  ide_widget_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);

  self->pending_replace_confirm--;

  gtk_widget_grab_focus (GTK_WIDGET (self->replace_button));
}

void
ide_editor_frame_set_document (IdeEditorFrame *self,
                               IdeBuffer      *buffer)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextMark *mark;
  GtkTextIter iter;
  GActionGroup *group;

  g_return_if_fail (IDE_IS_EDITOR_FRAME (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view), GTK_TEXT_BUFFER (buffer));

  g_signal_connect_object (buffer,
                           "notify::busy",
                           G_CALLBACK (ide_editor_frame_update_ruler),
                           self,
                           G_CONNECT_SWAPPED);

  self->cursor_moved_handler =
    g_signal_connect (buffer,
                      "cursor-moved",
                      G_CALLBACK (on_cursor_moved),
                      self);
  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, mark);
  on_cursor_moved (buffer, &iter, self);

  /*
   * Sync search entry with the search settings.
   */
  search_context = ide_source_view_get_search_context (self->source_view);
  search_settings = gtk_source_search_context_get_settings (search_context);
  g_object_bind_property_full (self->search_entry, "text", search_settings, "search-text",
                               (G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL),
                               search_text_transform_to, search_text_transform_from,
                               self, NULL);
  g_signal_connect_object (search_context,
                           "notify::occurrences-count",
                           G_CALLBACK (ide_editor_frame_on_search_occurrences_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (search_context,
                           "notify::regex-error",
                           G_CALLBACK (on_regex_error_changed),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * Add search option property actions
   */
  group = gtk_widget_get_action_group (GTK_WIDGET (self->search_frame), "search-entry");
  ide_editor_frame_add_search_actions (self, group);

  g_signal_connect_object (search_settings,
                           "notify::search-text",
                           G_CALLBACK (on_search_text_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (search_settings,
                           "notify::regex-enabled",
                           G_CALLBACK (on_regex_enabled_changed),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->replace_entry,
                           "notify::text",
                           G_CALLBACK (on_replace_text_changed),
                           self,
                           G_CONNECT_SWAPPED);

  /* Setup a callback so the replace-confirm action can work properly. */
  self->pending_replace_confirm = 0;
  g_signal_connect_object (self->search_revealer,
                           "notify::child-revealed",
                           G_CALLBACK (search_revealer_on_child_revealed_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static gboolean
get_smart_home_end (GValue   *value,
                    GVariant *variant,
                    gpointer  user_data)
{
  if (g_variant_get_boolean (variant))
    g_value_set_enum (value, GTK_SOURCE_SMART_HOME_END_BEFORE);
  else
    g_value_set_enum (value, GTK_SOURCE_SMART_HOME_END_DISABLED);

  return TRUE;
}

static gboolean
get_wrap_mode (GValue   *value,
               GVariant *variant,
               gpointer  user_data)
{
  if (g_variant_get_boolean (variant))
    g_value_set_enum (value, GTK_WRAP_WORD);
  else
    g_value_set_enum (value, GTK_WRAP_NONE);

  return TRUE;
}

static void
keybindings_changed (GSettings     *settings,
                     const gchar   *key,
                     IdeEditorFrame *self)
{
  g_signal_emit_by_name (self->source_view,
                         "set-mode",
                         NULL,
                         IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);
}

static void
ide_editor_frame_grab_focus (GtkWidget *widget)
{
  IdeEditorFrame *self = (IdeEditorFrame *)widget;

  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
ide_editor_frame__drag_data_received (IdeEditorFrame    *self,
                                      GdkDragContext    *context,
                                      gint               x,
                                      gint               y,
                                      GtkSelectionData  *selection_data,
                                      guint              info,
                                      guint              timestamp,
                                      GtkWidget         *widget)
{
  gchar **uri_list;

  g_return_if_fail (IDE_IS_SOURCE_VIEW (widget));

  switch (info)
    {
    case TARGET_URI_LIST:
      uri_list = ide_dnd_get_uri_list (selection_data);

      if (uri_list)
        {
          GVariantBuilder *builder;
          GVariant *variant;
          guint i;

          builder = g_variant_builder_new (G_VARIANT_TYPE_STRING_ARRAY);
          for (i = 0; uri_list [i]; i++)
            g_variant_builder_add (builder, "s", uri_list [i]);
          variant = g_variant_builder_end (builder);
          g_variant_builder_unref (builder);
          g_strfreev (uri_list);

          /*
           * request that we get focus first so the workbench will deliver the
           * document to us in the case it is not already open
           */
          gtk_widget_grab_focus (GTK_WIDGET (self));

          ide_widget_action (GTK_WIDGET (self), "workbench", "open-uri-list", variant);
        }

      gtk_drag_finish (context, TRUE, FALSE, timestamp);
      break;

    default:
      break;
    }
}

static gboolean
ide_editor_frame__search_key_press_event (IdeEditorFrame *self,
                                          GdkEventKey    *event,
                                          GdTaggedEntry  *entry)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GD_IS_TAGGED_ENTRY (entry));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      ide_widget_action (GTK_WIDGET (self->search_frame), "search-entry", "exit-search", NULL);
      return GDK_EVENT_STOP;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      /* stash the search string for later */
      g_free (self->previous_search_string);
      g_object_get (self->search_entry, "text", &self->previous_search_string, NULL);

      ide_widget_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
      gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
      return GDK_EVENT_STOP;

    case GDK_KEY_Down:
      ide_widget_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
      return GDK_EVENT_STOP;

    case GDK_KEY_Up:
      ide_widget_action (GTK_WIDGET (self), "frame", "previous-search-result", NULL);
      return GDK_EVENT_STOP;

    default:
      {
        if (!ide_source_view_get_rubberband_search (self->source_view))
          ide_source_view_set_rubberband_search (self->source_view, TRUE);
      }
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_editor_frame__replace_key_press_event (IdeEditorFrame *self,
                                           GdkEventKey    *event,
                                           GtkSearchEntry *entry)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_SEARCH_ENTRY (entry));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      ide_widget_action (GTK_WIDGET (self->search_frame), "search-entry", "exit-search", NULL);
      return GDK_EVENT_STOP;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      ide_widget_action (GTK_WIDGET (self->search_frame), "search-entry", "replace", NULL);
      return GDK_EVENT_STOP;

    case GDK_KEY_Down:
      ide_widget_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
      return GDK_EVENT_STOP;

    case GDK_KEY_Up:
      ide_widget_action (GTK_WIDGET (self), "frame", "previous-search-result", NULL);
      return GDK_EVENT_STOP;

    default:
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_editor_frame__source_view_focus_in_event (IdeEditorFrame *self,
                                              GdkEventKey    *event,
                                              IdeSourceView  *source_view)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));

  if (IDE_IS_BUFFER (buffer))
    ide_buffer_check_for_volume_change (IDE_BUFFER (buffer));

  return FALSE;
}

static void
ide_editor_frame__source_view_focus_location (IdeEditorFrame    *self,
                                              IdeSourceLocation *location,
                                              IdeSourceView     *source_view)
{
  IdeWorkbench *workbench;
  IdePerspective *editor;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self));
  editor = ide_workbench_get_perspective_by_name (workbench, "editor");

  ide_editor_perspective_focus_location (IDE_EDITOR_PERSPECTIVE (editor), location);
}

static gboolean
ide_editor_frame_get_show_map (IdeEditorFrame *self)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  return (self->source_map != NULL);
}

static void
ide_editor_frame_set_show_map (IdeEditorFrame *self,
                               gboolean        show_map)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  if (show_map != ide_editor_frame_get_show_map (self))
    {
      if (self->source_map != NULL)
        {
          gtk_container_remove (GTK_CONTAINER (self->source_map_container),
                                GTK_WIDGET (self->source_map));
          self->source_map = NULL;
        }
      else
        {
          self->source_map = g_object_new (IDE_TYPE_SOURCE_MAP,
                                           "view", self->source_view,
                                           "visible", TRUE,
                                           NULL);
          g_signal_connect_object (self->source_map,
                                   "show-map",
                                   G_CALLBACK (ide_editor_frame_show_map),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (self->source_map,
                                   "hide-map",
                                   G_CALLBACK (ide_editor_frame_hide_map),
                                   self,
                                   G_CONNECT_SWAPPED);
          gtk_container_add (GTK_CONTAINER (self->source_map_container),
                             GTK_WIDGET (self->source_map));
          g_signal_emit_by_name (self->source_map, "show-map");
        }

      /* Hide scrollbars when map is enabled. */
      g_object_set (self->scrolled_window,
                    "vscrollbar-policy", show_map ? GTK_POLICY_EXTERNAL : GTK_POLICY_AUTOMATIC,
                    NULL);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_MAP]);
    }
}

static void
ide_editor_frame_set_auto_hide_map (IdeEditorFrame *self,
                                    gboolean        auto_hide_map)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  auto_hide_map = !!auto_hide_map;

  if (auto_hide_map != self->auto_hide_map)
    {
      self->auto_hide_map = auto_hide_map;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_AUTO_HIDE_MAP]);
    }
}

static void
ide_editor_frame__search_populate_popup (IdeEditorFrame *self,
                                         GtkWidget      *popup,
                                         GdTaggedEntry  *entry)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_WIDGET (popup));
  g_assert (GD_IS_TAGGED_ENTRY (entry));

  if (GTK_IS_MENU_SHELL (popup))
    {
      GMenu *menu;
      GActionGroup *group;
      GAction *action;
      GtkEntryBuffer *buffer;
      GtkClipboard *clipboard;
      gboolean clipboard_contains_text;
      gboolean entry_has_selection;

      group = gtk_widget_get_action_group (GTK_WIDGET (self->search_frame), "search-entry");

      menu = ide_application_get_menu_by_id (IDE_APPLICATION_DEFAULT, "ide-editor-frame-search-menu");
      gtk_menu_shell_bind_model (GTK_MENU_SHELL (popup), G_MENU_MODEL (menu), NULL, TRUE);

      clipboard = gtk_widget_get_clipboard (GTK_WIDGET (entry), GDK_SELECTION_CLIPBOARD);
      clipboard_contains_text = gtk_clipboard_wait_is_text_available (clipboard);

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "paste-clipboard");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), clipboard_contains_text);

      entry_has_selection = gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), NULL, NULL);

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "cut-clipboard");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), entry_has_selection);

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "copy-clipboard");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), entry_has_selection);

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "delete-selection");
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), entry_has_selection);

      action = g_action_map_lookup_action (G_ACTION_MAP (group), "select-all");
      buffer = gtk_entry_get_buffer (GTK_ENTRY (self->search_entry));
      g_simple_action_set_enabled (G_SIMPLE_ACTION (action), gtk_entry_buffer_get_length (buffer) > 0);
    }
}

static void
ide_editor_frame_constructed (GObject *object)
{
  IdeEditorFrame *self = (IdeEditorFrame *)object;

  G_OBJECT_CLASS (ide_editor_frame_parent_class)->constructed (object);

  g_signal_connect_object (self->source_view,
                           "drag-data-received",
                           G_CALLBACK (ide_editor_frame__drag_data_received),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "focus-in-event",
                           G_CALLBACK (ide_editor_frame__source_view_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "focus-location",
                           G_CALLBACK (ide_editor_frame__source_view_focus_location),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "key-press-event",
                           G_CALLBACK (ide_editor_frame__search_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->replace_entry,
                           "key-press-event",
                           G_CALLBACK (ide_editor_frame__replace_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "populate-popup",
                           G_CALLBACK (ide_editor_frame__search_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
ide_editor_frame_destroy (GtkWidget *widget)
{
  IdeEditorFrame *self = (IdeEditorFrame *)widget;

  IDE_ENTRY;

  g_assert (IDE_IS_EDITOR_FRAME (self));

  if (self->source_view)
    {
      gtk_widget_destroy (GTK_WIDGET (self->source_view));
      self->source_view = NULL;
    }

  GTK_WIDGET_CLASS (ide_editor_frame_parent_class)->destroy (widget);

  IDE_EXIT;
}

static void
ide_editor_frame_dispose (GObject *object)
{
  IdeEditorFrame *self = (IdeEditorFrame *)object;

  g_clear_pointer (&self->previous_search_string, g_free);

  if (self->source_view && self->cursor_moved_handler)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
      ide_clear_signal_handler (buffer, &self->cursor_moved_handler);
    }

  g_clear_object (&self->search_entry_tag);

  G_OBJECT_CLASS (ide_editor_frame_parent_class)->dispose (object);
}

static void
ide_editor_frame_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeEditorFrame *self = IDE_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      g_value_set_boolean (value, self->auto_hide_map);
      break;

    case PROP_DOCUMENT:
      g_value_set_object (value, ide_editor_frame_get_document (self));
      break;

    case PROP_SHOW_MAP:
      g_value_set_boolean (value, ide_editor_frame_get_show_map (self));
      break;

    case PROP_SHOW_RULER:
      g_value_set_boolean (value, self->show_ruler);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_frame_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeEditorFrame *self = IDE_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      ide_editor_frame_set_auto_hide_map (self, g_value_get_boolean (value));
      break;

    case PROP_DOCUMENT:
      ide_editor_frame_set_document (self, g_value_get_object (value));
      break;

    case PROP_BACK_FORWARD_LIST:
      ide_source_view_set_back_forward_list (self->source_view, g_value_get_object (value));
      break;

    case PROP_SHOW_MAP:
      ide_editor_frame_set_show_map (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_RULER:
      ide_editor_frame_set_show_ruler (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_editor_frame_class_init (IdeEditorFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_editor_frame_constructed;
  object_class->dispose = ide_editor_frame_dispose;
  object_class->get_property = ide_editor_frame_get_property;
  object_class->set_property = ide_editor_frame_set_property;

  widget_class->destroy = ide_editor_frame_destroy;
  widget_class->grab_focus = ide_editor_frame_grab_focus;

  properties [PROP_AUTO_HIDE_MAP] =
    g_param_spec_boolean ("auto-hide-map",
                          "Auto Hide Map",
                          "Auto Hide Map",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_BACK_FORWARD_LIST] =
    g_param_spec_object ("back-forward-list",
                         "Back Forward List",
                         "The back forward list.",
                         IDE_TYPE_BACK_FORWARD_LIST,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         "Document",
                         "The editor document.",
                         IDE_TYPE_BUFFER,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_MAP] =
    g_param_spec_boolean ("show-map",
                          "Show Map",
                          "If the overview map should be shown.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SHOW_RULER] =
    g_param_spec_boolean ("show-ruler",
                          "Show Ruler",
                          "If the ruler should always be shown.",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-editor-frame.ui");

  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, floating_bar);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, map_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, mode_name_label);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, overwrite_label);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, scrolled_window);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, search_frame);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, search_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, replace_entry);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, replace_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, replace_all_button);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, search_options);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, search_revealer);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, source_map_container);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, source_overlay);
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, source_view);

  g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
}

static void
ide_editor_frame_init (IdeEditorFrame *self)
{
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GSettings) insight_settings = NULL;
  GtkTargetList *target_list;

  gtk_widget_init_template (GTK_WIDGET (self));

  ide_editor_frame_actions_init (self);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_bind (settings, "draw-spaces", self->source_view, "draw-spaces", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings, "font-name", self->source_view, "font-name", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "highlight-current-line", self->source_view, "highlight-current-line", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "overscroll", self->source_view, "overscroll", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "scroll-offset", self->source_view, "scroll-offset", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-grid-lines", self->source_view, "show-grid-lines", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-changes", self->source_view, "show-line-changes", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-numbers", self->source_view, "show-line-numbers", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "smart-backspace", self->source_view, "smart-backspace", G_SETTINGS_BIND_GET);
  g_settings_bind_with_mapping (settings, "smart-home-end", self->source_view, "smart-home-end", G_SETTINGS_BIND_GET, get_smart_home_end, NULL, NULL, NULL);
  g_settings_bind_with_mapping (settings, "wrap-text", self->source_view, "wrap-mode", G_SETTINGS_BIND_GET, get_wrap_mode, NULL, NULL, NULL);
  g_settings_bind (settings, "show-map", self, "show-map", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "auto-hide-map", self, "auto-hide-map", G_SETTINGS_BIND_GET);
  g_signal_connect_object (settings, "changed::keybindings", G_CALLBACK (keybindings_changed), self, 0);

  insight_settings = g_settings_new ("org.gnome.builder.code-insight");
  g_settings_bind (insight_settings, "word-completion", self->source_view, "enable-word-completion", G_SETTINGS_BIND_GET);

  g_signal_connect_object (self->source_view,
                           "notify::overwrite",
                           G_CALLBACK (ide_editor_frame_update_ruler),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "notify::mode-display-name",
                           G_CALLBACK (ide_editor_frame_update_ruler),
                           self,
                           G_CONNECT_SWAPPED);

  /*
   * we want to rubberbanding search until enter has been pressed or next/previous actions
   * have been activated.
   */
  g_object_bind_property (self->search_revealer, "visible",
                          self->source_view, "rubberband-search",
                          G_BINDING_SYNC_CREATE);

  /*
   * Drag and drop support
   */
  target_list = gtk_drag_dest_get_target_list (GTK_WIDGET (self->source_view));
  if (target_list)
    gtk_target_list_add_uri_targets (target_list, TARGET_URI_LIST);
}
