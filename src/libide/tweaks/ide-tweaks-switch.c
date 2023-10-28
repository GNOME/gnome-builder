/* ide-tweaks-switch.c
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

#define G_LOG_DOMAIN "ide-tweaks-switch"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks-switch.h"

struct _IdeTweaksSwitch
{
  IdeTweaksWidget parent_instance;
  char *title;
  char *subtitle;
};

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksSwitch, ide_tweaks_switch, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_switch_create_for_item (IdeTweaksWidget *instance,
                                   IdeTweaksItem   *widget)
{
  IdeTweaksSwitch *self = (IdeTweaksSwitch *)widget;
  IdeTweaksBinding *binding;
  AdwSwitchRow *row;

  g_assert (IDE_IS_TWEAKS_WIDGET (widget));

  if (!(binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (self))))
    return NULL;

  row = g_object_new (ADW_TYPE_SWITCH_ROW,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      NULL);
  ide_tweaks_binding_bind (binding, row, "active");

  return GTK_WIDGET (row);
}

static void
ide_tweaks_switch_dispose (GObject *object)
{
  IdeTweaksSwitch *self = (IdeTweaksSwitch *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

  G_OBJECT_CLASS (ide_tweaks_switch_parent_class)->dispose (object);
}

static void
ide_tweaks_switch_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksSwitch *self = IDE_TWEAKS_SWITCH (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_switch_get_subtitle (self));
      break;

    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_switch_get_title (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_switch_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksSwitch *self = IDE_TWEAKS_SWITCH (object);

  switch (prop_id)
    {
    case PROP_SUBTITLE:
      ide_tweaks_switch_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      ide_tweaks_switch_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_switch_class_init (IdeTweaksSwitchClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->dispose = ide_tweaks_switch_dispose;
  object_class->get_property = ide_tweaks_switch_get_property;
  object_class->set_property = ide_tweaks_switch_set_property;

  widget_class->create_for_item = ide_tweaks_switch_create_for_item;

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
ide_tweaks_switch_init (IdeTweaksSwitch *self)
{
}

const char *
ide_tweaks_switch_get_subtitle (IdeTweaksSwitch *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SWITCH (self), NULL);

  return self->subtitle;
}

void
ide_tweaks_switch_set_subtitle (IdeTweaksSwitch *self,
                                const char      *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_SWITCH (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

const char *
ide_tweaks_switch_get_title (IdeTweaksSwitch *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SWITCH (self), NULL);

  return self->title;
}

void
ide_tweaks_switch_set_title (IdeTweaksSwitch *self,
                             const char      *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_SWITCH (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
