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
  char *title;
  char *subtitle;
  guint digits;
};

enum {
  PROP_0,
  PROP_DIGITS,
  PROP_SUBTITLE,
  PROP_TITLE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksSpin, ide_tweaks_spin, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties [N_PROPS];

static GtkWidget *
ide_tweaks_spin_create_for_item (IdeTweaksWidget *instance,
                                 IdeTweaksItem   *widget)
{
  IdeTweaksSpin *self = (IdeTweaksSpin *)widget;
  IdeTweaksBinding *binding;
  GtkAdjustment *adjustment = NULL;
  AdwSpinRow *row;

  g_assert (IDE_IS_TWEAKS_SPIN (self));

  if ((binding = ide_tweaks_widget_get_binding (IDE_TWEAKS_WIDGET (self))) &&
      (adjustment = ide_tweaks_binding_create_adjustment (binding)))
    ide_tweaks_binding_bind (binding, adjustment, "value");

  row = g_object_new (ADW_TYPE_SPIN_ROW,
                      "title", self->title,
                      "subtitle", self->subtitle,
                      "adjustment", adjustment,
                      "digits", self->digits,
                      NULL);

  return GTK_WIDGET (row);
}

static void
ide_tweaks_spin_dispose (GObject *object)
{
  IdeTweaksSpin *self = (IdeTweaksSpin *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

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
    case PROP_DIGITS:
      g_value_set_uint (value, ide_tweaks_spin_get_digits (self));
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
    case PROP_DIGITS:
      ide_tweaks_spin_set_digits (self, g_value_get_uint (value));
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

  widget_class->create_for_item = ide_tweaks_spin_create_for_item;

  properties[PROP_DIGITS] =
    g_param_spec_uint ("digits", NULL, NULL,
                       0, 6, 0,
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

guint
ide_tweaks_spin_get_digits (IdeTweaksSpin *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_SPIN (self), 0);

  return self->digits;
}

void
ide_tweaks_spin_set_digits (IdeTweaksSpin *self,
                            guint          digits)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (digits != self->digits)
    {
      self->digits = digits;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DIGITS]);
    }
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
ide_tweaks_spin_set_subtitle (IdeTweaksSpin *self,
                              const char    *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}

void
ide_tweaks_spin_set_title (IdeTweaksSpin *self,
                           const char    *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_SPIN (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}
