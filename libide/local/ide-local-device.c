/* ide-local-device.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#include <string.h>
#include <sys/utsname.h>

#include "ide-local-device.h"

typedef struct
{
  gchar *system_type;
} IdeLocalDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeLocalDevice, ide_local_device, IDE_TYPE_DEVICE)

static gchar *
get_system_type (void)
{
  g_autofree gchar *os_lower = NULL;
  const gchar *machine = NULL;
  struct utsname u;

  if (uname (&u) < 0)
    return g_strdup ("unknown");

  os_lower = g_utf8_strdown (u.sysname, -1);

  /* config.sub doesn't accept amd64-OS */
  machine = strcmp (u.machine, "amd64") ? u.machine : "x86_64";

  /*
   * TODO: Clearly we want to discover "gnu", but that should be just fine
   *       for a default until we try to actually run on something non-gnu.
   *       Which seems unlikely at the moment. If you run FreeBSD, you can
   *       probably fix this for me :-) And while you're at it, make the
   *       uname() call more portable.
   */

#ifdef __GLIBC__
  return g_strdup_printf ("%s-%s-%s", machine, os_lower, "gnu");
#else
  return g_strdup_printf ("%s-%s", machine, os_lower);
#endif
}

static const gchar *
ide_local_device_get_system_type (IdeDevice *device)
{
  IdeLocalDevice *self = (IdeLocalDevice *)device;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_LOCAL_DEVICE (device), NULL);
  g_return_val_if_fail (IDE_IS_LOCAL_DEVICE (self), NULL);

  return priv->system_type;
}

static void
ide_local_device_finalize (GObject *object)
{
  IdeLocalDevice *self = (IdeLocalDevice *)object;
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  g_clear_pointer (&priv->system_type, g_free);

  G_OBJECT_CLASS (ide_local_device_parent_class)->finalize (object);
}

static void
ide_local_device_class_init (IdeLocalDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeDeviceClass *device_class = IDE_DEVICE_CLASS (klass);

  object_class->finalize = ide_local_device_finalize;

  device_class->get_system_type = ide_local_device_get_system_type;
}

static void
ide_local_device_init (IdeLocalDevice *self)
{
  IdeLocalDevicePrivate *priv = ide_local_device_get_instance_private (self);

  priv->system_type = get_system_type ();

  ide_device_set_display_name (IDE_DEVICE (self), g_get_host_name ());
  ide_device_set_id (IDE_DEVICE (self), "local");
}
