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

#include "ide-macros.h"
#include "ide-preferences-switch.h"

struct _IdePreferencesSwitch
{
  IdePreferencesContainer parent_instance;

  guint     is_radio : 1;
  guint     updating : 1;

  gchar     *key;
  gchar     *schema_id;
  gchar     *path;
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
  PROP_KEY,
  PROP_PATH,
  PROP_SCHEMA_ID,
  PROP_SUBTITLE,
  PROP_TARGET,
  PROP_TITLE,
  LAST_PROP
};

enum {
  ACTIVATED,
  LAST_SIGNAL
};

static GParamSpec *properties [LAST_PROP];
static guint signals [LAST_SIGNAL];

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

  if (self->updating == TRUE)
    return;

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

  self->updating = TRUE;

  if (self->is_radio)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->image), active);
    }
  else
    {
      gtk_switch_set_active (self->widget, active);
      gtk_switch_set_state (self->widget, active);
    }

  self->updating = FALSE;

  g_variant_unref (value);
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

  if (self->path != NULL)
    self->settings = g_settings_new_with_path (self->schema_id, self->path);
  else
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
ide_preferences_switch_toggle (IdePreferencesSwitch *self,
                               gboolean              state)
{
  GVariant *value;

  g_assert (IDE_IS_PREFERENCES_SWITCH (self));

  if (self->updating)
    return;

  self->updating = TRUE;

  value = g_settings_get_value (self->settings, self->key);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
    {
      g_settings_set_boolean (self->settings, self->key, state);
    }
  else if ((self->target != NULL) &&
           g_variant_is_of_type (self->target, G_VARIANT_TYPE_STRING) &&
           g_variant_is_of_type (value, G_VARIANT_TYPE_STRING_ARRAY))
    {
      g_autofree const gchar **strv = g_variant_get_strv (value, NULL);
      g_autoptr(GPtrArray) ar = g_ptr_array_new ();
      const gchar *flag = g_variant_get_string (self->target, NULL);
      gboolean found = FALSE;
      gint i;

      for (i = 0; strv [i]; i++)
        {
          if (!state && ide_str_equal0 (strv [i], flag))
            continue;
          if (ide_str_equal0 (strv [i], flag))
            found = TRUE;
          g_ptr_array_add (ar, (gchar *)strv [i]);
        }

      if (state && !found)
        g_ptr_array_add (ar, (gchar *)flag);

      g_ptr_array_add (ar, NULL);

      g_settings_set_strv (self->settings, self->key, (const gchar * const *)ar->pdata);
    }
  else if ((self->target != NULL) &&
           g_variant_is_of_type (value, g_variant_get_type (self->target)))
    {
      g_settings_set_value (self->settings, self->key, self->target);
    }
  else
    {
      g_warning ("I don't know how to set a variant of type %s to %s",
                 (const gchar *)g_variant_get_type (value),
                 self->target ? (const gchar *)g_variant_get_type (self->target) : "(nil)");
    }


  g_variant_unref (value);

  if (self->is_radio)
    gtk_widget_set_visible (GTK_WIDGET (self->image), state);
  else
    gtk_switch_set_state (self->widget, state);

  self->updating = FALSE;
}

static gboolean
ide_preferences_switch_state_set (IdePreferencesSwitch *self,
                                  gboolean              state,
                                  GtkSwitch            *widget)
{
  g_assert (IDE_IS_PREFERENCES_SWITCH (self));
  g_assert (GTK_IS_SWITCH (widget));

  ide_preferences_switch_toggle (self, state);

  return TRUE;
}

static void
ide_preferences_switch_activate (IdePreferencesSwitch *self)
{
  gboolean state;

  g_assert (IDE_IS_PREFERENCES_SWITCH (self));

  if (!gtk_widget_get_sensitive (GTK_WIDGET (self)) || (self->settings == NULL))
    return;

  if (self->is_radio)
    state = !gtk_widget_get_visible (GTK_WIDGET (self->image));
  else
    state = !gtk_switch_get_active (self->widget);

  ide_preferences_switch_toggle (self, state);
}

static void
ide_preferences_switch_finalize (GObject *object)
{
  IdePreferencesSwitch *self = (IdePreferencesSwitch *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->schema_id, g_free);
  g_clear_pointer (&self->path, g_free);
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

    case PROP_PATH:
      g_value_set_string (value, self->path);
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

    case PROP_PATH:
      self->path = g_value_dup_string (value);
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

  signals [ACTIVATED] =
    g_signal_new_class_handler ("activated",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (ide_preferences_switch_activate),
                                NULL, NULL, NULL, G_TYPE_NONE, 0);

  widget_class->activate_signal = signals [ACTIVATED];

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

  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "Path",
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

  g_signal_connect_object (self->widget,
                           "state-set",
                           G_CALLBACK (ide_preferences_switch_state_set),
                           self,
                           G_CONNECT_SWAPPED);
}
