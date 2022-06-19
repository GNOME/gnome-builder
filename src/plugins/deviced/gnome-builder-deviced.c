/* gnome-builder-deviced.c
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

#include "config.h"

#include <glib/gi18n.h>
#include <libdeviced.h>
#include <stdlib.h>

static GMainLoop *main_loop;
static char *opt_address;
static char *opt_app_id;
static int opt_pty_fd = -1;
static GOptionEntry options[] = {
  { "address", 0, 0, G_OPTION_ARG_STRING, &opt_address, N_("The device address") },
  { "app-id", 0, 0, G_OPTION_ARG_STRING, &opt_app_id, N_("The application to run") },
  { "pty-fd", 0, 0, G_OPTION_ARG_INT, &opt_pty_fd, N_("A PTY to bidirectionally proxy to the device") },
  { NULL }
};

static void
device_added_cb (DevdBrowser *browser,
                 DevdDevice  *device,
                 gpointer     user_data)
{
  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (DEVD_IS_DEVICE (device));

  g_printerr ("%s added\n", devd_device_get_name (device));
}

static void
device_removed_cb (DevdBrowser *browser,
                   DevdDevice  *device,
                   gpointer     user_data)
{
  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (DEVD_IS_DEVICE (device));

  g_printerr ("%s removed\n", devd_device_get_name (device));
}

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
  DevdBrowser *browser = (DevdBrowser *)object;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (user_data == NULL);

  if (!devd_browser_load_finish (browser, result, &error))
    g_error ("%s", error->message);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(DevdBrowser) browser = NULL;
  g_autoptr(GError) error = NULL;

  context = g_option_context_new ("gnome-builder-deviced");
  g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  if (opt_address == NULL || opt_app_id == NULL)
    {
      g_printerr ("You must provide --address and --app-id\n");
      return EXIT_FAILURE;
    }

  main_loop = g_main_loop_new (NULL, FALSE);

  browser = devd_browser_new ();
  g_signal_connect (browser, "device-added", G_CALLBACK (device_added_cb), NULL);
  g_signal_connect (browser, "device-removed", G_CALLBACK (device_removed_cb), NULL);
  devd_browser_load_async (browser, NULL, load_cb, NULL);

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
