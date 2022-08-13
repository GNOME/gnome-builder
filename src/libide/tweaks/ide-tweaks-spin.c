/* ide-tweaks-spin.c
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

#define G_LOG_DOMAIN "ide-tweaks-spin"

#include "config.h"

#include <glib/gi18n.h>

#include <adwaita.h>

#include "ide-tweaks.h"
#include "ide-tweaks-spin.h"

struct _IdeTweaksSpin
{
  IdeTweaksWidget parent_instance;
  IdeTweaksSettings *settings;
  char *key;
  char *title;
  char *subtitle;
};

enum {
  PROP_0,
  PROP_KEY,
  PROP_SETTINGS,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksSpin, ide_tweaks_spin, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
set_double_property (gpointer    instance,
                     const char *property,
                     GVariant   *value)
{
  GValue val = { 0 };
  double v = 0;

  g_assert (instance != NULL);
  g_assert (property != NULL);
  g_assert (value != NULL);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_DOUBLE))
    v = g_variant_get_double (value);

  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT16))
    v = g_variant_get_int16 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT16))
    v = g_variant_get_uint16 (value);

  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT32))
    v = g_variant_get_int32 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT32))
    v = g_variant_get_uint32 (value);

  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    v = g_variant_get_int64 (value);
  else if (g_variant_is_of_type (value, G_VARIANT_TYPE_UINT64))
    v = g_variant_get_uint64 (value);

  else
    g_warning ("Unknown variant type: %s\n", (gchar *)g_variant_get_type (value));

  g_value_init (&val, G_TYPE_DOUBLE);
  g_value_set_double (&val, v);
  g_object_set_property (instance, property, &val);
  g_value_unset (&val);
}

static GtkAdjustment *
create_adjustment (const char *schema_id,
                   const char *key,
                   guint      *digits)
{
  GSettingsSchemaSource *source;
  GSettingsSchemaKey *schema_key = NULL;
  GSettingsSchema *schema = NULL;
  GtkAdjustment *ret = NULL;
  GVariant *range = NULL;
  GVariant *values = NULL;
  GVariant *lower = NULL;
  GVariant *upper = NULL;
  GVariantIter iter;
  char *type = NULL;

  g_assert (schema_id != NULL);

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
  schema_key = g_settings_schema_get_key (schema, key);
  range = g_settings_schema_key_get_range (schema_key);
  g_variant_get (range, "(sv)", &type, &values);

  if (!ide_str_equal0 (type, "range") ||
      (2 != g_variant_iter_init (&iter, values)))
    goto cleanup;

  lower = g_variant_iter_next_value (&iter);
  upper = g_variant_iter_next_value (&iter);

  ret = gtk_adjustment_new (0, 0, 0, 1, 10, 0);
  set_double_property (ret, "lower", lower);
  set_double_property (ret, "upper", upper);

  if (g_variant_is_of_type (lower, G_VARIANT_TYPE_DOUBLE) ||
      g_variant_is_of_type (upper, G_VARIANT_TYPE_DOUBLE))
    {
      gtk_adjustment_set_step_increment (ret, 0.1);
      *digits = 2;
    }

cleanup:
  g_clear_pointer (&schema, g_settings_schema_unref);
  g_clear_pointer (&schema_key, g_settings_schema_key_unref);
  g_clear_pointer (&range, g_variant_unref);
  g_clear_pointer (&lower, g_variant_unref);
  g_clear_pointer (&upper, g_variant_unref);
  g_clear_pointer (&values, g_variant_unref);
  g_clear_pointer (&type, g_free);

  return ret;
}

static GtkWidget *
ide_tweaks_spin_create (IdeTweaksWidget *widget)
{
  IdeTweaksSpin *self = (IdeTweaksSpin *)widget;
  GtkAdjustment *adjustment;
  GtkSpinButton *button;
  const char *schema_id;
  AdwActionRow *row;
  guint digits = 0;

  g_assert (IDE_IS_TWEAKS_SPIN (self));

  if (self->settings == NULL || self->key == NULL)
    return NULL;

  schema_id = ide_tweaks_settings_get_schema_id (self->settings);
  adjustment = create_adjustment (schema_id, self->key, &digits);
  button = g_object_new (GTK_TYPE_SPIN_BUTTON,
                         "adjustment", adjustment,
                         "digits", digits,
                         "valign", GTK_ALIGN_CENTER,
                         NULL);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "activatable-widget", button,
                      NULL);
  adw_action_row_add_suffix (row, GTK_WIDGET (button));

  ide_tweaks_settings_bind (self->settings, self->key,
                            adjustment, "value",
                            G_SETTINGS_BIND_DEFAULT);

  return GTK_WIDGET (row);
}

static void
ide_tweaks_spin_dispose (GObject *object)
{
  IdeTweaksSpin *self = (IdeTweaksSpin *)object;

  g_clear_pointer (&self->key, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (ide_tweaks_spin_parent_class)->dispose (object);
}

static void
ide_tweaks_spin_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeTweaksSpin *self = IDE_TWEAKS_SPIN (object);

  switch (prop_id)
    {
    case PROP_KEY:
      g_value_set_string (value, ide_tweaks_spin_get_key (self));
      break;

    case PROP_SETTINGS:
      g_value_set_object (value, ide_tweaks_spin_get_settings (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_spin_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_spin_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_spin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeTweaksSpin *self = IDE_TWEAKS_SPIN (object);

  switch (prop_id)
    {
    case PROP_KEY:
      ide_tweaks_spin_set_key (self, g_value_get_string (value));
      break;

    case PROP_SETTINGS:
      ide_tweaks_spin_set_settings (self, g_value_get_object (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_spin_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_tweaks_spin_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_spin_class_init (IdeTweaksSpinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_spin_dispose;
  object_class->get_property = ide_tweaks_spin_get_property;
  object_class->set_property = ide_tweaks_spin_set_property;

  widget_class->create = ide_tweaks_spin_create;

  properties[PROP_KEY] =
    g_param_spec_string ("key", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SETTINGS] =
    g_param_spec_object ("settings", NULL, NULL,
                         IDE_TYPE_TWEAKS_SETTINGS,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_spin_init (IdeTweaksSpin *self)
{
}

/**
 * ide_tweaks_spin_get_settings:
 * @self: a #IdeTweaksSpin
 *
 * Gets the settings containing #IdeTweaksSpin:key.
 *
 * Returns: (transfer none) (nullable): an #IdeTweaksSettings or %NULL
 */
IdeTweaksSettings *
ide_tweaks_spin_get_settings (IdeTweaksSpin *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SPIN (self), NULL);

  return self->settings;
}

const char *
ide_tweaks_spin_get_key (IdeTweaksSpin *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SPIN (self), NULL);

  return self->key;
}

const char *
ide_tweaks_spin_get_subtitle (IdeTweaksSpin *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SPIN (self), NULL);

  return self->subtitle;
}

const char *
ide_tweaks_spin_get_title (IdeTweaksSpin *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SPIN (self), NULL);

  return self->title;
}

void
ide_tweaks_spin_set_settings (IdeTweaksSpin     *self,
                              IdeTweaksSettings *settings)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (g_set_object (&self->settings, settings))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SETTINGS]);
}

void
ide_tweaks_spin_set_key (IdeTweaksSpin *self,
                           const char    *key)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (ide_set_string (&self->key, key))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KEY]);
}

void
ide_tweaks_spin_set_subtitle (IdeTweaksSpin *self,
                              const char    *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (ide_set_string (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_tweaks_spin_set_title (IdeTweaksSpin *self,
                           const char    *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (ide_set_string (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
