/* ide-preferences-switch.c
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

#include "ide-preferences-switch.h"

struct _IdePreferencesSwitch
{
  IdePreferencesContainer parent_instance;

  guint     is_radio : 1;

  gchar     *key;
  gchar     *schema_id;
  GSettings *settings;
  GVariant  *target;

  GtkLabel  *subtitle;
  GtkLabel  *title;
  GtkSwitch *widget;
  GtkImage  *image;
};

G_DEFINE_TYPE (IdePreferencesSwitch, ide_preferences_switch, IDE_TYPE_PREFERENCES_CONTAINER)

enum {
  PROP_0,
  PROP_IS_RADIO,
  PROP_SUBTITLE,
  PROP_TARGET,
  PROP_TITLE,
  PROP_SCHEMA_ID,
  PROP_KEY,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
ide_preferences_switch_changed (IdePreferencesSwitch *self,
                                const gchar          *key,
                                GSettings            *settings)
{
  GVariant *value;
  gboolean active = FALSE;

  g_assert (IDE_IS_PREFERENCES_SWITCH (self));
  g_assert (key != NULL);
  g_assert (G_IS_SETTINGS (settings));

  value = g_settings_get_value (settings, key);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
    active = g_variant_get_boolean (value);
  else if ((self->target != NULL) &&
           g_variant_is_of_type (value, g_variant_get_type (self->target)))
    active = g_variant_equal (value, self->target);
  else if ((self->target != NULL) &&
           g_variant_is_of_type (self->target, G_VARIANT_TYPE_STRING) &&
           g_variant_is_of_type (value, G_VARIANT_TYPE_STRING_ARRAY))
    {
      g_autofree const gchar **strv = g_variant_get_strv (value, NULL);
      const gchar *flag = g_variant_get_string (self->target, NULL);
      active = g_strv_contains (strv, flag);
    }

  if (self->is_radio)
    gtk_widget_set_visible (GTK_WIDGET (self->image), active);
  else
    gtk_switch_set_active (self->widget, active);
}

static void
ide_preferences_switch_constructed (GObject *object)
{
  IdePreferencesSwitch *self = (IdePreferencesSwitch *)object;
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autofree gchar *signal_detail = NULL;

  g_assert (IDE_IS_PREFERENCES_SWITCH (self));

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
                           G_CALLBACK (ide_preferences_switch_changed),
                           self,
                           G_CONNECT_SWAPPED);

  ide_preferences_switch_changed (self, self->key, self->settings);

chainup:
  G_OBJECT_CLASS (ide_preferences_switch_parent_class)->constructed (object);
}

static void
ide_preferences_switch_finalize (GObject *object)
{
  IdePreferencesSwitch *self = (IdePreferencesSwitch *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->target, g_variant_unref);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_preferences_switch_parent_class)->finalize (object);
}

static void
ide_preferences_switch_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdePreferencesSwitch *self = IDE_PREFERENCES_SWITCH (object);

  switch (prop_id)
    {
    case PROP_IS_RADIO:
      g_value_set_boolean (value, self->is_radio);
      break;

    case PROP_SCHEMA_ID:
      g_value_set_string (value, self->schema_id);
      break;

    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    case PROP_TARGET:
      g_value_set_variant (value, self->target);
      break;

    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->subtitle));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_switch_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdePreferencesSwitch *self = IDE_PREFERENCES_SWITCH (object);

  switch (prop_id)
    {
    case PROP_IS_RADIO:
      self->is_radio = g_value_get_boolean (value);
      gtk_widget_set_visible (GTK_WIDGET (self->widget), !self->is_radio);
      gtk_widget_set_visible (GTK_WIDGET (self->image), self->is_radio);
      break;

    case PROP_SCHEMA_ID:
      self->schema_id = g_value_dup_string (value);
      break;

    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    case PROP_TARGET:
      self->target = g_value_dup_variant (value);
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      g_object_set (self->subtitle,
                    "label", g_value_get_string (value),
                    "visible", !!g_value_get_string (value),
                    NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_switch_class_init (IdePreferencesSwitchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = ide_preferences_switch_constructed;
  object_class->finalize = ide_preferences_switch_finalize;
  object_class->get_property = ide_preferences_switch_get_property;
  object_class->set_property = ide_preferences_switch_set_property;

  properties [PROP_IS_RADIO] =
    g_param_spec_boolean ("is-radio",
                         "Is Radio",
                         "If a radio style should be used instead of a switch.",
                         FALSE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SCHEMA_ID] =
    g_param_spec_string ("schema-id",
                         "Schema Id",
                         "Schema Id",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TARGET] =
    g_param_spec_variant ("target",
                          "Target",
                          "Target",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_KEY] =
    g_param_spec_string ("key",
                         "Key",
                         "Key",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "Title",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "Subtitle",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/builder/ui/ide-preferences-switch.ui");
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesSwitch, image);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesSwitch, subtitle);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesSwitch, title);
  gtk_widget_class_bind_template_child (widget_class, IdePreferencesSwitch, widget);
}

static void
ide_preferences_switch_init (IdePreferencesSwitch *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
