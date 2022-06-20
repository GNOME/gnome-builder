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
#include <signal.h>
#include <string.h>

static GMainLoop *main_loop;
static GInetSocketAddress *address;
static DevdProcessService *procsvc;
static char *pty_id;
static char *process_id;
static struct {
  gboolean exited;
  int exit_code;
  int term_sig;
} exit_info;
static guint fail_source;
static guint signal_source;
static int signal_to_proxy;
static char *opt_address;
static char *opt_app_id;
static int opt_port;
static int opt_pty_fd = -1;
static int opt_timeout_seconds = 10;
static GOptionEntry options[] = {
  { "address", 0, 0, G_OPTION_ARG_STRING, &opt_address, N_("The device address") },
  { "port", 0, 0, G_OPTION_ARG_INT, &opt_port, N_("The device port number") },
  { "app-id", 0, 0, G_OPTION_ARG_STRING, &opt_app_id, N_("The application to run") },
  { "pty-fd", 0, 0, G_OPTION_ARG_INT, &opt_pty_fd, N_("A PTY to bidirectionally proxy to the device") },
  { "timeout", 0, 0, G_OPTION_ARG_INT, &opt_timeout_seconds, N_("Number of seconds to wait for the deviced peer to appear") },
  { NULL }
};

static void
proxy_signal (int signum)
{
  /* We need to be signal handler safe here of course, which means no
   * allocations, no locks, etc. Basically all we can do is read/write to FDs
   * or set some variables. So we just set the signal to be proxied and handle
   * it from the main loop on the next cycle through.
   */
  signal_to_proxy = signum;
}

static struct {
  int signum;
  sighandler_t previous;
} proxied_signals[] = {
  { SIGHUP },
  { SIGINT },
  { SIGQUIT },
  { SIGUSR1 },
  { SIGUSR2 },
  { SIGUSR2 },
  { SIGTERM },
#if 0
/* These signals cannot be handled and therefore cannot be proxied.
 * To do this, we'd need to create a monitor process that watches
 * us and sends the signal to the peer. Probably more effort than
 * it is worth if we're going to drop this and move towards Bonsai
 * anyway in the future.
 */
  { SIGSTOP },
  { SIGKILL },
#endif
};

static void
setup_signal_handling (void)
{
  /* Note: We could use signalfd() here on Linux and do this much
   * better than spinning our main loop occasionally. But that would
   * still require porting to other platforms and quite frankly it's
   * not really worth the effort due to how short the lifespan is of
   * applications running.
   */
  for (guint i = 0; i < G_N_ELEMENTS (proxied_signals); i++)
    proxied_signals[i].previous = signal (proxied_signals[i].signum, proxy_signal);
}

static void
tear_down_signal_handling (void)
{
  for (guint i = 0; i < G_N_ELEMENTS (proxied_signals); i++)
    signal (proxied_signals[i].signum, proxied_signals[i].previous);
}

static gboolean
fail_to_connect_cb (gpointer data)
{
  g_error ("Failed to locate target device, exiting!");
  return G_SOURCE_REMOVE;
}

static void
destroy_pty_cb (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
  DevdProcessService *process = (DevdProcessService *)object;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_PROCESS_SERVICE (process));

  if (!devd_process_service_destroy_pty_finish (process, result, &error))
    g_error ("Failed to destroy PTY: %s", error->message);

  g_clear_pointer (&process_id, g_free);

  tear_down_signal_handling ();

  g_main_loop_quit (main_loop);

  if (exit_info.exited)
    exit (exit_info.exit_code);
  else
    kill (getpid (), exit_info.term_sig);
}

static void
wait_for_process_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  DevdProcessService *process = (DevdProcessService *)object;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_PROCESS_SERVICE (process));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!devd_process_service_wait_for_process_finish (process,
                                                     result,
                                                     &exit_info.exited,
                                                     &exit_info.exit_code,
                                                     &exit_info.term_sig,
                                                     &error))
    g_error ("Failed to wait for process exit: %s", error->message);

  g_printerr ("Process exited\n");

  /* Clean up our PTY if we can */
  devd_process_service_destroy_pty_async (process,
                                          pty_id,
                                          NULL,
                                          destroy_pty_cb,
                                          NULL);
}

static void
client_run_app_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(process_id = devd_client_run_app_finish (client, result, &error)))
    g_error ("Failed to launch process: %s", error->message);

  setup_signal_handling ();

  devd_process_service_wait_for_process_async (procsvc,
                                               process_id,
                                               NULL,
                                               wait_for_process_cb,
                                               NULL);
}

