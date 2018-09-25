/* ide-application-color.c
 *
 * Copyright 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-color"

#include "config.h"

#include "application/ide-application.h"
#include "application/ide-application-private.h"

void
_ide_application_update_color (IdeApplication *self)
{
  g_assert (IDE_IS_APPLICATION (self));

  if (self->color_proxy == NULL || self->settings == NULL)
    return;

  if (g_settings_get_boolean (self->settings, "follow-night-light"))
    {
      g_autoptr(GVariant) activev = NULL;
      gboolean active;

      activev = g_dbus_proxy_get_cached_property (self->color_proxy, "NightLightActive");
      active = g_variant_get_boolean (activev);

      if (active != g_settings_get_boolean (self->settings, "night-mode"))
        g_settings_set_boolean (self->settings, "night-mode", active);
    }
}

static void
ide_application_color_properties_changed (IdeApplication      *self,
                                          GVariant            *properties,
                                          const gchar * const *invalidated,
                                          GDBusProxy          *proxy)
{
  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_DBUS_PROXY (proxy));

  _ide_application_update_color (self);
}

void
_ide_application_init_color (IdeApplication *self)
{
  g_autoptr(GDBusConnection) conn = NULL;
  g_autoptr(GDBusProxy) proxy = NULL;

  g_return_if_fail (IDE_IS_APPLICATION (self));

  if (NULL == (conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL)))
    return;

  if (NULL == (proxy = g_dbus_proxy_new_sync (conn,
                                              G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                              NULL,
                                              "org.gnome.SettingsDaemon.Color",
                                              "/org/gnome/SettingsDaemon/Color",
                                              "org.gnome.SettingsDaemon.Color",
                                              NULL, NULL)))
    return;

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (ide_application_color_properties_changed),
                           self,
                           G_CONNECT_SWAPPED);

  self->color_proxy = g_steal_pointer (&proxy);

  ide_application_color_properties_changed (self, NULL, NULL, self->color_proxy);
}
