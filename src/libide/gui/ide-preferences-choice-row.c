/* ide-preferences-choice-row.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-preferences-choice-row"

#include "config.h"

#include <libide-core.h>

#include "ide-preferences-choice-row.h"

struct _IdePreferencesChoiceRow
{
  AdwComboRow parent_instance;
  GSettings *settings;
  char *key;
};

enum {
  PROP_0,
  PROP_KEY,
  PROP_SETTINGS,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdePreferencesChoiceRow, ide_preferences_choice_row, ADW_TYPE_COMBO_ROW)

static GParamSpec *properties [N_PROPS];

static void
on_selected_changed_cb (IdePreferencesChoiceRow *self,
                        GParamSpec              *pspec)
{
  g_autoptr(GtkStringObject) strobj = NULL;
  GListModel *model;
  const char *value;
  guint selected;

  g_assert (IDE_IS_PREFERENCES_CHOICE_ROW (self));

  selected = adw_combo_row_get_selected (ADW_COMBO_ROW (self));
  model = adw_combo_row_get_model (ADW_COMBO_ROW (self));
  strobj = g_list_model_get_item (model, selected);
  value = gtk_string_object_get_string (strobj);

  g_settings_set_string (self->settings, self->key, value);
}

static void
on_settings_changed_cb (IdePreferencesChoiceRow *self,
                        const char              *key,
                        GSettings               *settings)
{
  g_autofree char *value = NULL;
  GListModel *model;
  guint n_items;

  g_assert (IDE_IS_PREFERENCES_CHOICE_ROW (self));
  g_assert (G_IS_SETTINGS (settings));

  model = adw_combo_row_get_model (ADW_COMBO_ROW (self));
  n_items = g_list_model_get_n_items (model);
  value = g_settings_get_string (settings, key);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GtkStringObject) strobj = g_list_model_get_item (model, i);
      const char *str = gtk_string_object_get_string (strobj);

      if (ide_str_equal0 (str, value))
        {
          adw_combo_row_set_selected (ADW_COMBO_ROW (self), i);
          break;
        }
    }
}

static void
ide_preferences_choice_row_constructed (GObject *object)
{
  IdePreferencesChoiceRow *self = (IdePreferencesChoiceRow *)object;
  g_autoptr(GSettingsSchemaKey) key = NULL;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GListStore) model = NULL;
  g_autoptr(GVariant) range = NULL;
  g_autoptr(GVariant) wrapper = NULL;
  g_autoptr(GVariant) choices = NULL;
  g_autofree char *current = NULL;
  g_autofree char *changed_key_name = NULL;
  GVariantIter iter;
  const char *choice = NULL;
  const char *type = NULL;

  G_OBJECT_CLASS (ide_preferences_choice_row_parent_class)->constructed (object);

  if (self->key == NULL || self->settings == NULL)
    g_return_if_reached ();

  g_object_get (self->settings,
                "settings-schema", &schema,
                NULL);

  if (!(key = g_settings_schema_get_key (schema, self->key)))
    g_return_if_reached ();

  current = g_settings_get_string (self->settings, self->key);
  range = g_settings_schema_key_get_range (key);
  g_variant_get (range, "(&s@v)", &type, &wrapper);

  if (!ide_str_equal0 (type, "enum"))
    {
      g_warning ("%s must be used with GSettings choice keys",
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  choices = g_variant_get_variant (wrapper);
  model = g_list_store_new (GTK_TYPE_STRING_OBJECT);

  g_variant_iter_init (&iter, choices);
  while (g_variant_iter_next (&iter, "&s", &choice))
    {
      g_autoptr(GtkStringObject) strobj = gtk_string_object_new (choice);
      g_list_store_append (model, strobj);
    }

  adw_combo_row_set_model (ADW_COMBO_ROW (self), G_LIST_MODEL (model));

  g_signal_connect (self,
                    "notify::selected",
                    G_CALLBACK (on_selected_changed_cb),
                    NULL);

  changed_key_name = g_strdup_printf ("changed::%s", self->key);
  g_signal_connect_object (self->settings,
                           changed_key_name,
                           G_CALLBACK (on_settings_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);
  on_settings_changed_cb (self, self->key, self->settings);
}

static void
ide_preferences_choice_row_dispose (GObject *object)
{
  IdePreferencesChoiceRow *self = (IdePreferencesChoiceRow *)object;

  g_clear_object (&self->settings);
  ide_clear_string (&self->key);

  G_OBJECT_CLASS (ide_preferences_choice_row_parent_class)->dispose (object);
}

static void
ide_preferences_choice_row_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  IdePreferencesChoiceRow *self = IDE_PREFERENCES_CHOICE_ROW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->settings);
      break;

    case PROP_KEY:
      g_value_set_string (value, self->key);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_choice_row_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  IdePreferencesChoiceRow *self = IDE_PREFERENCES_CHOICE_ROW (object);

  switch (prop_id)
    {
    case PROP_SETTINGS:
      self->settings = g_value_dup_object (value);
      break;

    case PROP_KEY:
      self->key = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_preferences_choice_row_class_init (IdePreferencesChoiceRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = ide_preferences_choice_row_constructed;
  object_class->dispose = ide_preferences_choice_row_dispose;
  object_class->get_property = ide_preferences_choice_row_get_property;
  object_class->set_property = ide_preferences_choice_row_set_property;

  properties [PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         G_TYPE_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_preferences_choice_row_init (IdePreferencesChoiceRow *self)
{
}
