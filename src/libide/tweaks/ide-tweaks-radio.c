/* ide-tweaks-radio.c
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

#define G_LOG_DOMAIN "ide-tweaks-radio"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks-radio.h"

#include "gsettings-mapping.h"

struct _IdeTweaksRadio
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *subtitle;
  GVariant *value;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksRadio, ide_tweaks_radio, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static char **
add_to_set (const char * const *strv,
            const char         *value)
{
  g_autoptr(GStrvBuilder) builder = NULL;

  if (value == NULL || (strv != NULL && g_strv_contains (strv, value)))
    return g_strdupv ((char **)strv);

  builder = g_strv_builder_new ();
  g_strv_builder_addv (builder, (const char **)strv);
  g_strv_builder_add (builder, value);

  return g_strv_builder_end (builder);
}

static char **
remove_from_set (const char * const *strv,
                 const char         *value)
{
  g_autoptr(GStrvBuilder) builder = NULL;

  if (value == NULL || strv == NULL || !g_strv_contains (strv, value))
    return g_strdupv ((char **)strv);

  builder = g_strv_builder_new ();

  for (guint i = 0; strv[i]; i++)
    {
      if (!ide_str_equal0 (strv[i], value))
        g_strv_builder_add (builder, strv[i]);
    }

  return g_strv_builder_end (builder);
}

static void
ide_tweaks_radio_notify_active_cb (GtkCheckButton   *button,
                                   GParamSpec       *pspec,
                                   IdeTweaksBinding *binding)
{
  GVariant *value;
  GType type;

  g_assert (GTK_IS_CHECK_BUTTON (button));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  if (!ide_tweaks_binding_get_expected_type (binding, &type))
    return;

  value = g_object_get_data (G_OBJECT (button), "VALUE");

  if (gtk_check_button_get_active (button))
    {
      if (type == G_TYPE_STRV && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        {
          const char *str = g_variant_get_string (value, NULL);
          g_auto(GStrv) old_strv = ide_tweaks_binding_dup_strv (binding);
          g_auto(GStrv) new_strv = add_to_set ((const char * const *)old_strv, str);

          if (!old_strv ||
              !g_strv_equal ((const char * const *)old_strv,
                             (const char * const *)new_strv))
            ide_tweaks_binding_set_strv (binding, (const char * const *)new_strv);
        }
      else
        {
          ide_tweaks_binding_set_variant (binding, value);
        }
    }
  else
    {
      if (type == G_TYPE_STRV && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        {
          const char *str = g_variant_get_string (value, NULL);
          g_auto(GStrv) old_strv = ide_tweaks_binding_dup_strv (binding);
          g_auto(GStrv) new_strv = remove_from_set ((const char * const *)old_strv, str);

          if (old_strv &&
              !g_strv_equal ((const char * const *)old_strv,
                             (const char * const *)new_strv))
            ide_tweaks_binding_set_strv (binding, (const char * const *)new_strv);
        }
      else if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN) &&
               g_variant_get_boolean (value))
        {
          /* If boolean, unchecking might require we swap the state */
          ide_tweaks_binding_set_variant (binding, g_variant_new_boolean (FALSE));
        }
    }
}

