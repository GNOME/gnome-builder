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

#include <gtk/gtk.h>
#include <libpeas.h>
#include <string.h>

#include <libide-gui.h>
#include <ide-build-ident.h>

#include "ide-support-private.h"

typedef struct
{
  GString *str;
  guint depth;
} ObjectPrintf;

static void
object_printf_recurse (IdeObject    *object,
                       ObjectPrintf *state)
{
  g_autofree char *repr = ide_object_repr (object);

  for (guint i = 0; i < state->depth; i++)
    {
      g_string_append_c (state->str, ' ');
      g_string_append_c (state->str, ' ');
    }

  g_string_append (state->str, repr);
  g_string_append_c (state->str, '\n');

  state->depth++;
  ide_object_foreach (object, (GFunc)object_printf_recurse, state);
  state->depth--;
}

static void
ide_support_foreach_workbench_cb (gpointer data,
                                  gpointer user_data)
{
  IdeWorkbench *workbench = data;
  g_autofree char *title = NULL;
  GString *str = user_data;
  IdeContext *context;
  ObjectPrintf state;

  g_assert (IDE_IS_WORKBENCH (workbench));
  g_assert (str != NULL);

  context = ide_workbench_get_context (workbench);
  title = ide_context_dup_title (context);

  g_string_append_c (str, '\n');
  g_string_append_printf (str, "[Workbench %s]\n", title);

  state.depth = 0;
  state.str = str;

  object_printf_recurse (IDE_OBJECT (context), &state);
}

gchar *
ide_get_support_log (void)
{
  PeasEngine *engine = peas_engine_get_default ();
  GListModel *monitors;
  GChecksum *checksum;
  GDateTime *now;
  GDateTime *started_at;
  GString *str;
  gchar *tmp;
  gchar **env;
  guint n_items;
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
  g_string_append_printf (str, "libadwaita = \"%u.%u.%u\"\n",
                          adw_get_major_version (),
                          adw_get_minor_version (),
                          adw_get_micro_version ());
  g_string_append (str, "\n");

  g_string_append (str, "[build.libraries]\n");
  g_string_append_printf (str, "glib = \"%u.%u.%u\"\n",
                          GLIB_MAJOR_VERSION,
                          GLIB_MINOR_VERSION,
                          GLIB_MICRO_VERSION);
  g_string_append_printf (str, "gtk = \"%u.%u.%u\"\n",
                          GTK_MAJOR_VERSION,
                          GTK_MINOR_VERSION,
                          GTK_MICRO_VERSION);
  g_string_append_printf (str, "libadwaita = \"%u.%u.%u\"\n",
                          ADW_MAJOR_VERSION,
                          ADW_MINOR_VERSION,
                          ADW_MICRO_VERSION);
  g_string_append (str, "\n");

  /*
   * Log display server information.
   */
  display = gdk_display_get_default ();

  g_string_append (str, "[runtime.display]\n");
  g_string_append_printf (str, "name = \"%s\"\n", gdk_display_get_name (display));

  monitors = gdk_display_get_monitors (display);
  n_monitors = g_list_model_get_n_items (monitors);
  g_string_append_printf (str, "n_monitors = %u\n", n_monitors);
  for (guint i = 0; i < n_monitors; i++)
    {
      g_autoptr(GdkMonitor) monitor = g_list_model_get_item (monitors, i);
      GdkRectangle geom;

      gdk_monitor_get_geometry (monitor, &geom);
      g_string_append_printf (str, "geometry[%u] = [%u,%u]\n",
                              i, geom.width, geom.height);
    }
  g_string_append (str, "\n");

  /*
   * Log the list of plugins and if they are enabled.
   */
  g_string_append (str, "[runtime.plugins]\n");
  n_items = g_list_model_get_n_items (G_LIST_MODEL (engine));
  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(PeasPluginInfo) info = g_list_model_get_item (G_LIST_MODEL (engine), i);
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
  for (guint i = 0; env [i]; i++)
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
  g_string_append (str, "\n\n");

  ide_application_foreach_workbench (IDE_APPLICATION_DEFAULT,
                                     ide_support_foreach_workbench_cb,
                                     str);

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
