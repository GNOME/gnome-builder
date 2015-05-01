/* gb-editor-frame.c
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
#include <ide.h>

#include "gb-dnd.h"
#include "gb-editor-document.h"
#include "gb-editor-frame.h"
#include "gb-editor-frame-actions.h"
#include "gb-editor-frame-private.h"
#include "gb-editor-map-bin.h"
#include "gb-string.h"
#include "gb-view-stack.h"
#include "gb-widget.h"

#define MINIMAP_HIDE_DURATION 1000
#define MINIMAP_SHOW_DURATION 250

G_DEFINE_TYPE (GbEditorFrame, gb_editor_frame, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_AUTO_HIDE_MAP,
  PROP_BACK_FORWARD_LIST,
  PROP_DOCUMENT,
  PROP_SHOW_MAP,
  LAST_PROP
};

enum {
  TARGET_URI_LIST = 100
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
gb_editor_frame_animate_map (GbEditorFrame *self,
                             gboolean       visible)
{
  IdeAnimation *animation;
  GdkFrameClock *frame_clock;
  gdouble value;
  guint duration;

  g_assert (GB_IS_EDITOR_FRAME (self));

  if (self->map_animation)
    {
      animation = self->map_animation;
      ide_clear_weak_pointer (&self->map_animation);
      ide_animation_stop (animation);
    }

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (self->source_map_container));
  duration = visible ? MINIMAP_SHOW_DURATION : MINIMAP_HIDE_DURATION;
  value = visible ? 0.0 : 1.0;

  animation = ide_object_animate (self->overlay_adj,
                                  IDE_ANIMATION_EASE_IN_OUT_QUAD,
                                  duration,
                                  frame_clock,
                                  "value", value,
                                  NULL);
  ide_set_weak_pointer (&self->map_animation, animation);
}

static void
gb_editor_frame_show_map (GbEditorFrame *self,
                          IdeSourceMap  *source_map)
{
  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_MAP (source_map));

  gb_editor_frame_animate_map (self, TRUE);
}

static void
gb_editor_frame_hide_map (GbEditorFrame *self,
                          IdeSourceMap  *source_map)
{
  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_MAP (source_map));

  /* ignore hide request if auto-hide is disabled */
  if ((self->source_map != NULL) && !self->auto_hide_map)
    return;

  gb_editor_frame_animate_map (self, FALSE);
}

static void
gb_editor_frame_set_position_label (GbEditorFrame *self,
                                    const gchar   *text)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

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
gb_editor_frame_update_search_position_label (GbEditorFrame *self)
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

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));

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
      gb_editor_frame_set_position_label (self, NULL);
      return;
    }

  context = gtk_widget_get_style_context (GTK_WIDGET (self->search_entry));
  search_text = gtk_entry_get_text (GTK_ENTRY (self->search_entry));

  if ((count == 0) && !gb_str_empty0 (search_text))
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_ERROR);
  else
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_ERROR);

  text = g_strdup_printf (_("%u of %u"), pos, count);
  gb_editor_frame_set_position_label (self, text);
  g_free (text);
}

static void
gb_editor_frame_on_search_occurrences_notify (GbEditorFrame          *self,
                                              GParamSpec             *pspec,
                                              GtkSourceSearchContext *search_context)
{
  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));

  gb_editor_frame_update_search_position_label (self);
}

static void
on_cursor_moved (GbEditorDocument  *document,
                 const GtkTextIter *location,
                 GbEditorFrame     *self)
{
  g_autofree gchar *text = NULL;
  guint ln = 0;
  guint col = 0;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  ide_source_view_get_visual_position (self->source_view, &ln, &col);

  text = g_strdup_printf (_("Line %u, Column %u"), ln + 1, col + 1);
  nautilus_floating_bar_set_primary_label (self->floating_bar, text);

  gb_editor_frame_update_search_position_label (self);
}

/**
 * gb_editor_frame_get_document:
 *
 * Gets the #GbEditorFrame:document property.
 *
 * Returns: (transfer none) (nullable): A #GbEditorDocument or %NULL.
 */
GbEditorDocument *
gb_editor_frame_get_document (GbEditorFrame *self)
{
  GtkTextBuffer *buffer;

  g_return_val_if_fail (GB_IS_EDITOR_FRAME (self), NULL);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));

  if (GB_IS_EDITOR_DOCUMENT (buffer))
    return GB_EDITOR_DOCUMENT (buffer);

  return NULL;
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

