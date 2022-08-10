/* ide-tweaks-external.c
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

#define G_LOG_DOMAIN "ide-tweaks-external"

#include "config.h"

#include <adwaita.h>

#include "ide-tweaks-external.h"

struct _IdeTweaksExternal
{
  IdeTweaksWidget parent_instance;
  GType widget_type;
};

enum {
  PROP_0,
  PROP_WIDGET_TYPE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (IdeTweaksExternal, ide_tweaks_external, IDE_TYPE_TWEAKS_WIDGET)

static GParamSpec *properties[N_PROPS];

static GtkWidget *
ide_tweaks_external_create (IdeTweaksWidget *widget)
{
  IdeTweaksExternal *self = (IdeTweaksExternal *)widget;

  g_assert (IDE_IS_TWEAKS_EXTERNAL (self));

  if (self->widget_type == G_TYPE_INVALID)
    return NULL;

  return g_object_new (self->widget_type, NULL);
}

static void
ide_tweaks_external_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  IdeTweaksExternal *self = IDE_TWEAKS_EXTERNAL (object);

  switch (prop_id)
    {
    case PROP_WIDGET_TYPE:
      g_value_set_gtype (value, ide_tweaks_external_get_widget_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_external_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  IdeTweaksExternal *self = IDE_TWEAKS_EXTERNAL (object);

  switch (prop_id)
    {
    case PROP_WIDGET_TYPE:
      ide_tweaks_external_set_widget_type (self, g_value_get_gtype (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_external_class_init (IdeTweaksExternalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeTweaksWidgetClass *widget_class = IDE_TWEAKS_WIDGET_CLASS (klass);

  object_class->get_property = ide_tweaks_external_get_property;
  object_class->set_property = ide_tweaks_external_set_property;

  widget_class->create = ide_tweaks_external_create;

  properties[PROP_WIDGET_TYPE] =
    g_param_spec_gtype ("widget-type", NULL, NULL,
                        GTK_TYPE_WIDGET,
                        (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_external_init (IdeTweaksExternal *self)
{
}

GType
ide_tweaks_external_get_widget_type (IdeTweaksExternal *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_EXTERNAL (self), G_TYPE_INVALID);

  return self->widget_type;
}

void
ide_tweaks_external_set_widget_type (IdeTweaksExternal *self,
                                     GType              widget_type)
{
  g_return_if_fail (IDE_IS_TWEAKS_EXTERNAL (self));
  g_return_if_fail (!widget_type || g_type_is_a (widget_type, GTK_TYPE_WIDGET));

  if (self->widget_type != widget_type)
    {
      self->widget_type = widget_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_WIDGET_TYPE]);
    }
}
