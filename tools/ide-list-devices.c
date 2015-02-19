/* ide-list-devices.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <ide.h>
#include <stdlib.h>

static GMainLoop *gMainLoop;
static gint gExitCode = EXIT_SUCCESS;

static void
quit (gint exit_code)
{
  gExitCode = exit_code;
  g_main_loop_quit (gMainLoop);
}

static void
settled_cb (IdeDeviceManager *device_manager,
            GParamSpec       *pspec,
            gpointer          user_data)
{
  GPtrArray *devices;
  guint i;

  if (!ide_device_manager_get_settled (device_manager))
    return;

  devices = ide_device_manager_get_devices (device_manager);
  for (i = 0; i < devices->len; i++)
    {
      IdeDevice *device = g_ptr_array_index (devices, i);
      const gchar *device_id = ide_device_get_id (device);
      const gchar *display_name = ide_device_get_display_name (device);
      const gchar *system_type = ide_device_get_system_type (device);

      g_print ("  %s \"%s\" (%s)\n", device_id, display_name, system_type);
    }
  g_ptr_array_unref (devices);

  quit (EXIT_SUCCESS);
}

static void
context_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
  g_autoptr(IdeContext) context = NULL;
  g_autoptr(GError) error = NULL;
  IdeDeviceManager *device_manager;

  context = ide_context_new_finish (result, &error);

  if (!context)
    {
      g_printerr ("%s\n", error->message);
      quit (EXIT_FAILURE);
      return;
    }

  device_manager = ide_context_get_device_manager (context);

  if (!ide_device_manager_get_settled (device_manager))
    g_signal_connect (device_manager, "notify::settled",
                      G_CALLBACK (settled_cb), NULL);
  else
    settled_cb (device_manager, NULL, NULL);
}

gint
main (gint   argc,
      gchar *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) project_file = NULL;
  const gchar *project_path = ".";

  ide_set_program_name ("gnome-builder");
  g_set_prgname ("ide-list-devices");

  context = g_option_context_new (_("- List devices found on the system."));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  gMainLoop = g_main_loop_new (NULL, FALSE);

  if (argc > 1)
    project_path = argv [1];
  project_file = g_file_new_for_path (project_path);

  ide_context_new_async (project_file, NULL, context_cb, NULL);

  g_main_loop_run (gMainLoop);
  g_clear_pointer (&gMainLoop, g_main_loop_unref);

  return gExitCode;
}