static gboolean
string_to_boolean (GBinding     *binding,
                   const GValue *from_value,
                   GValue       *to_value,
                   gpointer      user_data)
{
  gboolean visible;

  visible = !gb_str_empty0 (g_value_get_string (from_value));
  g_value_set_boolean (to_value, visible);
  return TRUE;
}

void
gb_editor_frame_set_document (GbEditorFrame    *self,
                              GbEditorDocument *document)
{
  GtkSourceSearchContext *search_context;
  GtkSourceSearchSettings *search_settings;
  GtkTextMark *mark;
  GtkTextIter iter;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view), GTK_TEXT_BUFFER (document));
  self->cursor_moved_handler = g_signal_connect (document, "cursor-moved", G_CALLBACK (on_cursor_moved), self);
  g_object_bind_property (document, "busy", self->floating_bar, "show-spinner", G_BINDING_SYNC_CREATE);

  mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (document));
  gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (document), &iter, mark);
  on_cursor_moved (document, &iter, self);

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
                           G_CALLBACK (gb_editor_frame_on_search_occurrences_notify),
                           self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (self->source_view, "mode-display-name",
                          self->mode_name_label, "label",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (self->source_view, "mode-display-name",
                               self->mode_name_label, "visible",
                               G_BINDING_SYNC_CREATE, string_to_boolean, NULL,
                               NULL, NULL);
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
                     GbEditorFrame *self)
{
  g_signal_emit_by_name (self->source_view,
                         "set-mode",
                         NULL,
                         IDE_SOURCE_VIEW_MODE_TYPE_PERMANENT);
}

static void
gb_editor_frame_grab_focus (GtkWidget *widget)
{
  GbEditorFrame *self = (GbEditorFrame *)widget;

  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
gb_editor_frame__drag_data_received (GbEditorFrame    *self,
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
      uri_list = gb_dnd_get_uri_list (selection_data);

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

          gb_widget_activate_action (GTK_WIDGET (self), "workbench", "open-uri-list", variant);
        }

      gtk_drag_finish (context, TRUE, FALSE, timestamp);
      break;

    default:
      GTK_WIDGET_CLASS (gb_editor_frame_parent_class)->
        drag_data_received (widget, context, x, y, selection_data, info, timestamp);
      break;
    }
}

static gboolean
gb_editor_frame__search_key_press_event (GbEditorFrame *self,
                                         GdkEventKey   *event,
                                         GdTaggedEntry *entry)
{
  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (GD_IS_TAGGED_ENTRY (entry));

  switch (event->keyval)
    {
    case GDK_KEY_Escape:
      ide_source_view_rollback_search (self->source_view);
      ide_source_view_clear_search (self->source_view);
      ide_source_view_set_rubberband_search (self->source_view, FALSE);
      gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
      return TRUE;

    case GDK_KEY_KP_Enter:
    case GDK_KEY_Return:
      if ((event->state & GDK_SHIFT_MASK) == 0)
        gb_widget_activate_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
      else
        gb_widget_activate_action (GTK_WIDGET (self), "frame", "previous-search-result", NULL);
      ide_source_view_set_rubberband_search (self->source_view, FALSE);
      gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
      return TRUE;

    case GDK_KEY_Down:
      gb_widget_activate_action (GTK_WIDGET (self), "frame", "next-search-result", NULL);
      return TRUE;

    case GDK_KEY_Up:
      gb_widget_activate_action (GTK_WIDGET (self), "frame", "previous-search-result", NULL);
      return TRUE;

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

  return FALSE;
}

static gboolean
gb_editor_frame__source_view_focus_in_event (GbEditorFrame *self,
                                             GdkEventKey   *event,
                                             IdeSourceView *source_view)
{
  GtkTextBuffer *buffer;

  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  gtk_revealer_set_reveal_child (self->search_revealer, FALSE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));

  if (IDE_IS_BUFFER (buffer))
    ide_buffer_check_for_volume_change (IDE_BUFFER (buffer));

  return FALSE;
}

