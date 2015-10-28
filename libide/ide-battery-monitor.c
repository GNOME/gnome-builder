/* ide-battery-monitor.c
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

#include <gio/gio.h>

#include "ide-battery-monitor.h"

#define CONSERVE_THRESHOLD 50.0

static GDBusProxy *u_powerProxy;
static GDBusProxy *u_powerDeviceProxy;
static gint        u_powerHold;

G_LOCK_DEFINE_STATIC (proxy_lock);

static GDBusProxy *
ide_battery_monitor_get_proxy (void)
{
  GDBusProxy *proxy = NULL;

  G_LOCK (proxy_lock);

  if (!u_powerProxy)
    {
      GDBusConnection *bus;

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

      if (bus)
        {
          u_powerProxy = g_dbus_proxy_new_sync (bus,
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

  proxy = u_powerProxy ? g_object_ref (u_powerProxy) : NULL;

  G_UNLOCK (proxy_lock);

  return proxy;
}

static GDBusProxy *
ide_battery_monitor_get_device_proxy (void)
{
  GDBusProxy *proxy = NULL;

  G_LOCK (proxy_lock);

  if (!u_powerDeviceProxy)
    {
      GDBusConnection *bus;

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

      if (bus)
        {
          u_powerDeviceProxy = g_dbus_proxy_new_sync (bus,
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

  proxy = u_powerDeviceProxy ? g_object_ref (u_powerDeviceProxy) : NULL;

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
ide_battery_monitor_get_energy_percentage (void)
{
  GDBusProxy *proxy;
  gdouble ret = 0.0;

  proxy = ide_battery_monitor_get_device_proxy ();

  if (proxy)
    {
      GVariant *prop;

      prop = g_dbus_proxy_get_cached_property (proxy, "Percentage");
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

      energy = ide_battery_monitor_get_energy_percentage ();
      should_conserve = (energy != 0.0) && (energy < CONSERVE_THRESHOLD);
    }

  return should_conserve;
}

void
_ide_battery_monitor_shutdown (void)
{
  G_LOCK (proxy_lock);

  if (--u_powerHold == 0)
    {
      g_clear_object (&u_powerProxy);
      g_clear_object (&u_powerDeviceProxy);
    }

  G_UNLOCK (proxy_lock);
}

void
_ide_battery_monitor_init (void)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GDBusProxy) device_proxy = NULL;

  G_LOCK (proxy_lock);
  u_powerHold++;
  G_UNLOCK (proxy_lock);

  proxy = ide_battery_monitor_get_proxy ();
  device_proxy = ide_battery_monitor_get_device_proxy ();
}
