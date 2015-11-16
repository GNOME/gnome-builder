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

#include <glib/gi18n.h>

#include "ide-dnd.h"
#include "ide-editor-frame-actions.h"
#include "ide-editor-frame-private.h"
#include "ide-editor-frame.h"
#include "ide-editor-map-bin.h"
#include "ide-gtk.h"
#include "ide-layout-stack.h"

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
                                gboolean       show_ruler)
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
                             gboolean       visible)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  gtk_revealer_set_reveal_child (self->map_revealer, visible);
}

static void
ide_editor_frame_show_map (IdeEditorFrame *self,
                          IdeSourceMap  *source_map)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_MAP (source_map));

  ide_editor_frame_animate_map (self, TRUE);
}

static void
ide_editor_frame_hide_map (IdeEditorFrame *self,
                          IdeSourceMap  *source_map)
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
                                    const gchar   *text)
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
      gd_tagged_entry_tag_set_style (self->search_entry_tag, "gb-search-entry-occurrences-tag");
      gd_tagged_entry_add_tag (self->search_entry, self->search_entry_tag);
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

  if ((count == 0) && !ide_str_empty0 (search_text))
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_ERROR);

  text = g_strdup_printf (_("%u of %u"), pos, count);
  ide_editor_frame_set_position_label (self, text);
  g_free (text);
}

static void
ide_editor_frame_on_search_occurrences_notify (IdeEditorFrame          *self,
                                              GParamSpec             *pspec,
                                              GtkSourceSearchContext *search_context)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  ide_editor_frame_update_search_position_label (self);
}

static void
on_cursor_moved (IdeBuffer         *buffer,
                 const GtkTextIter *location,
                 IdeEditorFrame    *self)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_BUFFER (buffer));

  ide_editor_frame_update_ruler (self);
  ide_editor_frame_update_search_position_label (self);
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
  g_assert (from_value != NULL);
  g_assert (to_value != NULL);

  if (g_value_get_string (from_value) == NULL)
    g_value_set_string (to_value, "");
  else
    g_value_copy (from_value, to_value);

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

void
ide_editor_frame_set_document (IdeEditorFrame *self,
                               IdeBuffer      *buffer)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextMark *mark;
  GtkTextIter iter;

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
                               NULL, NULL);
  g_signal_connect_object (search_context,
                           "notify::occurrences-count",
                           G_CALLBACK (ide_editor_frame_on_search_occurrences_notify),
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
                                     GdkDragContext   *context,
                                     gint              x,
                                     gint              y,
                                     GtkSelectionData *selection_data,
                                     guint             info,
                                     guint             timestamp,
                                     GtkWidget        *widget)
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
                                         GdkEventKey   *event,
                                         GdTaggedEntry *entry)
{
  GtkTextBuffer *buffer;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GD_IS_TAGGED_ENTRY (entry));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      /* stash the search string for later */
      g_free (self->previous_search_string);
      g_object_get (self->search_entry, "text", &self->previous_search_string, NULL);

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

      return GDK_EVENT_STOP;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
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
        GtkSourceSearchSettings *search_settings;
        GtkSourceSearchContext *search_context;

        if (!ide_source_view_get_rubberband_search (self->source_view))
          ide_source_view_set_rubberband_search (self->source_view, TRUE);

        /*
         * Other modes, such as Vim emulation, want word boundaries, but we do
         * not when searching from this entry. Sort of hacky, but gets the job
         * done to just change that setting here.
         */
        search_context = ide_source_view_get_search_context (self->source_view);
        search_settings = gtk_source_search_context_get_settings (search_context);
        gtk_source_search_settings_set_at_word_boundaries (search_settings, FALSE);
      }
      break;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
ide_editor_frame__source_view_focus_in_event (IdeEditorFrame *self,
                                             GdkEventKey   *event,
                                             IdeSourceView *source_view)
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
  GtkWidget *toplevel;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  toplevel = gtk_widget_get_ancestor (GTK_WIDGET (self), IDE_TYPE_WORKBENCH);

  if (IDE_IS_WORKBENCH (toplevel))
    {
      /*
       * Convert location to Uri and open.
       */
#if 0
      ide_layout_stack_focus_location (IDE_LAYOUT_STACK (widget), location);
#endif
    }
}

static void
ide_editor_frame__source_view_request_documentation (IdeEditorFrame *self,
                                                     IdeSourceView  *source_view)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GVariant *param;
  g_autofree gchar *text = NULL;

  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  text = gtk_text_iter_get_slice (&begin, &end);

  param = g_variant_new_string (text);
  ide_widget_action (GTK_WIDGET (self), "workbench", "search-docs", param);
}

static gboolean
ide_editor_frame_get_show_map (IdeEditorFrame *self)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));

  return (self->source_map != NULL);
}

static void
ide_editor_frame_set_show_map (IdeEditorFrame *self,
                              gboolean       show_map)
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

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_MAP]);
    }
}

static void
ide_editor_frame_set_auto_hide_map (IdeEditorFrame *self,
                                   gboolean       auto_hide_map)
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
ide_editor_frame__source_view_populate_popup (IdeEditorFrame *self,
                                             GtkWidget     *popup,
                                             IdeSourceView *source_view)
{
  g_assert (IDE_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_WIDGET (popup));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  if (GTK_IS_MENU_SHELL (popup))
    {
      GtkWidget *item;
      GtkWidget *sep;

      sep = g_object_new (GTK_TYPE_SEPARATOR_MENU_ITEM,
                          "visible", TRUE,
                          NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup), sep);

      item = g_object_new (GTK_TYPE_MENU_ITEM,
                           "action-name", "view.reveal",
                           "label", _("Re_veal in Project Tree"),
                           "use-underline", TRUE,
                           "visible", TRUE,
                           NULL);
      gtk_menu_shell_append (GTK_MENU_SHELL (popup), item);
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

  g_signal_connect_object (self->source_view,
                           "populate-popup",
                           G_CALLBACK (ide_editor_frame__source_view_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "request-documentation",
                           G_CALLBACK (ide_editor_frame__source_view_request_documentation),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "key-press-event",
                           G_CALLBACK (ide_editor_frame__search_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);
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
  gtk_widget_class_bind_template_child (widget_class, IdeEditorFrame, search_entry);
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
  g_settings_bind (settings, "scroll-offset", self->source_view, "scroll-offset", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-grid-lines", self->source_view, "show-grid-lines", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-changes", self->source_view, "show-line-changes", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-numbers", self->source_view, "show-line-numbers", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "smart-backspace", self->source_view, "smart-backspace", G_SETTINGS_BIND_GET);
  g_settings_bind_with_mapping (settings, "smart-home-end", self->source_view, "smart-home-end", G_SETTINGS_BIND_GET, get_smart_home_end, NULL, NULL, NULL);
  g_settings_bind (settings, "show-map", self, "show-map", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "auto-hide-map", self, "auto-hide-map", G_SETTINGS_BIND_GET);
  g_signal_connect (settings, "changed::keybindings", G_CALLBACK (keybindings_changed), self);

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