static void
gb_editor_frame__source_view_focus_location (GbEditorFrame     *self,
                                             IdeSourceLocation *location,
                                             IdeSourceView     *source_view)
{
  GtkWidget *widget = (GtkWidget *)self;

  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (location != NULL);
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  for (; widget && !GB_IS_VIEW_STACK (widget); widget = gtk_widget_get_parent (widget)) { }
  if (widget && GB_IS_VIEW_STACK (widget))
    gb_view_stack_focus_location (GB_VIEW_STACK (widget), location);
}

static void
gb_editor_frame__source_view_request_documentation (GbEditorFrame *self,
                                                    IdeSourceView *source_view)
{
  GtkTextBuffer *buffer;
  GtkTextIter begin;
  GtkTextIter end;
  GVariant *param;
  g_autofree gchar *text = NULL;

  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (IDE_IS_SOURCE_VIEW (source_view));

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source_view));
  gtk_text_buffer_get_selection_bounds (buffer, &begin, &end);
  text = gtk_text_iter_get_slice (&begin, &end);

  param = g_variant_new_string (text);
  gb_widget_activate_action (GTK_WIDGET (self), "workbench", "search-docs", param);
}

static gboolean
gb_editor_frame_get_show_map (GbEditorFrame *self)
{
  g_assert (GB_IS_EDITOR_FRAME (self));

  return (self->source_map != NULL);
}

static void
gb_editor_frame_set_show_map (GbEditorFrame *self,
                              gboolean       show_map)
{
  g_assert (GB_IS_EDITOR_FRAME (self));

  if (show_map != gb_editor_frame_get_show_map (self))
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
                                   G_CALLBACK (gb_editor_frame_show_map),
                                   self,
                                   G_CONNECT_SWAPPED);
          g_signal_connect_object (self->source_map,
                                   "hide-map",
                                   G_CALLBACK (gb_editor_frame_hide_map),
                                   self,
                                   G_CONNECT_SWAPPED);
          gtk_container_add (GTK_CONTAINER (self->source_map_container),
                             GTK_WIDGET (self->source_map));
        }

      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_SHOW_MAP]);
    }
}

static void
gb_editor_frame_set_auto_hide_map (GbEditorFrame *self,
                                   gboolean       auto_hide_map)
{
  g_assert (GB_IS_EDITOR_FRAME (self));

  auto_hide_map = !!auto_hide_map;

  if (auto_hide_map != self->auto_hide_map)
    {
      self->auto_hide_map = auto_hide_map;
      g_object_notify_by_pspec (G_OBJECT (self), gParamSpecs [PROP_AUTO_HIDE_MAP]);
    }
}

