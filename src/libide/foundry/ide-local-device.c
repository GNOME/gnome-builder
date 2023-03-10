/* ide-local-device.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-loca-device"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-threading.h>
#include <string.h>
#include <sys/utsname.h>

#include "ide-local-device.h"
#include "ide-device.h"
#include "ide-device-info.h"
#include "ide-triplet.h"

typedef struct
{
  IdeTriplet *triplet;
} IdeLocalDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLocalDevice, ide_local_device, IDE_TYPE_DEVICE)

enum {
  PROP_0,
  PROP_TRIPLET,
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
  ide_device_info_set_host_triplet (info, priv->triplet);

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
    case PROP_TRIPLET:
      g_value_set_boxed (value, priv->triplet);
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
    case PROP_TRIPLET:
      priv->triplet = g_value_dup_boxed (value);
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
  g_autofree gchar *name = NULL;

  g_assert (IDE_IS_LOCAL_DEVICE (self));

  if (priv->triplet == NULL)
    priv->triplet = ide_triplet_new_from_system ();

  if (ide_triplet_is_system (priv->triplet))
    {
      /* translators: %s is replaced with the host name */
      name = g_strdup_printf (_("My Computer (%s)"), g_get_host_name ());
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), "local");
    }
  else
    {
      const gchar *arch = ide_triplet_get_arch (priv->triplet);
      g_autofree gchar *id = g_strdup_printf ("local:%s", arch);

      /* translators: first %s is replaced with the host name, second with CPU architecture */
      name = g_strdup_printf (_("My Computer (%s) — %s"), g_get_host_name (), arch);
      ide_device_set_display_name (IDE_DEVICE (self), name);
      ide_device_set_id (IDE_DEVICE (self), id);
    }

  G_OBJECT_CLASS (ide_local_device_parent_class)->constructed (object);
}

static char *
ide_local_device_repr (IdeObject *object)
{
  IdeLocalDevice *self = IDE_LOCAL_DEVICE (object);
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  if (priv->triplet == NULL)
    return g_strdup_printf ("%s default", G_OBJECT_TYPE_NAME (self));

  return g_strdup_printf ("%s name=\"%s\" arch=\"%s\" vendor=\"%s\" kernel=\"%s\" operating-system=\"%s\"",
                          G_OBJECT_TYPE_NAME (self),
                          ide_triplet_get_full_name (priv->triplet) ?: "",
                          ide_triplet_get_arch (priv->triplet) ?: "",
                          ide_triplet_get_vendor (priv->triplet) ?: "",
                          ide_triplet_get_kernel (priv->triplet) ?: "",
                          ide_triplet_get_operating_system (priv->triplet) ?: "");
}

static void
ide_local_device_finalize (GObject *object)
{
  IdeLocalDevice *self = (IdeLocalDevice *)object;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  g_clear_pointer (&priv->triplet, ide_triplet_unref);

  G_OBJECT_CLASS (ide_local_device_parent_class)->finalize (object);
}

static void
ide_local_device_class_init (IdeLocalDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->constructed = ide_local_device_constructed;
  object_class->finalize = ide_local_device_finalize;
  object_class->get_property = ide_local_device_get_property;
  object_class->set_property = ide_local_device_set_property;

  i_object_class->repr = ide_local_device_repr;

  device_class->get_info_async = ide_local_device_get_info_async;
  device_class->get_info_finish = ide_local_device_get_info_finish;

  properties [PROP_TRIPLET] =
    g_param_spec_boxed ("triplet",
                        "Triplet",
                        "The #IdeTriplet object describing the local device configuration name",
                        IDE_TYPE_TRIPLET,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_local_device_init (IdeLocalDevice *self)
{
}
