/* ide-device-info.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-device-info"

#include "config.h"

#include "ide-device-info.h"
#include "ide-enums.h"

#include "util/ide-posix.h"

struct _IdeDeviceInfo
{
  GObject parent_instance;
  IdeTriplet *triplet;
  IdeDeviceKind kind;
};

G_DEFINE_TYPE (IdeDeviceInfo, ide_device_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_KIND,
  PROP_TRIPLET,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_device_info_finalize (GObject *object)
{
  IdeDeviceInfo *self = (IdeDeviceInfo *)object;

  g_clear_pointer (&self->triplet, ide_triplet_unref);

  G_OBJECT_CLASS (ide_device_info_parent_class)->finalize (object);
}

static void
ide_device_info_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeDeviceInfo *self = IDE_DEVICE_INFO (object);

  switch (prop_id)
    {
    case PROP_KIND:
      g_value_set_enum (value, ide_device_info_get_kind (self));
      break;

    case PROP_TRIPLET:
      g_value_set_boxed (value, ide_device_info_get_triplet (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_info_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeDeviceInfo *self = IDE_DEVICE_INFO (object);

  switch (prop_id)
    {
    case PROP_KIND:
      ide_device_info_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_TRIPLET:
      ide_device_info_set_triplet (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_device_info_class_init (IdeDeviceInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_autofree gchar *arch = ide_get_system_arch ();

  object_class->finalize = ide_device_info_finalize;
  object_class->get_property = ide_device_info_get_property;
  object_class->set_property = ide_device_info_set_property;

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The device kind",
                       IDE_TYPE_DEVICE_KIND,
                       IDE_DEVICE_KIND_COMPUTER,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_TRIPLET] =
    g_param_spec_boxed ("triplet",
                        "Triplet",
                        "The #IdeTriplet object holding all the configuration name values",
                        IDE_TYPE_TRIPLET,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_device_info_init (IdeDeviceInfo *self)
{
  self->triplet = ide_triplet_new_from_system ();
}

IdeDeviceInfo *
ide_device_info_new (void)
{
  return g_object_new (IDE_TYPE_DEVICE_INFO, NULL);
}

/**
 * ide_device_info_get_kind:
 * @self: An #IdeDeviceInfo
 *
 * Get the #IdeDeviceKind of the device describing the type of device @self refers to
 *
 * Returns: An #IdeDeviceKind.
 */
IdeDeviceKind
ide_device_info_get_kind (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), 0);

  return self->kind;
}


/**
 * ide_device_info_set_kind:
 * @self: An #IdeDeviceInfo
 * @kind: An #IdeDeviceKind
 *
 * Set the #IdeDeviceKind of the device describing the type of device @self refers to
 */
void
ide_device_info_set_kind (IdeDeviceInfo *self,
                          IdeDeviceKind  kind)
{
  g_return_if_fail (IDE_IS_DEVICE_INFO (self));

  if (self->kind != kind)
    {
      self->kind = kind;
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KIND]);
    }
}

/**
 * ide_device_info_get_triplet:
 * @self: An #IdeDeviceInfo
 *
 * Get the #IdeTriplet object describing the configuration name
 * of the Device (its architecture…)
 *
 * Returns: (transfer none) (nullable): An #IdeTriplet.
 *
 * Since: 3.30
 */
IdeTriplet *
ide_device_info_get_triplet (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), NULL);

  return self->triplet;
}

/**
 * ide_device_info_set_triplet:
 * @self: An #IdeDeviceInfo
 *
 * Set the #IdeTriplet object describing the configuration name
 *
 * Since: 3.30
 */
void
ide_device_info_set_triplet (IdeDeviceInfo *self,
                             IdeTriplet    *triplet)
{
  g_return_if_fail (IDE_IS_DEVICE_INFO (self));

  g_clear_pointer (&self->triplet, ide_triplet_unref);
  self->triplet = ide_triplet_ref (triplet);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_TRIPLET]);
}
