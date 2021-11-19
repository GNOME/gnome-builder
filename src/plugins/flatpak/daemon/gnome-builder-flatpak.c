/* gnome-builder-flatpak.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#define G_LOG_DOMAIN "gnome-builder-flatpak"

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <flatpak/flatpak.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
# include <sys/types.h>
# include <sys/syscall.h>
#endif

#ifdef __FreeBSD__
#include <sys/procctl.h>
#endif

#include "ipc-flatpak-repo.h"
#include "ipc-flatpak-service.h"
#include "ipc-flatpak-service-impl.h"

static GDBusConnection *
create_connection (GIOStream  *stream,
                   GMainLoop  *main_loop,
                   GError    **error)
{
  GDBusConnection *ret;

  g_assert (G_IS_IO_STREAM (stream));
  g_assert (main_loop != NULL);
  g_assert (error != NULL);

  if ((ret = g_dbus_connection_new_sync (stream, NULL,
                                          G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                          NULL, NULL, error)))
    {
      g_dbus_connection_set_exit_on_close (ret, FALSE);
      g_signal_connect_swapped (ret, "closed", G_CALLBACK (g_main_loop_quit), main_loop);
    }

  return ret;
}

static inline gint
log_get_thread (void)
{
#ifdef __linux__
  return (gint) syscall (SYS_gettid);
#else
  return GPOINTER_TO_INT (g_thread_self ());
#endif /* __linux__ */
}

static const char *
log_level_str (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:    return "   ERROR";
    case G_LOG_LEVEL_CRITICAL: return "CRITICAL";
    case G_LOG_LEVEL_WARNING:  return " WARNING";
    case G_LOG_LEVEL_MESSAGE:  return " MESSAGE";
    case G_LOG_LEVEL_INFO:     return "    INFO";
    case G_LOG_LEVEL_DEBUG:    return "   DEBUG";

    default:
      return " UNKNOWN";
    }
}

static const char *
log_level_str_with_color (GLogLevelFlags log_level)
{
  switch (((gulong)log_level & G_LOG_LEVEL_MASK))
    {
    case G_LOG_LEVEL_ERROR:    return "   \033[1;31mERROR\033[0m";
    case G_LOG_LEVEL_CRITICAL: return "\033[1;35mCRITICAL\033[0m";
    case G_LOG_LEVEL_WARNING:  return " \033[1;33mWARNING\033[0m";
    case G_LOG_LEVEL_MESSAGE:  return " \033[1;32mMESSAGE\033[0m";
    case G_LOG_LEVEL_INFO:     return "    \033[1;32mINFO\033[0m";
    case G_LOG_LEVEL_DEBUG:    return "   \033[1;32mDEBUG\033[0m";

    default:
      return " UNKNOWN";
    }
}

static int read_fileno = STDIN_FILENO;
static int write_fileno = STDOUT_FILENO;
static char *data_dir;
static gboolean verbose;
static gboolean ignore_system_installations;
static GOptionEntry main_entries[] = {
  { "read-fd", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &read_fileno },
  { "write-fd", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &write_fileno },
  { "data-dir", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME, &data_dir },
  { "ignore-system", 0, 0, G_OPTION_ARG_NONE, &ignore_system_installations },
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose },
  { 0 }
};

static void
log_func (const gchar    *log_domain,
          GLogLevelFlags  flags,
          const gchar    *message,
          gpointer        user_data)
{
  const char *level;
  gint64 now;
  struct tm tt;
  time_t t;
  char ftime[32];
  char *str;
  int fd;

  if (!verbose && flags > G_LOG_LEVEL_MESSAGE)
    return;

  if (user_data)
    level = log_level_str_with_color (flags);
  else
    level = log_level_str (flags);

  now = g_get_real_time ();
  t = now / G_USEC_PER_SEC;
  tt = *localtime (&t);
  strftime (ftime, sizeof (ftime), "%H:%M:%S", &tt);

  str = g_strdup_printf ("%s.%04d  %40s[% 5d]: %s: %s\n",
                         ftime,
                         (gint)((now % G_USEC_PER_SEC) / 100L),
                         log_domain,
                         log_get_thread (),
                         level,
                         message);
  fd = write_fileno == STDOUT_FILENO ? STDERR_FILENO : STDOUT_FILENO;
  write (fd, str, strlen (str));
  g_free (str);
}

gint
main (gint argc,
      gchar *argv[])
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(IpcFlatpakService) service = NULL;
  g_autoptr(GOutputStream) stdout_stream = NULL;
  g_autoptr(GInputStream) stdin_stream = NULL;
  g_autoptr(GIOStream) stream = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;

  g_log_set_default_handler (log_func, GINT_TO_POINTER (isatty (STDOUT_FILENO)));

  g_set_prgname ("gnome-builder-flatpak");
  g_set_application_name ("gnome-builder-flatpak");

#ifdef __linux__
  prctl (PR_SET_PDEATHSIG, SIGTERM);
#elif defined(__FreeBSD__)
  procctl (P_PID, 0, PROC_PDEATHSIG_CTL, &(int){ SIGTERM });
#else
# warning "Please submit a patch to support parent-death signal on your OS"
#endif

  signal (SIGPIPE, SIG_IGN);

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, main_entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    goto error;

  if (!g_unix_set_fd_nonblocking (read_fileno, TRUE, &error) ||
      !g_unix_set_fd_nonblocking (write_fileno, TRUE, &error))
    goto error;

  main_loop = g_main_loop_new (NULL, FALSE);
  stdin_stream = g_unix_input_stream_new (read_fileno, FALSE);
  stdout_stream = g_unix_output_stream_new (write_fileno, FALSE);
  stream = g_simple_io_stream_new (stdin_stream, stdout_stream);

  if (!(connection = create_connection (stream, main_loop, &error)))
    goto error;

  g_dbus_connection_set_exit_on_close (connection, FALSE);
  g_signal_connect_swapped (connection, "closed", G_CALLBACK (g_main_loop_quit), main_loop);

  ipc_flatpak_repo_load (data_dir);

  service = ipc_flatpak_service_impl_new (ignore_system_installations);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                         connection,
                                         "/org/gnome/Builder/Flatpak",
                                         &error))
    goto error;

  g_debug ("Message processing started.");
  g_dbus_connection_start_message_processing (connection);
  g_main_loop_run (main_loop);

  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (service));
  g_dbus_connection_close_sync (connection, NULL, NULL);

  return EXIT_SUCCESS;

error:
  if (error != NULL)
    g_printerr ("%s\n", error->message);

  return EXIT_FAILURE;
}
