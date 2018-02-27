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

#include "ide-device-info.h"
#include "ide-enums.h"

#include "util/ide-posix.h"

struct _IdeDeviceInfo
{
  GObject parent_instance;
  gchar *arch;
  gchar *kernel;
  gchar *system;
  IdeDeviceKind kind;
};

G_DEFINE_TYPE (IdeDeviceInfo, ide_device_info, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_ARCH,
  PROP_KERNEL,
  PROP_KIND,
  PROP_SYSTEM,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_device_info_finalize (GObject *object)
{
  IdeDeviceInfo *self = (IdeDeviceInfo *)object;

  g_clear_pointer (&self->arch, g_free);
  g_clear_pointer (&self->kernel, g_free);
  g_clear_pointer (&self->system, g_free);

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
    case PROP_ARCH:
      g_value_set_string (value, ide_device_info_get_arch (self));
      break;

    case PROP_KERNEL:
      g_value_set_string (value, ide_device_info_get_kernel (self));
      break;

    case PROP_KIND:
      g_value_set_enum (value, ide_device_info_get_kind (self));
      break;

    case PROP_SYSTEM:
      g_value_set_string (value, ide_device_info_get_system (self));
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
    case PROP_ARCH:
      ide_device_info_set_arch (self, g_value_get_string (value));
      break;

    case PROP_KERNEL:
      ide_device_info_set_kernel (self, g_value_get_string (value));
      break;

    case PROP_KIND:
      ide_device_info_set_kind (self, g_value_get_enum (value));
      break;

    case PROP_SYSTEM:
      ide_device_info_set_system (self, g_value_get_string (value));
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

  properties [PROP_ARCH] =
    g_param_spec_string ("arch",
                         "Arch",
                         "The architecture of the device, such as x86_64",
                         arch,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_KERNEL] =
    g_param_spec_string ("kernel",
                         "Kernel",
                         "The operating system kernel, such as Linux",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_KIND] =
    g_param_spec_enum ("kind",
                       "Kind",
                       "The device kind",
                       IDE_TYPE_DEVICE_KIND,
                       IDE_DEVICE_KIND_COMPUTER,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties [PROP_SYSTEM] =
    g_param_spec_string ("system",
                         "System",
                         "The system kind, such as 'gnu'",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_device_info_init (IdeDeviceInfo *self)
{
  self->arch = ide_get_system_arch ();
}

IdeDeviceInfo *
ide_device_info_new (void)
{
  return g_object_new (IDE_TYPE_DEVICE_INFO, NULL);
}

const gchar *
ide_device_info_get_arch (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), NULL);

  return self->arch;
}

const gchar *
ide_device_info_get_kernel (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), NULL);

  return self->kernel;
}

const gchar *
ide_device_info_get_system (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), NULL);

  return self->system;
}

IdeDeviceKind
ide_device_info_get_kind (IdeDeviceInfo *self)
{
  g_return_val_if_fail (IDE_IS_DEVICE_INFO (self), 0);

  return self->kind;
}

void
ide_device_info_set_arch (IdeDeviceInfo *self,
                          const gchar   *arch)
{
  g_return_if_fail (IDE_IS_DEVICE_INFO (self));

  if (g_strcmp0 (arch, self->arch) != 0)
    {
      g_free (self->arch);
      self->arch = g_strdup (arch);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ARCH]);
    }
}

void
ide_device_info_set_kernel (IdeDeviceInfo *self,
                            const gchar   *kernel)
{
  g_return_if_fail (IDE_IS_DEVICE_INFO (self));

  if (g_strcmp0 (kernel, self->kernel) != 0)
    {
      g_free (self->kernel);
      self->kernel = g_strdup (kernel);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_KERNEL]);
    }
}

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

void
ide_device_info_set_system (IdeDeviceInfo *self,
                            const gchar   *system)
{
  g_return_if_fail (IDE_IS_DEVICE_INFO (self));

  if (g_strcmp0 (system, self->system) != 0)
    {
      g_free (self->system);
      self->system = g_strdup (system);
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SYSTEM]);
    }
}