static void
process_create_pty_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  DevdProcessService *process = (DevdProcessService *)object;
  g_autoptr(GError) error = NULL;
  DevdClient *client;

  g_assert (DEVD_IS_PROCESS_SERVICE (process));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!(pty_id = devd_process_service_create_pty_finish (process, result, &error)))
    g_error ("Failed to create PTY: %s", error->message);

  procsvc = g_object_ref (process);
  client = devd_service_get_client (DEVD_SERVICE (process));

  devd_client_run_app_async (client,
                             "flatpak",
                             opt_app_id,
                             pty_id,
                             NULL,
                             client_run_app_cb,
                             NULL);
}

static void
client_connect_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  DevdClient *client = (DevdClient *)object;
  g_autoptr(DevdProcessService) process = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DEVD_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!devd_client_connect_finish (client, result, &error))
    g_error ("Failed to connect to device: %s", error->message);

  if (!(process = devd_process_service_new (client, &error)))
    g_error ("Failed to locate process service: %s", error->message);

  g_clear_handle_id (&fail_source, g_source_remove);

  devd_process_service_create_pty_async (process,
                                         opt_pty_fd,
                                         NULL,
                                         process_create_pty_cb,
                                         NULL);
}

static gboolean
inet_socket_address_equal (GSocketAddress *a,
                           GSocketAddress *b)
{
  gsize a_size;
  gsize b_size;
  gpointer a_data;
  gpointer b_data;

  g_assert (G_IS_SOCKET_ADDRESS (a));
  g_assert (G_IS_SOCKET_ADDRESS (b));

  a_size = g_socket_address_get_native_size (a);
  b_size = g_socket_address_get_native_size (b);

  if (a_size != b_size)
    return FALSE;

  a_data = g_alloca0 (a_size);
  b_data = g_alloca0 (b_size);

  if (!g_socket_address_to_native (a, a_data, a_size, NULL) ||
      !g_socket_address_to_native (b, b_data, b_size, NULL))
    return FALSE;

  return memcmp (a_data, b_data, a_size) == 0;
}

static void
device_added_cb (DevdBrowser *browser,
                 DevdDevice  *device,
                 gpointer     user_data)
{
  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (DEVD_IS_DEVICE (device));

  if (DEVD_IS_NETWORK_DEVICE (device))
    {
      GInetSocketAddress *device_address = devd_network_device_get_address (DEVD_NETWORK_DEVICE (device));

      if (inet_socket_address_equal (G_SOCKET_ADDRESS (address),
                                     G_SOCKET_ADDRESS (device_address)))
        {
          g_autoptr(DevdClient) client = devd_device_create_client (device);

          g_signal_handlers_disconnect_by_func (browser,
                                                G_CALLBACK (device_added_cb),
                                                user_data);

          devd_client_connect_async (client,
                                     NULL,
                                     client_connect_cb,
                                     NULL);
        }
    }
}

static void
device_removed_cb (DevdBrowser *browser,
                   DevdDevice  *device,
                   gpointer     user_data)
{
  g_assert (DEVD_IS_BROWSER (browser));
  g_assert (DEVD_IS_DEVICE (device));

  if (DEVD_IS_NETWORK_DEVICE (device))
    {
      GInetSocketAddress *device_address = devd_network_device_get_address (DEVD_NETWORK_DEVICE (device));

      if (inet_socket_address_equal (G_SOCKET_ADDRESS (address),
                                     G_SOCKET_ADDRESS (device_address)))
        {
          /* We might not have, but avahi says so and we just need to be
           * extra careful so we don't hang indefinitely.
           */
          g_printerr ("lost connection from device\n");
          exit (EXIT_FAILURE);
        }
    }
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

static gboolean
signal_source_cb (gpointer data)
{
  if (signal_to_proxy != 0 && procsvc != NULL && process_id != NULL)
    {
      devd_process_service_send_signal (procsvc, process_id, signal_to_proxy);
      signal_to_proxy = 0;
    }

  return G_SOURCE_CONTINUE;
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
    return EXIT_FAILURE;

  if (!(address = G_INET_SOCKET_ADDRESS (g_inet_socket_address_new_from_string (opt_address, opt_port))))
    return EXIT_FAILURE;

  main_loop = g_main_loop_new (NULL, FALSE);

  browser = devd_browser_new ();
  g_signal_connect (browser, "device-added", G_CALLBACK (device_added_cb), NULL);
  g_signal_connect (browser, "device-removed", G_CALLBACK (device_removed_cb), NULL);
  devd_browser_load_async (browser, NULL, load_cb, NULL);

  fail_source = g_timeout_add_seconds (opt_timeout_seconds, fail_to_connect_cb, NULL);
  signal_source = g_timeout_add (500, signal_source_cb, NULL);

  g_main_loop_run (main_loop);

  return EXIT_SUCCESS;
}
