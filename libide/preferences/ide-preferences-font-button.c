/* ide-preferences-font-button.c
 *
 * Copyright (C) 2015 Christian Hergert <chergert@redhat.com>
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

#include "ide-preferences-font-button.h"

struct _IdePreferencesFontButton
{
  GtkBin                parent_instance;

  GSettings            *settings;
  gchar                *schema_id;
  gchar                *key;

  GtkLabel             *title;
  GtkLabel             *font_family;
  GtkLabel             *font_size;
  GtkPopover           *popover;
  GtkButton            *confirm;
  GtkFontChooserWidget *chooser;
};

G_DEFINE_TYPE (IdePreferencesFontButton, ide_preferences_font_button, IDE_TYPE_PREFERENCES_CONTAINER)

enum {
  PROP_0,
  PROP_KEY,
  PROP_SCHEMA_ID,
  PROP_TITLE,
  LAST_PROP
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

static void
ide_preferences_font_button_show (IdePreferencesFontButton *self)
{
  gchar *font = NULL;

  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));

  font = g_settings_get_string (self->settings, self->key);
  g_object_set (self->chooser, "font", font, NULL);
  g_free (font);

  gtk_widget_show (GTK_WIDGET (self->popover));
}

static void
ide_preferences_font_button_activate (IdePreferencesFontButton *self)
{
  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));

  if (!gtk_widget_get_visible (GTK_WIDGET (self->popover)))
    ide_preferences_font_button_show (self);
}

static void
ide_preferences_font_button_changed (IdePreferencesFontButton *self,
                                     const gchar              *key,
                                     GSettings                *settings)
{
  PangoFontDescription *font_desc;
  gchar *name;

  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  name = g_settings_get_string (settings, key);
  font_desc = pango_font_description_from_string (name);

  if (font_desc != NULL)
    {
      gchar *font_size;

      gtk_label_set_label (self->font_family, pango_font_description_get_family (font_desc));
      font_size = g_strdup_printf ("%dpt", pango_font_description_get_size (font_desc) / PANGO_SCALE);
      gtk_label_set_label (self->font_size, font_size);
      g_free (font_size);
    }

  g_clear_pointer (&font_desc, pango_font_description_free);
  g_free (name);
}

static void
ide_preferences_font_button_constructed (GObject *object)
{
  IdePreferencesFontButton *self = (IdePreferencesFontButton *)object;
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autofree gchar *signal_detail = NULL;

  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, self->schema_id, TRUE);

  if (schema == NULL || !g_settings_schema_has_key (schema, self->key))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
      goto chainup;
    }

  self->settings = g_settings_new (self->schema_id);
  signal_detail = g_strdup_printf ("changed::%s", self->key);

  g_signal_connect_object (self->settings,
                           signal_detail,
                           G_CALLBACK (ide_preferences_font_button_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_preferences_font_button_changed (self, self->key, self->settings);

chainup:
  G_OBJECT_CLASS (ide_preferences_font_button_parent_class)->constructed (object);
}

static void
ide_preferences_font_button_finalize (GObject *object)
{
  IdePreferencesFontButton *self = (IdePreferencesFontButton *)object;

  g_clear_object (&self->settings);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->key, g_free);

  G_OBJECT_CLASS (ide_preferences_font_button_parent_class)->finalize (object);
}

static void
ide_preferences_font_button_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  IdePreferencesFontButton *self = IDE_PREFERENCES_FONT_BUTTON (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, self->schema_id);
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_font_button_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  IdePreferencesFontButton *self = IDE_PREFERENCES_FONT_BUTTON (object);

  switch (prop_id)
    {
    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_SCHEMA_ID:
      self->schema_id = g_value_dup_string (value);
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_font_button_class_init (IdePreferencesFontButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_preferences_font_button_constructed;
  object_class->finalize = ide_preferences_font_button_finalize;
  object_class->get_property = ide_preferences_font_button_get_property;
  object_class->set_property = ide_preferences_font_button_set_property;

  signals [ACTIVATE] =
    g_signal_new_class_handler ("activate",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_preferences_font_button_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  widget_class->activate_signal = signals [ACTIVATE];

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "Key",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         "Schema Id",
                         "Schema Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-font-button.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, chooser);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, confirm);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, font_family);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, font_size);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, popover);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesFontButton, title);
}

static gboolean
transform_to (GBinding     *binding,
              const GValue *value,
              GValue       *to_value,
              gpointer      user_data)
{
  g_value_set_boolean (to_value, !!g_value_get_boxed (value));
  return TRUE;
}

static void
ide_preferences_font_button_clicked (IdePreferencesFontButton *self,
                                     GtkButton                *button)
{
  g_autofree gchar *font = NULL;

  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));
  g_assert (GTK_IS_BUTTON (button));

  g_object_get (self->chooser, "font", &font, NULL);
  g_settings_set_string (self->settings, self->key, font);
  gtk_widget_hide (GTK_WIDGET (self->popover));
}

static void
ide_preferences_font_button_font_activated (IdePreferencesFontButton *self,
                                            const gchar              *font,
                                            GtkFontChooser           *chooser)
{
  g_assert (IDE_IS_PREFERENCES_FONT_BUTTON (self));
  g_assert (GTK_IS_FONT_CHOOSER (chooser));

  g_settings_set_string (self->settings, self->key, font);
  gtk_widget_hide (GTK_WIDGET (self->popover));
}

static void
ide_preferences_font_button_init (IdePreferencesFontButton *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property_full (self->chooser, "font-desc",
                               self->confirm, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               transform_to,
                               NULL, NULL, NULL);

  g_signal_connect_object (self->chooser,
                           "font-activated",
                           G_CALLBACK (ide_preferences_font_button_font_activated),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->confirm,
                           "clicked",
                           G_CALLBACK (ide_preferences_font_button_clicked),
                           self,
                           G_CONNECT_SWAPPED);
}
