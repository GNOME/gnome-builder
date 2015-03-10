/* ide-battery-monitor.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

#include "ide-battery-monitor.h"

static GDBusProxy *gUPowerProxy;
static GDBusProxy *gUPowerDeviceProxy;

G_LOCK_DEFINE_STATIC (proxy_lock);

static GDBusProxy *
ide_battery_monitor_get_proxy (void)
{
  GDBusProxy *proxy = NULL;

  G_LOCK (proxy_lock);

  if (!gUPowerProxy)
    {
      GDBusConnection *bus;

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

      if (bus)
        {
          gUPowerProxy = g_dbus_proxy_new_sync (bus,
                                                G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                                NULL,
                                                "org.freedesktop.UPower",
                                                "/org/freedesktop/UPower",
                                                "org.freedesktop.UPower",
                                                NULL,
                                                NULL);
          g_object_unref (bus);
        }
    }

  proxy = gUPowerProxy ? g_object_ref (gUPowerProxy) : NULL;

  G_UNLOCK (proxy_lock);

  return proxy;
}

static GDBusProxy *
ide_battery_monitor_get_device_proxy (void)
{
  GDBusProxy *proxy = NULL;

  G_LOCK (proxy_lock);

  if (!gUPowerDeviceProxy)
    {
      GDBusConnection *bus;

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

      if (bus)
        {
          gUPowerDeviceProxy = g_dbus_proxy_new_sync (bus,
                                                      G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                                      NULL,
                                                      "org.freedesktop.UPower",
                                                      "/org/freedesktop/UPower/devices/DisplayDevice",
                                                      "org.freedesktop.UPower.Device",
                                                      NULL,
                                                      NULL);
          g_object_unref (bus);
        }
    }

  proxy = gUPowerDeviceProxy ? g_object_ref (gUPowerDeviceProxy) : NULL;

  G_UNLOCK (proxy_lock);

  return proxy;
}

gboolean
ide_battery_monitor_get_on_battery (void)
{
  GDBusProxy *proxy;
  gboolean ret = FALSE;

  proxy = ide_battery_monitor_get_proxy ();

  if (proxy)
    {
      GVariant *prop;

      prop = g_dbus_proxy_get_cached_property (proxy, "OnBattery");
      if (prop)
        ret = g_variant_get_boolean (prop);
      g_object_unref (proxy);
    }

  return ret;
}

gdouble
ide_battery_monitor_get_energy (void)
{
  GDBusProxy *proxy;
  gdouble ret = 0.0;

  proxy = ide_battery_monitor_get_device_proxy ();

  if (proxy)
    {
      GVariant *prop;

      prop = g_dbus_proxy_get_cached_property (proxy, "Energy");
      if (prop)
        ret = g_variant_get_double (prop);
      g_object_unref (proxy);
    }

  return ret;
}

gboolean
ide_battery_monitor_get_should_conserve (void)
{
  gboolean should_conserve = FALSE;

  if (ide_battery_monitor_get_on_battery ())
    {
      gdouble energy;

      energy = ide_battery_monitor_get_energy ();
      should_conserve = (energy != 0.0) && (energy < 0.50);
    }

  return should_conserve;
}