static void
on_binding_changed_cb (GtkCheckButton   *button,
                       IdeTweaksBinding *binding)
{
  g_auto(GValue) value = G_VALUE_INIT;
  GVariant *variant;
  gboolean active = FALSE;
  GType type;

  g_assert (GTK_IS_CHECK_BUTTON (button));
  g_assert (IDE_IS_TWEAKS_BINDING (binding));

  if (!ide_tweaks_binding_get_expected_type (binding, &type))
    return;

  g_value_init (&value, type);
  if (!ide_tweaks_binding_get_value (binding, &value))
    return;

  if (!(variant = g_object_get_data (G_OBJECT (button), "VALUE")))
    return;


  if (type == G_TYPE_STRV && g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    {
      g_auto(GStrv) strv = ide_tweaks_binding_dup_strv (binding);

      if (strv != NULL)
        active = g_strv_contains ((const char * const *)strv,
                                  g_variant_get_string (variant, NULL));
    }
  else
    {
      const GVariantType *variant_type = g_variant_get_type (variant);
      g_autoptr(GVariant) to_compare = NULL;

      if ((to_compare = g_settings_set_mapping (&value, variant_type, NULL)))
        {
          g_variant_take_ref (to_compare);
          active = g_variant_equal (variant, to_compare);
        }
    }

  if (active != gtk_check_button_get_active (button))
    gtk_check_button_set_active (button, active);
}

static GtkWidget *
ide_tweaks_radio_create_for_item (IdeTweaksWidget *instance,
                                  IdeTweaksItem   *widget)
{
  IdeTweaksRadio *self = (IdeTweaksRadio *)widget;
  IdeTweaksBinding *binding;
  AdwActionRow *row;
  GtkWidget *radio;

  g_assert (IDE_IS_TWEAKS_RADIO (self));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (self))))
    return NULL;

  radio = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "can-target", FALSE,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  if (self->value)
    g_object_set_data_full (G_OBJECT (radio),
                            "VALUE",
                            g_variant_ref (self->value),
                            (GDestroyNotify)g_variant_unref);
  else
    g_object_set_data_full (G_OBJECT (radio),
                            "VALUE",
                            g_variant_ref_sink (g_variant_new_boolean (TRUE)),
                            (GDestroyNotify)g_variant_unref);
  g_signal_connect_object (radio,
                           "notify::active",
                           G_CALLBACK (ide_tweaks_radio_notify_active_cb),
                           binding,
                           0);
  gtk_widget_add_css_class (radio, "checkimage");

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "activatable-widget", radio,
                      NULL);
  adw_action_row_add_suffix (row, radio);

  g_signal_connect_object (binding,
                           "changed",
                           G_CALLBACK (on_binding_changed_cb),
                           radio,
                           G_CONNECT_SWAPPED);

  on_binding_changed_cb (GTK_CHECK_BUTTON (radio), binding);

  return GTK_WIDGET (row);
}

static void
ide_tweaks_radio_dispose (GObject *object)
{
  IdeTweaksRadio *self = (IdeTweaksRadio *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->value, g_variant_unref);

  G_OBJECT_CLASS (ide_tweaks_radio_parent_class)->dispose (object);
}

static void
ide_tweaks_radio_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeTweaksRadio *self = IDE_TWEAKS_RADIO (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_variant (value, ide_tweaks_radio_get_value (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_radio_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_radio_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_radio_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeTweaksRadio *self = IDE_TWEAKS_RADIO (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      ide_tweaks_radio_set_value (self, g_value_get_variant (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_radio_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_tweaks_radio_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_radio_class_init (IdeTweaksRadioClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_radio_dispose;
  object_class->get_property = ide_tweaks_radio_get_property;
  object_class->set_property = ide_tweaks_radio_set_property;

  widget_class->create_for_item = ide_tweaks_radio_create_for_item;

  properties[PROP_VALUE] =
    g_param_spec_variant ("value", NULL, NULL,
                          G_VARIANT_TYPE_ANY,
                          NULL,
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
ide_tweaks_radio_init (IdeTweaksRadio *self)
{
}

/**
 * ide_tweaks_radio_get_value:
 * @self: a #IdeTweaksRadio
 *
 * Returns: (transfer none) (nullable): a #GVariant or %NULL
 */
GVariant *
ide_tweaks_radio_get_value (IdeTweaksRadio *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_RADIO (self), NULL);

  return self->value;
}

const char *
ide_tweaks_radio_get_subtitle (IdeTweaksRadio *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_RADIO (self), NULL);

  return self->subtitle;
}

const char *
ide_tweaks_radio_get_title (IdeTweaksRadio *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_RADIO (self), NULL);

  return self->title;
}

void
ide_tweaks_radio_set_value (IdeTweaksRadio *self,
                            GVariant       *value)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (value == self->value)
    return;

  g_clear_pointer (&self->value, g_variant_unref);
  self->value = value ? g_variant_ref_sink (value) : NULL;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
}

void
ide_tweaks_radio_set_subtitle (IdeTweaksRadio *self,
                               const char     *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_tweaks_radio_set_title (IdeTweaksRadio *self,
                            const char     *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
