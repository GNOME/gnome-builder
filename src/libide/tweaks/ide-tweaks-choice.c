/* ide-tweaks-choice.c
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

#define G_LOG_DOMAIN "ide-tweaks-choice"

#include "config.h"

#include "ide-tweaks-choice.h"

struct _IdeTweaksChoice
{
  IdeTweaksItem parent_instance;
  char *title;
  char *subtitle;
  GVariant *value;
};

enum {
  PROP_0,
  PROP_SUBTITLE,
  PROP_TITLE,
  PROP_VALUE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksChoice, ide_tweaks_choice, IDE_TYPE_TWEAKS_ITEM)

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_choice_dispose (GObject *object)
{
  IdeTweaksChoice *self = (IdeTweaksChoice *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->value, g_variant_unref);

  G_OBJECT_CLASS (ide_tweaks_choice_parent_class)->dispose (object);
}

static void
ide_tweaks_choice_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksChoice *self = IDE_TWEAKS_CHOICE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, ide_tweaks_choice_get_title (self));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, ide_tweaks_choice_get_subtitle (self));
      break;

    case PROP_VALUE:
      g_value_set_variant (value, ide_tweaks_choice_get_value (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_choice_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksChoice *self = IDE_TWEAKS_CHOICE (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      ide_tweaks_choice_set_title (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      ide_tweaks_choice_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_VALUE:
      ide_tweaks_choice_set_value (self, g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_choice_class_init (IdeTweaksChoiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_choice_dispose;
  object_class->get_property = ide_tweaks_choice_get_property;
  object_class->set_property = ide_tweaks_choice_set_property;

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  properties[PROP_VALUE] =
    g_param_spec_variant ("value", NULL, NULL,
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_choice_init (IdeTweaksChoice *self)
{
}

const char *
ide_tweaks_choice_get_title (IdeTweaksChoice *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_CHOICE (self), NULL);

  return self->title;
}

const char *
ide_tweaks_choice_get_subtitle (IdeTweaksChoice *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_CHOICE (self), NULL);

  return self->subtitle;
}

/**
 * ide_tweaks_choice_get_value:
 * @self: a #IdeTweaksChoice
 *
 * Returns: (transfer none) (nullable): A #GVariant or %NULL
 */
GVariant *
ide_tweaks_choice_get_value (IdeTweaksChoice *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_CHOICE (self), NULL);

  return self->value;
}

void
ide_tweaks_choice_set_value (IdeTweaksChoice *self,
                             GVariant        *value)
{
  g_return_if_fail (IDE_IS_TWEAKS_CHOICE (self));

  if (value == self->value)
    return;

  if (value != NULL)
    g_variant_ref_sink (value);

  g_clear_pointer (&self->value, g_variant_unref);
  self->value = value;
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_VALUE]);
}

void
ide_tweaks_choice_set_title (IdeTweaksChoice *self,
                             const char      *title)
{
  g_return_if_fail (IDE_IS_TWEAKS_CHOICE (self));

  if (g_set_str (&self->title, title))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TITLE]);
}

void
ide_tweaks_choice_set_subtitle (IdeTweaksChoice *self,
                                const char      *subtitle)
{
  g_return_if_fail (IDE_IS_TWEAKS_CHOICE (self));

  if (g_set_str (&self->subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUBTITLE]);
}
