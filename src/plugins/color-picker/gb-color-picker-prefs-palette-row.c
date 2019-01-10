/* gb-color-picker-prefs-palette-row.c
 *
 * Copyright 2016 Sebastien Lafargue <slafargue@gnome.org>
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

#include <gdk/gdk.h>
#include "glib/gi18n.h"
#include <libide-editor.h>

#include "gstyle-rename-popover.h"

#include "gb-color-picker-prefs-palette-row.h"

struct _GbColorPickerPrefsPaletteRow
{
  DzlPreferencesBin  parent_instance;

  GtkLabel          *palette_name;
  GtkImage          *image;
  GtkWidget         *event_box;
  GtkWidget         *popover_menu;
  gchar             *palette_id;

  gulong             handler;

  gchar             *key;
  GVariant          *target;
  GSettings         *settings;

  guint              updating : 1;
  guint              is_editing : 1;
  guint              needs_attention : 1;
};

G_DEFINE_TYPE (GbColorPickerPrefsPaletteRow, gb_color_picker_prefs_palette_row, DZL_TYPE_PREFERENCES_BIN)

enum {
  PROP_0,
  PROP_KEY,
  PROP_NEEDS_ATTENTION,
  PROP_IS_EDITING,
  PROP_TARGET,
  PROP_PALETTE_NAME,
  N_PROPS
};

enum {
  ACTIVATED,
  CLOSED,
  EDIT,
  NAME_CHANGED,
  LAST_SIGNAL
};

static GParamSpec *properties [N_PROPS];
static guint signals [LAST_SIGNAL];

static void
gb_color_picker_prefs_palette_row_changed (GbColorPickerPrefsPaletteRow *self,
                                           const gchar                  *key,
                                           GSettings                    *settings)
{
  g_autoptr (GVariant) value = NULL;
  gboolean active;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  if (self->target == NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->image), FALSE);
      return;
    }

  if (self->updating == TRUE)
    return;

  value = g_settings_get_value (settings, key);
  if (g_variant_is_of_type (value, g_variant_get_type (self->target)))
    {
      active = (g_variant_equal (value, self->target));
      gtk_widget_set_visible (GTK_WIDGET (self->image), active);
    }
  else
    g_warning ("Value and target must be of the same type");
}

static void
gb_color_picker_prefs_palette_row_activate (GbColorPickerPrefsPaletteRow *self)
{
  g_autoptr (GVariant) value = NULL;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (self->target != NULL);

  if (!gtk_widget_get_sensitive (GTK_WIDGET (self)) || self->settings == NULL || self->updating)
    return;

  value = g_settings_get_value (self->settings, self->key);
  if (g_variant_is_of_type (value, g_variant_get_type (self->target)))
    {
      if (!g_variant_equal (value, self->target))
        {
          self->updating = TRUE;
          g_settings_set_value (self->settings, self->key, self->target);
          gtk_widget_set_visible (GTK_WIDGET (self->image), TRUE);
          self->updating = FALSE;
        }
    }
  else
    g_warning ("Value and target must be of the same type");
}

static void
gb_color_picker_prefs_palette_row_set_edit (GbColorPickerPrefsPaletteRow *self,
                                            gboolean                      is_editing)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));

  if (is_editing && !self->is_editing)
    g_signal_emit_by_name (self, "edit");

  self->is_editing = is_editing;
}

static void
contextual_popover_closed_cb (GbColorPickerPrefsPaletteRow *self,
                              GtkWidget                    *popover)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (GTK_IS_WIDGET (popover));

  gtk_widget_destroy (popover);

  gb_color_picker_prefs_palette_row_set_edit (self, FALSE);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_EDITING]);
}

static void
rename_popover_entry_renamed_cb (GbColorPickerPrefsPaletteRow *self,
                                 const gchar                  *name)
{
  const gchar *id;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));

  gtk_label_set_text (self->palette_name, name);
  id = g_variant_get_string (self->target, NULL);
  g_signal_emit_by_name (self, "name-changed",
                         id,
                         gtk_label_get_text (self->palette_name));
}

static void
gb_color_picker_prefs_palette_row_edit (GbColorPickerPrefsPaletteRow *self)
{
  GtkWidget *popover;
  const gchar *name;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));

  name = gtk_label_get_text (self->palette_name);
  popover = g_object_new (GSTYLE_TYPE_RENAME_POPOVER,
                          "label", _("Palette name"),
                          "name", name,
                          "message", _("Enter a new name for the palette"),
                          NULL);

  gtk_popover_set_relative_to (GTK_POPOVER (popover), GTK_WIDGET (self));
  g_signal_connect_swapped (popover, "closed", G_CALLBACK (contextual_popover_closed_cb), self);
  g_signal_connect_swapped (popover, "renamed", G_CALLBACK (rename_popover_entry_renamed_cb), self);
  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
gb_color_picker_prefs_palette_row_connect (DzlPreferencesBin *bin,
                                           GSettings         *settings)
{
  GbColorPickerPrefsPaletteRow *self = (GbColorPickerPrefsPaletteRow *)bin;
  g_autofree gchar *signal_detail = NULL;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (G_IS_SETTINGS (settings));

  signal_detail = g_strdup_printf ("changed::%s", self->key);
  self->settings = g_object_ref (settings);
  self->handler =
    g_signal_connect_object (settings,
                             signal_detail,
                             G_CALLBACK (gb_color_picker_prefs_palette_row_changed),
                             self,
                             G_CONNECT_SWAPPED);

  gb_color_picker_prefs_palette_row_changed (self, self->key, settings);
}

static void
gb_color_picker_prefs_palette_row_disconnect (DzlPreferencesBin *bin,
                                              GSettings         *settings)
{
  GbColorPickerPrefsPaletteRow *self = (GbColorPickerPrefsPaletteRow *)bin;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (G_IS_SETTINGS (settings));

  g_signal_handler_disconnect (settings, self->handler);
  self->handler = 0;
  g_clear_object (&self->settings);
}

static void
popover_button_rename_clicked_cb (GbColorPickerPrefsPaletteRow *self,
                                  GdkEvent                     *event,
                                  GtkButton                    *button)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (GTK_IS_BUTTON (button));

  self->is_editing = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_IS_EDITING]);
  gtk_popover_popdown (GTK_POPOVER (self->popover_menu));

  g_signal_emit_by_name (self, "edit");
}

static void
popover_button_remove_clicked_cb (GbColorPickerPrefsPaletteRow *self,
                                  GdkEvent                     *event,
                                  GtkButton                    *button)
{
  const gchar *id;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_BUTTON (button));

  id = g_variant_get_string (self->target, NULL);
  g_signal_emit_by_name (self, "closed", id);
  gtk_popover_popdown (GTK_POPOVER (self->popover_menu));
}

static gboolean
event_box_button_pressed_cb (GbColorPickerPrefsPaletteRow *self,
                             GdkEventButton               *event,
                             GtkEventBox                  *event_box)
{
  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));
  g_assert (event != NULL);
  g_assert (GTK_IS_EVENT_BOX (event_box));

  if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY)
    {
      gtk_popover_popup (GTK_POPOVER (self->popover_menu));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gb_color_picker_prefs_palette_row_set_palette_name (GbColorPickerPrefsPaletteRow *self,
                                                    const gchar                  *new_text)
{
  const gchar *text;

  g_assert (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));

  if (dzl_str_empty0 (new_text))
    {
      gtk_label_set_text (self->palette_name, "No name");
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PALETTE_NAME]);

      return;
    }

  text = gtk_label_get_text (self->palette_name);
  if (g_strcmp0 (text, new_text) != 0)
    {
      gtk_label_set_text (self->palette_name, new_text);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PALETTE_NAME]);
    }
}

void
gb_color_picker_prefs_palette_row_set_needs_attention (GbColorPickerPrefsPaletteRow *self,
                                                       gboolean                      needs_attention)
{
  GtkStyleContext *context;

  g_return_if_fail (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self));

  if (self->needs_attention != needs_attention)
    {
      context = gtk_widget_get_style_context (GTK_WIDGET (self));
      self->needs_attention = needs_attention;
      if (needs_attention)
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);
      else
       gtk_style_context_remove_class (context, GTK_STYLE_CLASS_NEEDS_ATTENTION);

      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_NEEDS_ATTENTION]);
    }
}

gboolean
gb_color_picker_prefs_palette_row_get_needs_attention (GbColorPickerPrefsPaletteRow *self)
{
  g_return_val_if_fail (GB_IS_COLOR_PICKER_PREFS_PALETTE_ROW (self), FALSE);

  return self->needs_attention;
}

GbColorPickerPrefsPaletteRow *
gb_color_picker_prefs_palette_row_new (void)
{
  return g_object_new (GB_TYPE_COLOR_PICKER_PREFS_PALETTE_ROW, NULL);
}

static void
gb_color_picker_prefs_palette_row_finalize (GObject *object)
{
  GbColorPickerPrefsPaletteRow *self = (GbColorPickerPrefsPaletteRow *)object;

  if (self->settings != NULL)
    gb_color_picker_prefs_palette_row_disconnect (DZL_PREFERENCES_BIN (self), self->settings);

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->target, g_variant_unref);
  g_clear_pointer (&self->palette_id, g_free);
  g_clear_object (&self->popover_menu);

  G_OBJECT_CLASS (gb_color_picker_prefs_palette_row_parent_class)->finalize (object);
}

static void
gb_color_picker_prefs_palette_row_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  GbColorPickerPrefsPaletteRow *self = GB_COLOR_PICKER_PREFS_PALETTE_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_NEEDS_ATTENTION:
      g_value_set_boolean (value, gb_color_picker_prefs_palette_row_get_needs_attention (self));
      break;

    case PROP_IS_EDITING:
      g_value_set_boolean (value, self->is_editing);
      break;

    case PROP_TARGET:
      g_value_set_variant (value, self->target);
      break;

    case PROP_PALETTE_NAME:
      g_value_set_string (value, gtk_label_get_text (self->palette_name));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_prefs_palette_row_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  GbColorPickerPrefsPaletteRow *self = GB_COLOR_PICKER_PREFS_PALETTE_ROW (object);

  switch (prop_id)
    {
    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_NEEDS_ATTENTION:
      gb_color_picker_prefs_palette_row_set_needs_attention (self, g_value_get_boolean (value));
      break;

    case PROP_IS_EDITING:
      gb_color_picker_prefs_palette_row_set_edit (self, g_value_get_boolean (value));
      break;

    case PROP_TARGET:
      self->target = g_value_dup_variant (value);
      break;

    case PROP_PALETTE_NAME:
      gb_color_picker_prefs_palette_row_set_palette_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_color_picker_prefs_palette_row_class_init (GbColorPickerPrefsPaletteRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  DzlPreferencesBinClass *bin_class = DZL_PREFERENCES_BIN_CLASS (klass);

  object_class->finalize = gb_color_picker_prefs_palette_row_finalize;
  object_class->get_property = gb_color_picker_prefs_palette_row_get_property;
  object_class->set_property = gb_color_picker_prefs_palette_row_set_property;

  bin_class->connect = gb_color_picker_prefs_palette_row_connect;
  bin_class->disconnect = gb_color_picker_prefs_palette_row_disconnect;

  properties [PROP_IS_EDITING] =
    g_param_spec_boolean ("is-editing",
                          "is-editing",
                          "Whether the row is currently in edit mode or not",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_TARGET] =
    g_param_spec_variant ("target",
                          "Target",
                          "Target",
                          G_VARIANT_TYPE_STRING,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "Key",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_NEEDS_ATTENTION] =
    g_param_spec_boolean ("needs-attention",
                          "Needs Attention",
                          "Whether this row needs attention",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties [PROP_PALETTE_NAME] =
    g_param_spec_string ("palette-name",
                         "Palette name",
                         "Palette name",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  signals [ACTIVATED] =
    g_signal_new_class_handler ("activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (gb_color_picker_prefs_palette_row_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);
  signals [CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);

  signals [NAME_CHANGED] =
    g_signal_new ("name-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_POINTER,
                  G_TYPE_POINTER);

  signals [EDIT] =
    g_signal_new_class_handler ("edit",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                G_CALLBACK (gb_color_picker_prefs_palette_row_edit),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  widget_class->activate_signal = signals [ACTIVATED];

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/color-picker/gtk/color-picker-palette-row.ui");
  gtk_widget_class_bind_template_child (widget_class, GbColorPickerPrefsPaletteRow, image);
  gtk_widget_class_bind_template_child (widget_class, GbColorPickerPrefsPaletteRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, GbColorPickerPrefsPaletteRow, palette_name);

  gtk_widget_class_set_css_name (widget_class, "colorpickerpaletterow");
}

static void
gb_color_picker_prefs_palette_row_init (GbColorPickerPrefsPaletteRow *self)
{
  GtkBuilder *builder;
  GtkWidget *button_rename;
  GtkWidget *button_remove;

  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_add_events (self->event_box, GDK_KEY_PRESS_MASK);

  g_signal_connect_swapped (self->event_box, "button-press-event",
                            G_CALLBACK (event_box_button_pressed_cb),
                            self);

  builder = gtk_builder_new_from_resource ("/plugins/color-picker/gtk/color-picker-palette-menu.ui");
  self->popover_menu = GTK_WIDGET (g_object_ref_sink (gtk_builder_get_object (builder, "popover")));
  button_rename = GTK_WIDGET (gtk_builder_get_object (builder, "button_rename"));
  g_signal_connect_object (button_rename, "button-release-event",
                           G_CALLBACK (popover_button_rename_clicked_cb), self, G_CONNECT_SWAPPED);

  button_remove = GTK_WIDGET (gtk_builder_get_object (builder, "button_remove"));
  g_signal_connect_object (button_remove, "button-release-event",
                           G_CALLBACK (popover_button_remove_clicked_cb), self, G_CONNECT_SWAPPED);

  gtk_popover_set_relative_to (GTK_POPOVER (self->popover_menu), GTK_WIDGET (self));

  g_object_unref (builder);
}
