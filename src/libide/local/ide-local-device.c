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
#include "threading/ide-task.h"

typedef struct
{
  gchar *system_type;
  gchar *arch;
  gchar *kernel;
  gchar *system;
} IdeLocalDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLocalDevice, ide_local_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_ARCH,
  PROP_KERNEL,
  PROP_SYSTEM,
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
  const gchar *system_type = NULL;
  g_auto(GStrv) parts = NULL;
  g_autofree gchar *arch = NULL;

  g_assert (IDE_IS_LOCAL_DEVICE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (device, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_local_device_get_info_async);
  ide_task_set_check_cancellable (task, FALSE);

  system_type = ide_get_system_type ();
  arch = ide_get_system_arch ();
  parts = g_strsplit (system_type, "-", 3);

  info = ide_device_info_new ();
  ide_device_info_set_arch (info, arch);

  if (parts[1] != NULL)
    {
      ide_device_info_set_kernel (info, parts[1]);
      if (parts[2] != NULL)
        ide_device_info_set_system (info, parts[2]);
    }

  /* Now override anything that was specified in the device */

  if (priv->arch != NULL)
    ide_device_info_set_arch (info, priv->arch);
  if (priv->kernel != NULL)
    ide_device_info_set_kernel (info, priv->kernel);
  if (priv->system != NULL)
    ide_device_info_set_system (info, priv->system);

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
    case PROP_ARCH:
      g_value_set_string (value, priv->arch);
      break;

    case PROP_KERNEL:
      g_value_set_string (value, priv->kernel);
      break;

    case PROP_SYSTEM:
      g_value_set_string (value, priv->system);
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
    case PROP_ARCH:
      priv->arch = g_value_dup_string (value);
      break;

    case PROP_KERNEL:
      priv->kernel = g_value_dup_string (value);
      break;

    case PROP_SYSTEM:
      priv->system = g_value_dup_string (value);
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
  const gchar *system_type;

  g_assert (IDE_IS_LOCAL_DEVICE (self));

  G_OBJECT_CLASS (ide_local_device_parent_class)->constructed (object);

  arch = ide_get_system_arch ();
  system_type = ide_get_system_type ();
  parts = g_strsplit (system_type, "-", 3);

  /* Parse our system type into the 3 pieces. We'll use this
   * to reconstruct our system_type property in case the caller
   * changed the arch manually (say from x86_64 to i386).
   */
  if (parts[0] != NULL)
    {
      if (priv->arch == NULL)
        priv->arch = g_strdup (parts[0]);

      if (parts[1] != NULL)
        {
          if (priv->kernel == NULL)
            priv->kernel = g_strdup (parts[1]);

          if (parts[2] != NULL)
            {
              if (priv->system == NULL)
                priv->system = g_strdup (parts[2]);
            }
        }
    }

  priv->system_type = g_strdup_printf ("%s-%s-%s", priv->arch, priv->kernel, priv->system);

  if (g_strcmp0 (arch, priv->arch) == 0)
    {
      /* translators: %s is replaced with the host name */
      name = g_strdup_printf (_("My Computer (%s)"), g_get_host_name ());
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), "local");
    }
  else
    {
      g_autofree gchar *id = g_strdup_printf ("local:%s", priv->arch);

      /* translators: first %s is replaced with the host name, second with CPU architecture */
      name = g_strdup_printf (_("My Computer (%s) — %s"), g_get_host_name (), priv->arch);
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), id);
    }
}

static void
ide_local_device_finalize (GObject *object)
{
  IdeLocalDevice *self = (IdeLocalDevice *)object;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  g_clear_pointer (&priv->arch, g_free);
  g_clear_pointer (&priv->kernel, g_free);
  g_clear_pointer (&priv->system, g_free);
  g_clear_pointer (&priv->system_type, g_free);

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

  properties [PROP_ARCH] =
    g_param_spec_string ("arch",
                         "Arch",
                         "The architecture of the local device",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_KERNEL] =
    g_param_spec_string ("kernel",
                         "Kernel",
                         "The kernel of the local device",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_SYSTEM] =
    g_param_spec_string ("system",
                         "System",
                         "The system of the local device, such as 'gnu'",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_local_device_init (IdeLocalDevice *self)
{
}