static void
gb_editor_frame__source_view_populate_popup (GbEditorFrame *self,
                                             GtkWidget     *popup,
                                             IdeSourceView *source_view)
{
  g_assert (GB_IS_EDITOR_FRAME (self));
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

static gboolean
gb_editor_frame__source_overlay_get_child_position (GbEditorFrame *self,
                                                   GtkWidget     *widget,
                                                   GtkAllocation *alloc,
                                                   GtkOverlay    *overlay)
{
  GtkAllocation main_alloc;
  GtkRequisition req;

  g_assert (GTK_IS_OVERLAY (overlay));
  g_assert (GB_IS_EDITOR_FRAME (self));
  g_assert (GTK_IS_WIDGET (widget));
  g_assert (alloc != NULL);

  if (widget == (GtkWidget *)self->source_map_container)
    {
      gdouble value;

      gtk_widget_get_allocation (GTK_WIDGET (self), &main_alloc);
      gtk_widget_get_preferred_size (widget, &req, NULL);

      alloc->x = main_alloc.x + main_alloc.width - req.width;
      alloc->width = req.width;
      alloc->y = main_alloc.y;
      alloc->height = main_alloc.height;

      /* adjust for animation */
      value = gtk_adjustment_get_value (self->overlay_adj);
      alloc->x += (value * alloc->width);

      return TRUE;
    }

  return FALSE;
}

static void
gb_editor_frame_constructed (GObject *object)
{
  GbEditorFrame *self = (GbEditorFrame *)object;

  G_OBJECT_CLASS (gb_editor_frame_parent_class)->constructed (object);

  g_signal_connect_object (self->source_view,
                           "drag-data-received",
                           G_CALLBACK (gb_editor_frame__drag_data_received),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "focus-in-event",
                           G_CALLBACK (gb_editor_frame__source_view_focus_in_event),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "focus-location",
                           G_CALLBACK (gb_editor_frame__source_view_focus_location),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "populate-popup",
                           G_CALLBACK (gb_editor_frame__source_view_populate_popup),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->source_view,
                           "request-documentation",
                           G_CALLBACK (gb_editor_frame__source_view_request_documentation),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->search_entry,
                           "key-press-event",
                           G_CALLBACK (gb_editor_frame__search_key_press_event),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_editor_frame_dispose (GObject *object)
{
  GbEditorFrame *self = (GbEditorFrame *)object;

  ide_clear_weak_pointer (&self->map_animation);

  if (self->source_view && self->cursor_moved_handler)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
      ide_clear_signal_handler (buffer, &self->cursor_moved_handler);
    }

  g_clear_object (&self->search_entry_tag);

  G_OBJECT_CLASS (gb_editor_frame_parent_class)->dispose (object);
}

static void
gb_editor_frame_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GbEditorFrame *self = GB_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      g_value_set_boolean (value, self->auto_hide_map);
      break;

    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_frame_get_document (self));
      break;

    case PROP_SHOW_MAP:
      g_value_set_boolean (value, gb_editor_frame_get_show_map (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_frame_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GbEditorFrame *self = GB_EDITOR_FRAME (object);

  switch (prop_id)
    {
    case PROP_AUTO_HIDE_MAP:
      gb_editor_frame_set_auto_hide_map (self, g_value_get_boolean (value));
      break;

    case PROP_DOCUMENT:
      gb_editor_frame_set_document (self, g_value_get_object (value));
      break;

    case PROP_BACK_FORWARD_LIST:
      ide_source_view_set_back_forward_list (self->source_view, g_value_get_object (value));
      break;

    case PROP_SHOW_MAP:
      gb_editor_frame_set_show_map (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_frame_class_init (GbEditorFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gb_editor_frame_constructed;
  object_class->dispose = gb_editor_frame_dispose;
  object_class->get_property = gb_editor_frame_get_property;
  object_class->set_property = gb_editor_frame_set_property;

  widget_class->grab_focus = gb_editor_frame_grab_focus;

  gParamSpecs [PROP_AUTO_HIDE_MAP] =
    g_param_spec_boolean ("auto-hide-map",
                          _("Auto Hide Map"),
                          _("Auto Hide Map"),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_AUTO_HIDE_MAP,
                                   gParamSpecs [PROP_AUTO_HIDE_MAP]);

  gParamSpecs [PROP_BACK_FORWARD_LIST] =
    g_param_spec_object ("back-forward-list",
                         _("Back Forward List"),
                         _("The back forward list."),
                         IDE_TYPE_BACK_FORWARD_LIST,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_BACK_FORWARD_LIST,
                                   gParamSpecs [PROP_BACK_FORWARD_LIST]);

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The editor document."),
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT, gParamSpecs [PROP_DOCUMENT]);

  gParamSpecs [PROP_SHOW_MAP] =
    g_param_spec_boolean ("show-map",
                          _("Show Map"),
                          _("If the overview map should be shown."),
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_SHOW_MAP,
                                   gParamSpecs [PROP_SHOW_MAP]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-frame.ui");

  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, floating_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, mode_name_label);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, overlay_adj);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, overwrite_label);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, scrolled_window);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_entry);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, source_map_container);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, source_overlay);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, source_view);

  g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
  g_type_ensure (GB_TYPE_EDITOR_MAP_BIN);
  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
}

static void
gb_editor_frame_init (GbEditorFrame *self)
{
  g_autoptr(GSettings) settings = NULL;
  GtkTargetList *target_list;

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_editor_frame_actions_init (self);

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
  g_settings_bind (settings, "word-completion", self->source_view, "enable-word-completion", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-map", self, "show-map", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "auto-hide-map", self, "auto-hide-map", G_SETTINGS_BIND_GET);
  g_signal_connect (settings, "changed::keybindings", G_CALLBACK (keybindings_changed), self);

  g_object_bind_property (self->source_view, "overwrite", self->overwrite_label, "visible", G_BINDING_SYNC_CREATE);

  g_signal_connect_object (self->source_overlay,
                           "get-child-position",
                           G_CALLBACK (gb_editor_frame__source_overlay_get_child_position),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->overlay_adj,
                           "value-changed",
                           G_CALLBACK (gtk_widget_queue_resize),
                           self->source_map_container,
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
