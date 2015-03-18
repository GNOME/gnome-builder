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

#include "gb-editor-document.h"
#include "gb-editor-frame.h"
#include "gb-editor-frame-actions.h"
#include "gb-editor-frame-private.h"
#include "gb-widget.h"

G_DEFINE_TYPE (GbEditorFrame, gb_editor_frame, GTK_TYPE_BIN)

enum {
  PROP_0,
  PROP_DOCUMENT,
  LAST_PROP
};

static GParamSpec *gParamSpecs [LAST_PROP];

static void
on_cursor_moved (GbEditorDocument  *document,
                 const GtkTextIter *location,
                 GbEditorFrame     *self)
{
  GtkSourceView *source_view;
  gchar *text;
  guint ln;
  guint col;

  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  source_view = GTK_SOURCE_VIEW (self->source_view);

  ln = gtk_text_iter_get_line (location);
  col = gtk_source_view_get_visual_column (source_view, location);
  text = g_strdup_printf (_("Line %u, Column %u"), ln + 1, col + 1);
  nautilus_floating_bar_set_primary_label (self->floating_bar, text);
  g_free (text);

  //gb_editor_frame_update_search_position_label (self);
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

void
gb_editor_frame_set_document (GbEditorFrame    *self,
                              GbEditorDocument *document)
{
  g_return_if_fail (GB_IS_EDITOR_FRAME (self));
  g_return_if_fail (GB_IS_EDITOR_DOCUMENT (document));

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->source_view), GTK_TEXT_BUFFER (document));
  g_signal_connect (document, "cursor-moved", G_CALLBACK (on_cursor_moved), self);
  g_object_bind_property (document, "busy", self->floating_bar, "show-spinner", G_BINDING_SYNC_CREATE);
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
gb_editor_frame_finalize (GObject *object)
{
  G_OBJECT_CLASS (gb_editor_frame_parent_class)->finalize (object);
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
    case PROP_DOCUMENT:
      g_value_set_object (value, gb_editor_frame_get_document (self));
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
    case PROP_DOCUMENT:
      gb_editor_frame_set_document (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_editor_frame_class_init (GbEditorFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_editor_frame_finalize;
  object_class->get_property = gb_editor_frame_get_property;
  object_class->set_property = gb_editor_frame_set_property;

  gParamSpecs [PROP_DOCUMENT] =
    g_param_spec_object ("document",
                         _("Document"),
                         _("The editor document."),
                         GB_TYPE_EDITOR_DOCUMENT,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class, PROP_DOCUMENT, gParamSpecs [PROP_DOCUMENT]);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-editor-frame.ui");

  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, floating_bar);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, scrolled_window);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_entry);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, search_revealer);
  GB_WIDGET_CLASS_BIND (klass, GbEditorFrame, source_view);

  g_type_ensure (NAUTILUS_TYPE_FLOATING_BAR);
  g_type_ensure (GD_TYPE_TAGGED_ENTRY);
}

static void
gb_editor_frame_init (GbEditorFrame *self)
{
  g_autoptr(GSettings) settings = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  gb_editor_frame_actions_init (self);

  settings = g_settings_new ("org.gnome.builder.editor");
  g_settings_bind (settings, "font-name", self->source_view, "font-name", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "highlight-current-line", self->source_view, "highlight-current-line", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "scroll-offset", self->source_view, "scroll-offset", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-grid-lines", self->source_view, "show-grid-lines", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-changes", self->source_view, "show-line-changes", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "show-line-numbers", self->source_view, "show-line-numbers", G_SETTINGS_BIND_GET);
  g_settings_bind (settings, "smart-backspace", self->source_view, "smart-backspace", G_SETTINGS_BIND_GET);
  g_settings_bind_with_mapping (settings, "smart-home-end", self->source_view, "smart-home-end", G_SETTINGS_BIND_GET, get_smart_home_end, NULL, NULL, NULL);
  g_settings_bind (settings, "word-completion", self->source_view, "enable-word-completion", G_SETTINGS_BIND_GET);
  g_signal_connect (settings, "changed::keybindings", G_CALLBACK (keybindings_changed), self);
}
