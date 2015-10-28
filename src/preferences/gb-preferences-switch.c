/* gb-preferences-switch.c
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

#include "gb-preferences-switch.h"
#include "gb-widget.h"

struct _GbPreferencesSwitch
{
  GtkEventBox      parent_instance;

  GtkBox          *controls_box;
  GtkLabel        *description_label;
  GtkCheckButton  *settings_radio;
  GtkSwitch       *settings_switch;
  GtkLabel        *title_label;

  GSettings       *settings;
  gchar           *settings_schema_key;
  GVariant        *settings_schema_value;

  guint            in_widget : 1;
  guint            is_radio : 1;
};

G_DEFINE_TYPE (GbPreferencesSwitch, gb_preferences_switch, GTK_TYPE_EVENT_BOX)

enum {
  PROP_0,
  PROP_TITLE,
  PROP_DESCRIPTION,
  PROP_IS_RADIO,
  PROP_SETTINGS,
  PROP_SETTINGS_SCHEMA_KEY,
  PROP_SETTINGS_SCHEMA_VALUE,
  PROP_SIZE_GROUP,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

GtkWidget *
gb_preferences_switch_new (void)
{
  return g_object_new (GB_TYPE_PREFERENCES_SWITCH, NULL);
}

static void
gb_preferences_switch_update_settings (GbPreferencesSwitch *self)
{
  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  if ((self->settings != NULL) && (self->settings_schema_key != NULL))
    {
      GSimpleActionGroup *group;
      GAction *action;
      gchar *name;

      action = g_settings_create_action (self->settings, self->settings_schema_key);
      group = g_simple_action_group_new ();
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      gtk_widget_insert_action_group (GTK_WIDGET (self), "settings", G_ACTION_GROUP (group));
      g_object_unref (action);

      name = g_strdup_printf ("settings.%s", self->settings_schema_key);

      if (self->is_radio)
        {
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->settings_radio), name);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->settings_switch), NULL);
        }
      else
        {
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->settings_radio), NULL);
          gtk_actionable_set_action_name (GTK_ACTIONABLE (self->settings_switch), name);
        }

      g_free (name);
    }
}

static void
gb_preferences_switch_set_is_radio (GbPreferencesSwitch *self,
                                    gboolean             is_radio)
{
  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  self->is_radio = !!is_radio;

  gtk_widget_set_visible (GTK_WIDGET (self->settings_radio), is_radio);
  gtk_widget_set_visible (GTK_WIDGET (self->settings_switch), !is_radio);

  gb_preferences_switch_update_settings (self);
}

static void
gb_preferences_switch_set_settings_schema_value (GbPreferencesSwitch *self,
                                                 GVariant            *variant)
{
  g_return_if_fail (GB_IS_PREFERENCES_SWITCH (self));

  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->settings_switch), variant);
  gtk_actionable_set_action_target_value (GTK_ACTIONABLE (self->settings_radio), variant);
}

static void
gb_preferences_switch_set_settings (GbPreferencesSwitch *self,
                                    GSettings           *settings)
{
  g_return_if_fail (GB_IS_PREFERENCES_SWITCH (self));
  g_return_if_fail (G_IS_SETTINGS (settings));

  if (g_set_object (&self->settings, settings))
    gb_preferences_switch_update_settings (self);
}

static void
gb_preferences_switch_set_settings_schema_key (GbPreferencesSwitch *self,
                                               const gchar         *settings_schema_key)
{
  g_return_if_fail (GB_IS_PREFERENCES_SWITCH (self));

  if (self->settings_schema_key != settings_schema_key)
    {
      g_free (self->settings_schema_key);
      self->settings_schema_key = g_strdup (settings_schema_key);
      gb_preferences_switch_update_settings (self);
    }
}

static gboolean
gb_preferences_switch_draw (GtkWidget *widget,
                            cairo_t   *cr)
{

  GbPreferencesSwitch *self = (GbPreferencesSwitch *)widget;
  GtkStyleContext *style_context;
  GtkStateFlags flags;
  gboolean ret = FALSE;

  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  style_context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (style_context);

  if (self->in_widget)
    {
      flags = gtk_style_context_get_state (style_context);
      gtk_style_context_set_state (style_context, flags | GTK_STATE_FLAG_PRELIGHT);
    }

  ret = GTK_WIDGET_CLASS (gb_preferences_switch_parent_class)->draw (widget, cr);

  gtk_style_context_restore (style_context);

  return ret;
}

static gboolean
gb_preferences_switch_enter_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *crossing)
{
  GbPreferencesSwitch *self = (GbPreferencesSwitch *)widget;
  gboolean ret = FALSE;

  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  self->in_widget = TRUE;
  gtk_widget_queue_draw (widget);

  return ret;
}

static gboolean
gb_preferences_switch_leave_notify_event (GtkWidget        *widget,
                                          GdkEventCrossing *crossing)
{
  GbPreferencesSwitch *self = (GbPreferencesSwitch *)widget;
  gboolean ret = FALSE;

  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  self->in_widget = FALSE;
  gtk_widget_queue_draw (widget);

  return ret;
}

static gboolean
gb_preferences_switch_button_release_event (GtkWidget      *widget,
                                            GdkEventButton *button)
{
  GbPreferencesSwitch *self = (GbPreferencesSwitch *)widget;
  gboolean ret;

  g_assert (GB_IS_PREFERENCES_SWITCH (self));

  ret = GTK_WIDGET_CLASS (gb_preferences_switch_parent_class)->button_release_event (widget, button);

  if ((ret == FALSE) && (self->in_widget == TRUE))
    {
      if (button->button == GDK_BUTTON_PRIMARY)
        {
          if (self->is_radio)
            g_signal_emit_by_name (self->settings_radio, "activate");
          else
            g_signal_emit_by_name (self->settings_switch, "activate");

          ret = TRUE;
        }
    }

  return ret;
}

static void
gb_preferences_switch_set_size_group (GbPreferencesSwitch *self,
                                      GtkSizeGroup        *group)
{
  g_return_if_fail (GB_IS_PREFERENCES_SWITCH (self));
  g_return_if_fail (!group || GTK_IS_SIZE_GROUP (group));

  if (group != NULL)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->controls_box));
}

static void
gb_preferences_switch_finalize (GObject *object)
{
  GbPreferencesSwitch *self = (GbPreferencesSwitch *)object;

  g_clear_pointer (&self->settings_schema_key, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (gb_preferences_switch_parent_class)->finalize (object);
}

static void
gb_preferences_switch_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GbPreferencesSwitch *self = GB_PREFERENCES_SWITCH (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (self->title_label, g_value_get_string (value));
      break;

    case PROP_DESCRIPTION:
      gtk_label_set_label (self->description_label, g_value_get_string (value));
      break;

    case PROP_IS_RADIO:
      gb_preferences_switch_set_is_radio (self, g_value_get_boolean (value));
      break;

    case PROP_SETTINGS:
      gb_preferences_switch_set_settings (self, g_value_get_object (value));
      break;

    case PROP_SETTINGS_SCHEMA_KEY:
      gb_preferences_switch_set_settings_schema_key (self, g_value_get_string (value));
      break;

    case PROP_SETTINGS_SCHEMA_VALUE:
      gb_preferences_switch_set_settings_schema_value (self, g_value_get_variant (value));
      break;

    case PROP_SIZE_GROUP:
      gb_preferences_switch_set_size_group (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gb_preferences_switch_class_init (GbPreferencesSwitchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gb_preferences_switch_finalize;
  object_class->set_property = gb_preferences_switch_set_property;

  widget_class->button_release_event = gb_preferences_switch_button_release_event;
  widget_class->draw = gb_preferences_switch_draw;
  widget_class->enter_notify_event = gb_preferences_switch_enter_notify_event;
  widget_class->leave_notify_event = gb_preferences_switch_leave_notify_event;

  properties [PROP_TITLE] =
    g_param_spec_string ("title",
                         "Title",
                         "The title of the switch.",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "The description for the switch.",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_IS_RADIO] =
    g_param_spec_boolean ("is-radio",
                          "Is Radio",
                          "If a radio button should be used.",
                          FALSE,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         "Settings",
                         "The GSettings for the setting.",
                         G_TYPE_SETTINGS,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS_SCHEMA_KEY] =
    g_param_spec_string ("settings-schema-key",
                         "Settings Schema Key",
                         "The settings schema key.",
                         NULL,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS_SCHEMA_VALUE] =
    g_param_spec_variant ("settings-schema-value",
                          "Settings Schema Value",
                          "An action-target for the settings action.",
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  properties [PROP_SIZE_GROUP] =
    g_param_spec_object ("size-group",
                         "Size Group",
                         "The sizing group for the control.",
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  GB_WIDGET_CLASS_TEMPLATE (klass, "gb-preferences-switch.ui");

  GB_WIDGET_CLASS_BIND (klass, GbPreferencesSwitch, controls_box);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesSwitch, description_label);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesSwitch, settings_radio);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesSwitch, settings_switch);
  GB_WIDGET_CLASS_BIND (klass, GbPreferencesSwitch, title_label);
}

static void
gb_preferences_switch_init (GbPreferencesSwitch *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_add_events (GTK_WIDGET (self),
                         (GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_ENTER_NOTIFY_MASK |
                          GDK_LEAVE_NOTIFY_MASK));
}
