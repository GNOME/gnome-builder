/* ide-application-settings.c
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

#define G_LOG_DOMAIN "ide-application-settings"

#include "config.h"

#include <libide-sourceview.h>

#include "ide-application-private.h"
#include "ide-recoloring-private.h"

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

static void
on_portal_settings_changed_cb (IdeApplication *self,
                               const char     *sender_name,
                               const char     *signal_name,
                               GVariant       *parameters,
                               gpointer        user_data)
{
  g_autoptr(GVariant) value = NULL;
  const char *schema_id;
  const char *key;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (sender_name != NULL);
  g_assert (signal_name != NULL);

  if (g_strcmp0 (signal_name, "SettingChanged") != 0)
    return;

  g_variant_get (parameters, "(&s&sv)", &schema_id, &key, &value);

  if (g_strcmp0 (schema_id, "org.gnome.desktop.interface") == 0 &&
      g_strcmp0 (key, "monospace-font-name") == 0 &&
      g_strcmp0 (g_variant_get_string (value, NULL), "") != 0)
    {
      g_free (self->system_font_name);
      self->system_font_name = g_strdup (g_variant_get_string (value, NULL));
      g_object_notify (G_OBJECT (self), "system-font-name");
      g_object_notify (G_OBJECT (self), "system-font");
    }
}

static void
parse_portal_settings (IdeApplication *self,
                       GVariant       *parameters)
{
  GVariantIter *iter = NULL;
  const char *schema_str;
  GVariant *val;

  g_assert (IDE_IS_APPLICATION (self));

  if (parameters == NULL)
    return;

  g_variant_get (parameters, "(a{sa{sv}})", &iter);

  while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema_str, &val))
    {
      GVariantIter *iter2 = g_variant_iter_new (val);
      const char *key;
      GVariant *v;

      while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
        {
          if (g_strcmp0 (schema_str, "org.gnome.desktop.interface") == 0 &&
              g_strcmp0 (key, "monospace-font-name") == 0 &&
              g_strcmp0 (g_variant_get_string (v, NULL), "") != 0)
            {
              g_free (self->system_font_name);
              self->system_font_name = g_strdup (g_variant_get_string (v, NULL));
            }
        }

      g_variant_iter_free (iter2);
    }

  g_variant_iter_free (iter);
}

static void
ide_application_settings_style_scheme_changed_cb (IdeApplication *self,
                                                  const char     *key,
                                                  GSettings      *settings)
{
  IDE_ENTRY;

  g_assert (IDE_IS_APPLICATION (self));
  g_assert (G_IS_SETTINGS (settings));

  g_object_notify (G_OBJECT (self), "style-scheme");

  IDE_EXIT;
}

void
_ide_application_init_settings (IdeApplication *self)
{
  static const char *patterns[] = { "org.gnome.*", NULL };
  g_autoptr(GVariant) all = NULL;
  g_autofree char *style_scheme_name = NULL;

  g_return_if_fail (IDE_IS_APPLICATION (self));
  g_return_if_fail (self->settings_portal == NULL);

  /* We must query the key to get changed notifications */
  style_scheme_name = g_settings_get_string (self->editor_settings, "style-scheme-name");
  g_debug ("Initial style scheme set to %s", style_scheme_name);
  g_signal_connect_object (self->editor_settings,
                           "changed::style-scheme-name",
                           G_CALLBACK (ide_application_settings_style_scheme_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  self->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL,
                                                         PORTAL_BUS_NAME,
                                                         PORTAL_OBJECT_PATH,
                                                         PORTAL_SETTINGS_INTERFACE,
                                                         NULL,
                                                         NULL);

  if (self->settings_portal != NULL)
    {
      g_signal_connect_object (self->settings_portal,
                               "g-signal",
                               G_CALLBACK (on_portal_settings_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
      all = g_dbus_proxy_call_sync (self->settings_portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    NULL);
      parse_portal_settings (self, all);
    }
}

void
ide_application_set_style_scheme (IdeApplication *self,
                                  const char     *style_scheme)
{
  g_return_if_fail (IDE_IS_APPLICATION (self));

  if (style_scheme == NULL)
    style_scheme = "Adwaita";

  g_object_freeze_notify (G_OBJECT (self));
  g_settings_set_string (self->editor_settings, "style-scheme-name", style_scheme);
  g_object_thaw_notify (G_OBJECT (self));
}

const char *
ide_application_get_style_scheme (IdeApplication *self)
{
  GtkSourceStyleSchemeManager *style_scheme_manager;
  GtkSourceStyleScheme *style_scheme;
  AdwStyleManager *style_manager;
  g_autofree char *style_scheme_id = NULL;
  const char *variant;

  g_return_val_if_fail (IDE_IS_APPLICATION (self), NULL);

  style_manager = adw_style_manager_get_default ();
  style_scheme_manager = gtk_source_style_scheme_manager_get_default ();
  style_scheme_id = g_settings_get_string (self->editor_settings, "style-scheme-name");

  /* Fallback to Adwaita if we don't find a match */
  if (gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, style_scheme_id) == NULL)
    {
      g_free (style_scheme_id);
      style_scheme_id = g_strdup ("Adwaita");
    }

  if (adw_style_manager_get_dark (style_manager))
    variant = "dark";
  else
    variant = "light";

  style_scheme = gtk_source_style_scheme_manager_get_scheme (style_scheme_manager, style_scheme_id);
  style_scheme = ide_source_style_scheme_get_variant (style_scheme, variant);

  return gtk_source_style_scheme_get_id (style_scheme);
}
