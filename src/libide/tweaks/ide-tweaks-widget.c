/* ide-tweaks-widget.c
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

#define G_LOG_DOMAIN "ide-tweaks-widget"

#include "config.h"

#include "ide-tweaks-widget-private.h"

struct _IdeTweaksWidget
{
  IdeTweaksItem parent_instance;
  GType widget_type;
};

G_DEFINE_FINAL_TYPE (IdeTweaksWidget, ide_tweaks_widget, IDE_TYPE_TWEAKS_ITEM)

enum {
  PROP_0,
  PROP_WIDGET_TYPE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_tweaks_widget_dispose (GObject *object)
{
  IdeTweaksWidget *self = (IdeTweaksWidget *)object;

  G_OBJECT_CLASS (ide_tweaks_widget_parent_class)->dispose (object);
}

static void
ide_tweaks_widget_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeTweaksWidget *self = IDE_TWEAKS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_WIDGET_TYPE:
      g_value_set_gtype (value, ide_tweaks_widget_get_widget_type (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_widget_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeTweaksWidget *self = IDE_TWEAKS_WIDGET (object);

  switch (prop_id)
    {
    case PROP_WIDGET_TYPE:
      ide_tweaks_widget_set_widget_type (self, g_value_get_gtype (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_tweaks_widget_class_init (IdeTweaksWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_tweaks_widget_dispose;
  object_class->get_property = ide_tweaks_widget_get_property;
  object_class->set_property = ide_tweaks_widget_set_property;

  properties [PROP_WIDGET_TYPE] =
    g_param_spec_gtype ("widget-type", NULL, NULL,
                        GTK_TYPE_WIDGET,
                         (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_tweaks_widget_init (IdeTweaksWidget *self)
{
}

IdeTweaksWidget *
ide_tweaks_widget_new (void)
{
  return g_object_new (IDE_TYPE_TWEAKS_WIDGET, NULL);
}

GType
ide_tweaks_widget_get_widget_type (IdeTweaksWidget *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WIDGET (self), G_TYPE_INVALID);

  return self->widget_type;
}

void
ide_tweaks_widget_set_widget_type (IdeTweaksWidget *self,
                                   GType            widget_type)
{
  g_return_if_fail (IDE_IS_TWEAKS_WIDGET (self));
  g_return_if_fail (!widget_type || g_type_is_a (widget_type, GTK_TYPE_WIDGET));

  if (self->widget_type != widget_type)
    {
      self->widget_type = widget_type;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDGET_TYPE]);
    }
}

GtkWidget *
_ide_tweaks_widget_inflate (IdeTweaksWidget *self)
{
  g_return_val_if_fail (IDE_IS_TWEAKS_WIDGET (self), NULL);
  g_return_val_if_fail (self->widget_type != G_TYPE_INVALID, NULL);

  return g_object_new (self->widget_type, NULL);
}
