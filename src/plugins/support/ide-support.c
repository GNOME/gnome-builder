/* ide-support.c
 *
 * Copyright 2014-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-support"

#include "config.h"

#include <dazzle.h>
#include <gtk/gtk.h>
#include <libide-gui.h>
#include <ide-build-ident.h>
#include <libpeas/peas.h>
#include <string.h>

#include "ide-support.h"

static gchar *
str_to_key (const gchar *str)
{
  return g_strdelimit (g_strdup (str), " ", '_');
}

static void
counter_arena_foreach_cb (DzlCounter *counter,
                          gpointer    user_data)
{
  GString *str = (GString *)user_data;
  g_autofree gchar *category = str_to_key (counter->category);
  g_autofree gchar *name = str_to_key (counter->name);

  g_string_append_printf (str,
                          "%s.%s = %"G_GINT64_FORMAT"\n",
                          category, name, dzl_counter_get (counter));
}

gchar *
ide_get_support_log (void)
{
  PeasEngine *engine = peas_engine_get_default ();
  const GList *plugins;
  GChecksum *checksum;
  GDateTime *now;
  GDateTime *started_at;
  GString *str;
  gchar *tmp;
  gchar **env;
  guint i;
  GdkDisplay *display;
  guint n_monitors;

  str = g_string_new (NULL);

  g_string_append (str, "[runtime.version]\n");
  g_string_append_printf (str, "version = \"%s\"\n", PACKAGE_VERSION);
  g_string_append_printf (str, "channel = \"%s\"\n", IDE_BUILD_CHANNEL);
  g_string_append_printf (str, "identifier = \"%s\"\n", IDE_BUILD_IDENTIFIER);
  g_string_append (str, "\n");

  /*
   * Log host information.
   */
  g_string_append (str, "[runtime.host]\n");
  g_string_append_printf (str, "hostname = \"%s\"\n", g_get_host_name ());
  g_string_append_printf (str, "username = \"%s\"\n", g_get_user_name ());
  g_string_append_printf (str, "codeset = \"%s\"\n", g_get_codeset ());
  g_string_append_printf (str, "cpus = %u\n", g_get_num_processors ());
  g_string_append_printf (str, "cache_dir = \"%s\"\n", g_get_user_cache_dir ());
  g_string_append_printf (str, "data_dir = \"%s\"\n", g_get_user_data_dir ());
  g_string_append_printf (str, "config_dir = \"%s\"\n", g_get_user_config_dir ());
  g_string_append_printf (str, "runtime_dir = \"%s\"\n", g_get_user_runtime_dir ());
  g_string_append_printf (str, "home_dir = \"%s\"\n", g_get_home_dir ());
  g_string_append_printf (str, "tmp_dir = \"%s\"\n", g_get_tmp_dir ());
  tmp = g_get_current_dir ();
  g_string_append_printf (str, "current_dir = \"%s\"\n", tmp);
  g_free (tmp);

  started_at = ide_application_get_started_at (IDE_APPLICATION_DEFAULT);
  tmp = g_date_time_format (started_at, "%FT%H:%M:%SZ");
  g_string_append_printf (str, "started-at = \"%s\"\n", tmp);
  g_free (tmp);

  now = g_date_time_new_now_utc ();
  tmp = g_date_time_format (now, "%FT%H:%M:%SZ");
  g_string_append_printf (str, "generated-at = \"%s\"\n", tmp);
  g_free (tmp);
  g_date_time_unref (now);

  g_string_append (str, "\n");

  /*
   * Log various library versions to the log.
   */
  g_string_append (str, "[runtime.libraries]\n");
  g_string_append_printf (str, "glib = \"%u.%u.%u\"\n",
                          glib_major_version,
                          glib_minor_version,
                          glib_micro_version);
  g_string_append_printf (str, "gtk = \"%u.%u.%u\"\n",
                          gtk_get_major_version (),
                          gtk_get_minor_version (),
                          gtk_get_micro_version ());
  g_string_append (str, "\n");

  /*
   * Log display server information.
   */
  display = gdk_display_get_default ();

  g_string_append (str, "[runtime.display]\n");
  g_string_append_printf (str, "name = \"%s\"\n", gdk_display_get_name (display));

  n_monitors = gdk_display_get_n_monitors (display);
  g_string_append_printf (str, "n_monitors = %u\n", n_monitors);
  for (i = 0; i < n_monitors; i++)
    {
      GdkMonitor *monitor;
      GdkRectangle geom;

      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &geom);
      g_string_append_printf (str, "geometry[%u] = [%u,%u]\n",
                              i, geom.width, geom.height);
    }
  g_string_append (str, "\n");

  /*
   * Log the list of plugins and if they are enabled.
   */
  g_string_append (str, "[runtime.plugins]\n");
  plugins = peas_engine_get_plugin_list (engine);
  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      const PeasPluginInfo *info = iter->data;
      const gchar *name = peas_plugin_info_get_module_name (info);

      g_string_append_printf (str, "%s = %s\n",
                              name,
                              peas_plugin_info_is_loaded (info) ? "loaded" : "unloaded");
    }
  g_string_append (str, "\n");

  /*
   * Log the environment variables.
   */
  g_string_append (str, "[runtime.environ]\n");
  env = g_get_environ ();
  for (i = 0; env [i]; i++)
    {
      const gchar *value;
      gchar *escape;
      gchar *key;

      value = strchr (env [i], '=');
      if (!value)
        continue;

      escape = g_strescape (env [i], NULL);
      key = g_strndup (env [i], value - env [i]);

      g_string_append_printf (str, "%s = \"%s\"\n", key, escape);

      g_free (escape);
      g_free (key);
    }
  g_strfreev (env);
  g_string_append (str, "\n");

  /*
   * Log the counters.
   */
  g_string_append (str, "[runtime.counters]\n");
  dzl_counter_arena_foreach (dzl_counter_arena_get_default (),
                             counter_arena_foreach_cb, str);

  g_string_append (str, "\n\n");

  /*
   * Add simple checksum for validation at the end.
   * Not that anyone would alter the results or anything...
   */
  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, (const guint8 *)str->str, str->len);
  g_string_append (str, g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return g_string_free (str, FALSE);
}
