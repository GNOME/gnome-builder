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

struct _IdeTweaksRadio
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *subtitle;
  char *action_name;
  GVariant *action_target;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksRadio, ide_tweaks_radio, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_radio_create_for_item (IdeTweaksWidget *instance,
                                  IdeTweaksItem   *widget)
{
  IdeTweaksRadio *self = (IdeTweaksRadio *)widget;
  AdwActionRow *row;
  GtkWidget *radio;

  g_assert (IDE_IS_TWEAKS_RADIO (self));

  radio = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "action-name", self->action_name,
                        "action-target", self->action_target,
                        "can-target", FALSE,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  gtk_widget_add_css_class (radio, "checkimage");

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "activatable-widget", radio,
                      NULL);
  adw_action_row_add_suffix (row, radio);

  return GTK_WIDGET (row);
}

static void
ide_tweaks_radio_dispose (GObject *object)
{
  IdeTweaksRadio *self = (IdeTweaksRadio *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->action_name, g_free);
  g_clear_pointer (&self->action_target, g_variant_unref);

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
    case PROP_ACTION_NAME:
      g_value_set_string (value, ide_tweaks_radio_get_action_name (self));
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, ide_tweaks_radio_get_action_target (self));
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
    case PROP_ACTION_NAME:
      ide_tweaks_radio_set_action_name (self, g_value_get_string (value));
      break;

    case PROP_ACTION_TARGET:
      ide_tweaks_radio_set_action_target (self, g_value_get_variant (value));
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

  properties[PROP_ACTION_NAME] =
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ACTION_TARGET] =
    g_param_spec_variant ("action-target", NULL, NULL,
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

const char *
ide_tweaks_radio_get_action_name (IdeTweaksRadio *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_RADIO (self), NULL);

  return self->action_name;
}

/**
 * ide_tweaks_radio_get_action_target:
 * @self: a #IdeTweaksRadio
 *
 * Returns: (transfer none) (nullable): a #GVariant or %NULL
 */
GVariant *
ide_tweaks_radio_get_action_target (IdeTweaksRadio *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_RADIO (self), NULL);

  return self->action_target;
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
ide_tweaks_radio_set_action_name (IdeTweaksRadio *self,
                                  const char     *action_name)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (ide_set_string (&self->action_name, action_name))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTION_NAME]);
}

void
ide_tweaks_radio_set_action_target (IdeTweaksRadio *self,
                                    GVariant       *action_target)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (action_target == self->action_target)
    return;

  g_clear_pointer (&self->action_target, g_variant_unref);
  self->action_target = action_target ? g_variant_ref_sink (action_target) : NULL;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ACTION_TARGET]);
}

void
ide_tweaks_radio_set_subtitle (IdeTweaksRadio *self,
                               const char     *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (ide_set_string (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_tweaks_radio_set_title (IdeTweaksRadio *self,
                            const char     *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_RADIO (self));

  if (ide_set_string (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
