/* ide-application-color.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-application-color"

#include "config.h"

#include <gtksourceview/gtksource.h>

#include "ide-application.h"
#include "ide-application-private.h"

static void
add_style_name (GPtrArray   *ar,
                const gchar *base,
                gboolean     dark)
{
  g_ptr_array_add (ar, g_strdup_printf ("%s-%s", base, dark ? "dark" : "light"));
}

static gchar *
find_similar_style_scheme (const gchar *name,
                           gboolean     is_dark_mode)
{
  g_autoptr(GPtrArray) attempts = NULL;
  GtkSourceStyleSchemeManager *mgr;
  const gchar * const *scheme_ids;
  const gchar *dash;

  g_assert (name != NULL);

  attempts = g_ptr_array_new_with_free_func (g_free);

  mgr = gtk_source_style_scheme_manager_get_default ();
  scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (mgr);

  add_style_name (attempts, name, is_dark_mode);

  if ((dash = strrchr (name, '-')))
    {
      if (g_str_equal (dash, "-light") ||
          g_str_equal (dash, "-dark"))
        {
          g_autofree gchar *base = NULL;

          base = g_strndup (name, dash - name);
          add_style_name (attempts, base, is_dark_mode);

          /* Add the base name last so light/dark matches first */
          g_ptr_array_add (attempts, g_steal_pointer (&base));
        }
    }

  /*
   * Instead of using gtk_source_style_scheme_manager_get_scheme(), we get the
   * IDs and look using case insensitive search so that its more likely we get
   * a match when something is named Dark or Light in the id.
   */

  for (guint i = 0; i < attempts->len; i++)
    {
      const gchar *attempt = g_ptr_array_index (attempts, i);

      for (guint j = 0; scheme_ids[j] != NULL; j++)
        {
          if (strcasecmp (attempt, scheme_ids[j]) == 0)
            return g_strdup (scheme_ids[j]);
        }
    }

  return NULL;
}

static void
_ide_application_update_color (IdeApplication *self)
{
  static gboolean ignore_reentrant = FALSE;
  GtkSettings *gtk_settings;
  gboolean prefer_dark_theme;
  gboolean follow;
  gboolean night_mode;

  g_assert (IDE_IS_APPLICATION (self));

  if (ignore_reentrant)
    return;

  if (self->color_proxy == NULL || self->settings == NULL)
    return;

  ignore_reentrant = TRUE;

  g_assert (G_IS_SETTINGS (self->settings));
  g_assert (G_IS_DBUS_PROXY (self->color_proxy));

  follow = g_settings_get_boolean (self->settings, "follow-night-light");
  night_mode = g_settings_get_boolean (self->settings, "night-mode");

  /*
   * If we are using the Follow Night Light feature, then we want to update
   * the application color based on the D-Bus NightLightActive property from
   * GNOME Shell.
   */

  if (follow)
    {
      g_autoptr(GVariant) activev = NULL;
      g_autoptr(GSettings) editor_settings = NULL;
      g_autofree gchar *old_name = NULL;
      g_autofree gchar *new_name = NULL;
      gboolean active;

      /*
       * Update our internal night-mode setting based on the GNOME Shell
       * Night Light setting.
       */

      activev = g_dbus_proxy_get_cached_property (self->color_proxy, "NightLightActive");
      active = g_variant_get_boolean (activev);

      if (active != night_mode)
        {
          night_mode = active;
          g_settings_set_boolean (self->settings, "night-mode", night_mode);
        }

      /*
       * Now that we have our color up to date, we need to possibly update the
       * color scheme to match the setting. We always do this (and not just when
       * the night-mode changes) so that we pick up changes at startup.
       *
       * Try to locate a corresponding style-scheme for the light/dark switch
       * based on some naming conventions. If found, switch the current style
       * scheme to match.
       */

      editor_settings = g_settings_new ("org.gnome.builder.editor");
      old_name = g_settings_get_string (editor_settings, "style-scheme-name");
      new_name = find_similar_style_scheme (old_name, night_mode);

      if (new_name != NULL)
        g_settings_set_string (editor_settings, "style-scheme-name", new_name);
    }

  gtk_settings = gtk_settings_get_default ();

  g_object_get (gtk_settings,
                "gtk-application-prefer-dark-theme", &prefer_dark_theme,
                NULL);

  if (prefer_dark_theme != night_mode)
    g_object_set (gtk_settings,
                  "gtk-application-prefer-dark-theme", night_mode,
                  NULL);

  ignore_reentrant = FALSE;
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
  g_return_if_fail (G_IS_SETTINGS (self->settings));

  if (g_getenv ("GTK_THEME") == NULL)
    {
      g_signal_connect_object (self->settings,
                               "changed::follow-night-light",
                               G_CALLBACK (_ide_application_update_color),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->settings,
                               "changed::night-mode",
                               G_CALLBACK (_ide_application_update_color),
                               self,
                               G_CONNECT_SWAPPED);
    }

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

  _ide_application_update_color (self);
}
