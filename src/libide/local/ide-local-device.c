/* ide-local-device.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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
 */

#define G_LOG_DOMAIN "ide-loca-device"

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>
#include <sys/utsname.h>

#include "local/ide-local-device.h"
#include "util/ide-posix.h"
#include "util/ide-machine-config-name.h"
#include "threading/ide-task.h"

typedef struct
{
  IdeMachineConfigName *machine_config_name;
} IdeLocalDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLocalDevice, ide_local_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_MACHINE_CONFIG_NAME,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
ide_local_device_get_info_async (IdeDevice           *device,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  IdeLocalDevice *self = (IdeLocalDevice *)device;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(IdeDeviceInfo) info = NULL;

  g_assert (IDE_IS_LOCAL_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (device, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_local_device_get_info_async);
  ide_task_set_check_cancellable (task, FALSE);

  info = ide_device_info_new ();
  ide_device_info_set_machine_config_name (info, priv->machine_config_name);

  ide_task_return_pointer (task, g_steal_pointer (&info), g_object_unref);
}

static IdeDeviceInfo *
ide_local_device_get_info_finish (IdeDevice     *device,
                                  GAsyncResult  *result,
                                  GError       **error)
{
  g_assert (IDE_IS_DEVICE (device));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_local_device_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  IdeLocalDevice *self = IDE_LOCAL_DEVICE (object);
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MACHINE_CONFIG_NAME:
      g_value_set_boxed (value, ide_machine_config_name_ref (priv->machine_config_name));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_local_device_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  IdeLocalDevice *self = IDE_LOCAL_DEVICE (object);
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_MACHINE_CONFIG_NAME:
      g_clear_pointer (&priv->machine_config_name, ide_machine_config_name_unref);
      priv->machine_config_name = ide_machine_config_name_ref (g_value_get_boxed (value));

      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_local_device_constructed (GObject *object)
{
  IdeLocalDevice *self = (IdeLocalDevice *)object;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);
  g_autofree gchar *arch = NULL;
  g_autofree gchar *name = NULL;
  g_auto(GStrv) parts = NULL;
  g_autoptr(IdeMachineConfigName) machine_config_name = NULL;

  g_assert (IDE_IS_LOCAL_DEVICE (self));

  G_OBJECT_CLASS (ide_local_device_parent_class)->constructed (object);

  arch = ide_get_system_arch ();
  machine_config_name = ide_machine_config_name_new_from_system ();
  if (priv->machine_config_name == NULL)
    priv->machine_config_name = g_steal_pointer (&machine_config_name);
  else
    {
      const gchar *current_cpu = ide_machine_config_name_get_cpu (machine_config_name);
      g_autofree gchar *device_cpu = g_strdup (current_cpu);
      g_autofree gchar *device_vendor = NULL;
      g_autofree gchar *device_kernel = NULL;
      g_autofree gchar *device_operating_system = NULL;
      const gchar *vendor;
      const gchar *kernel;
      const gchar *operating_system;

      vendor = ide_machine_config_name_get_vendor (priv->machine_config_name);
      if (vendor != NULL)
        device_vendor = g_strdup (vendor);
      else
        device_vendor = g_strdup (ide_machine_config_name_get_vendor (machine_config_name));

      kernel = ide_machine_config_name_get_kernel (priv->machine_config_name);
      if (kernel != NULL)
        device_kernel = g_strdup (kernel);
      else
        device_kernel = g_strdup (ide_machine_config_name_get_kernel (machine_config_name));

      operating_system = ide_machine_config_name_get_operating_system (priv->machine_config_name);
      if (operating_system != NULL)
        device_operating_system = g_strdup (operating_system);
      else
        device_operating_system = g_strdup (ide_machine_config_name_get_operating_system (machine_config_name));

      priv->machine_config_name =
        ide_machine_config_name_new_with_quadruplet (device_cpu,
                                                     device_vendor,
                                                     device_kernel,
                                                     device_operating_system);
    }

  if (ide_machine_config_name_is_system (priv->machine_config_name))
    {
      /* translators: %s is replaced with the host name */
      name = g_strdup_printf (_("My Computer (%s)"), g_get_host_name ());
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), "local");
    }
  else
    {
      const gchar *cpu = ide_machine_config_name_get_cpu (priv->machine_config_name);
      g_autofree gchar *id = g_strdup_printf ("local:%s", cpu);

      /* translators: first %s is replaced with the host name, second with CPU architecture */
      name = g_strdup_printf (_("My Computer (%s) — %s"), g_get_host_name (), cpu);
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), id);
    }
}

static void
ide_local_device_finalize (GObject *object)
{
  IdeLocalDevice *self = (IdeLocalDevice *)object;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  g_clear_pointer (&priv->machine_config_name, ide_machine_config_name_unref);

  G_OBJECT_CLASS (ide_local_device_parent_class)->finalize (object);
}

static void
ide_local_device_class_init (IdeLocalDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->constructed = ide_local_device_constructed;
  object_class->finalize = ide_local_device_finalize;
  object_class->get_property = ide_local_device_get_property;
  object_class->set_property = ide_local_device_set_property;

  device_class->get_info_async = ide_local_device_get_info_async;
  device_class->get_info_finish = ide_local_device_get_info_finish;

  properties [PROP_MACHINE_CONFIG_NAME] =
    g_param_spec_boxed ("machine-config-name",
                        "Machine Configuration Name",
                        "The #IdeMachineConfigName object describing the local device configuration name (also known as architecture triplet)",
                        IDE_TYPE_MACHINE_CONFIG_NAME,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_local_device_init (IdeLocalDevice *self)
{
}
