/* gnome-builder-sysprof.c
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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include <ipc-sysprof.h>
#include <sysprof.h>

#define BUFFER_SIZE (4096L*16L) /* 64KB */

static gboolean forward_fd_func (const char  *option_name,
                                 const char  *option_value,
                                 gpointer     data,
                                 GError     **error);

static GMainLoop *main_loop;
static GSubprocess *subprocess;
static char *subprocess_ident;
static gboolean subprocess_finished;
static IpcSysprof *service;
static int exit_code = EXIT_SUCCESS;
static int read_fd = -1;
static int write_fd = -1;
static int pty_fd = -1;
static char *directory;
static char *capture_filename;
static GArray *forward_fds;
static char **env;
static gboolean clear_env;
static gboolean aid_battery;
static gboolean aid_compositor;
static gboolean aid_cpu;
static gboolean aid_disk;
static gboolean aid_energy;
static gboolean aid_gjs;
static gboolean aid_memory;
static gboolean aid_memprof;
static gboolean aid_net;
static gboolean aid_perf;
static gboolean aid_tracefd;
static gboolean no_throttle;
static const GOptionEntry options[] = {
  { "read-fd", 0, 0, G_OPTION_ARG_INT, &read_fd, "The read side of the FD to use for D-Bus" },
  { "write-fd", 0, 0, G_OPTION_ARG_INT, &write_fd, "The write side of the FD to use for D-Bus" },
  { "pty-fd", 0, 0, G_OPTION_ARG_INT, &pty_fd, "The FD of a PTY to use in the target process" },
  { "forward-fd", 0, 0, G_OPTION_ARG_CALLBACK, forward_fd_func, "The FD to forward to the subprocess" },
  { "directory", 0, 0, G_OPTION_ARG_FILENAME, &directory, "The directory to run spawn the subprocess from", "PATH" },
  { "capture", 0, 0, G_OPTION_ARG_FILENAME, &capture_filename, "The filename to save the sysprof capture to", "PATH" },
  { "clear-env", 0, 0, G_OPTION_ARG_NONE, &clear_env, "Clear environment instead of inheriting" },
  { "env", 0, 0, G_OPTION_ARG_STRING_ARRAY, &env, "Add an environment variable to the spawned process", "KEY=VALUE" },
  { "cpu", 0, 0, G_OPTION_ARG_NONE, &aid_cpu, "Track CPU usage and frequency" },
  { "gjs", 0, 0, G_OPTION_ARG_NONE, &aid_gjs, "Record stack traces within GJS" },
  { "perf", 0, 0, G_OPTION_ARG_NONE, &aid_perf, "Record stack traces with perf" },
  { "memory", 0, 0, G_OPTION_ARG_NONE, &aid_memory, "Record basic system memory usage" },
  { "memprof", 0, 0, G_OPTION_ARG_NONE, &aid_memprof, "Record stack traces during memory allocations" },
  { "disk", 0, 0, G_OPTION_ARG_NONE, &aid_disk, "Record disk usage information" },
  { "net", 0, 0, G_OPTION_ARG_NONE, &aid_net, "Record network usage information" },
  { "energy", 0, 0, G_OPTION_ARG_NONE, &aid_energy, "Record energy usage using RAPL" },
  { "battery", 0, 0, G_OPTION_ARG_NONE, &aid_battery, "Record battery charge and discharge rates" },
  { "compositor", 0, 0, G_OPTION_ARG_NONE, &aid_compositor, "Record GNOME Shell compositor information" },
  { "no-throttle", 0, 0, G_OPTION_ARG_NONE, &no_throttle, "Disable CPU throttling" },
  { "tracefd", 0, 0, G_OPTION_ARG_NONE, &aid_tracefd, "Provide TRACEFD to subprocess" },
  { NULL }
};

G_GNUC_PRINTF (1, 2)
static void
message (const char *format,
         ...)
{
  g_autofree char *formatted = NULL;
  va_list args;

  if (service == NULL)
    return;

  va_start (args, format);
  formatted = g_strdup_vprintf (format, args);
  va_end (args);

  ipc_sysprof_emit_log (service, formatted);
}

#define GBP_TYPE_SPAWN_SOURCE (gbp_spawn_source_get_type())
G_DECLARE_FINAL_TYPE (GbpSpawnSource, gbp_spawn_source, GBP, SPAWN_SOURCE, GObject)

struct _GbpSpawnSource
{
  GObject parent_instance;
};

static void
gbp_spawn_source_modify_spawn (SysprofSource    *source,
                               SysprofSpawnable *spawnable)
{
  g_assert (GBP_IS_SPAWN_SOURCE (source));
  g_assert (SYSPROF_IS_SPAWNABLE (spawnable));

  if (forward_fds == NULL)
    return;

  for (guint i = 0; i < forward_fds->len; i++)
    {
      int fd = g_array_index (forward_fds, int, i);
      sysprof_spawnable_take_fd (spawnable, dup (fd), fd);
    }

  if (pty_fd != -1)
    {
      sysprof_spawnable_take_fd (spawnable, dup (pty_fd), STDIN_FILENO);
      sysprof_spawnable_take_fd (spawnable, dup (pty_fd), STDOUT_FILENO);
      sysprof_spawnable_take_fd (spawnable, dup (pty_fd), STDERR_FILENO);
    }
}

static void
gbp_spawn_source_start (SysprofSource *source)
{
  sysprof_source_emit_ready (source);
}

static void
gbp_spawn_source_stop (SysprofSource *source)
{
  sysprof_source_emit_finished (source);
}

static void
spawn_source_init (SysprofSourceInterface *iface)
{
  iface->modify_spawn = gbp_spawn_source_modify_spawn;
  iface->start = gbp_spawn_source_start;
  iface->stop = gbp_spawn_source_stop;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpSpawnSource, gbp_spawn_source, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (SYSPROF_TYPE_SOURCE, spawn_source_init))

static void
gbp_spawn_source_class_init (GbpSpawnSourceClass *klass)
{
}

static void
gbp_spawn_source_init (GbpSpawnSource *self)
{
}

#define IPC_TYPE_SYSPROF_IMPL (ipc_sysprof_impl_get_type())
G_DECLARE_FINAL_TYPE (IpcSysprofImpl, ipc_sysprof_impl, IPC, SYPSROF_IMPL, IpcSysprofSkeleton)

struct _IpcSysprofImpl
{
  IpcSysprofSkeleton parent_instance;
};

static gboolean
handle_force_exit (IpcSysprof            *sysprof,
                   GDBusMethodInvocation *invocation)
{
  if (subprocess && !subprocess_finished)
    g_subprocess_force_exit (subprocess);

  ipc_sysprof_complete_force_exit (sysprof, invocation);

  return TRUE;
}

static gboolean
handle_send_signal (IpcSysprof            *sysprof,
                    GDBusMethodInvocation *invocation,
                    int                    signum)
{
  if (subprocess && !subprocess_finished)
    g_subprocess_send_signal (subprocess, signum);

  ipc_sysprof_complete_send_signal (sysprof, invocation);

  return TRUE;
}

static void
service_iface_init (IpcSysprofIface *iface)
{
  iface->handle_force_exit = handle_force_exit;
  iface->handle_send_signal = handle_send_signal;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IpcSysprofImpl, ipc_sysprof_impl, IPC_TYPE_SYSPROF_SKELETON,
                               G_IMPLEMENT_INTERFACE (IPC_TYPE_SYSPROF, service_iface_init))

static void
ipc_sysprof_impl_class_init (IpcSysprofImplClass *klass)
{
}

static void
ipc_sysprof_impl_init (IpcSysprofImpl *self)
{
}

static gboolean
forward_fd_func (const char  *option_name,
                 const char  *option_value,
                 gpointer     data,
                 GError     **error)
{
  int fd;

  if (forward_fds == NULL)
    forward_fds = g_array_new (FALSE, FALSE, sizeof (int));

  errno = 0;

  if (!(fd = atoi (option_value)) && errno != 0)
    {
      int errsv = errno;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   "--forward-fd must contain a file-descriptor: %s",
                   g_strerror (errsv));
      return FALSE;
    }

  if (fd < 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVAL,
                   "--forward-fd must be 0 or a positive integer");
      return FALSE;
    }

  g_array_append_val (forward_fds, fd);

  return TRUE;
}

G_GNUC_NULL_TERMINATED
static void
add_source (SysprofProfiler *profiler,
            gboolean         enabled,
            GType            source_type,
            ...)
{
  g_autoptr(SysprofSource) source = NULL;
  const char *first_property;
  va_list args;

  g_assert (SYSPROF_IS_PROFILER (profiler));
  g_assert (g_type_is_a (source_type, SYSPROF_TYPE_SOURCE));

  if (!enabled)
    return;

  va_start (args, source_type);
  first_property = va_arg (args, const char *);
  if (first_property != NULL)
    source = (SysprofSource *)g_object_new_valist (source_type, first_property, args);
  else
    source = g_object_new (source_type, NULL);
  va_end (args);

  g_assert (!source || SYSPROF_IS_SOURCE (source));

  if (source != NULL)
    sysprof_profiler_add_source (profiler, source);
  else
    g_printerr ("Failed to create source of type \"%s\"\n",
                g_type_name (source_type));
}

static void
profiler_failed_cb (SysprofProfiler *profiler,
                    const GError    *error)
{
  g_assert (SYSPROF_IS_LOCAL_PROFILER (profiler));
  g_assert (error != NULL);

  g_printerr ("Profiling failed: %s", error->message);
  exit_code = EXIT_FAILURE;
  g_main_loop_quit (main_loop);
}

static void
profiler_stopped_cb (SysprofProfiler *profiler)
{
  g_assert (SYSPROF_IS_LOCAL_PROFILER (profiler));

  g_main_loop_quit (main_loop);
}

static void
subprocess_spawned_cb (SysprofLocalProfiler *profiler,
                       GSubprocess          *new_subprocess)
{
  g_assert (SYSPROF_IS_LOCAL_PROFILER (profiler));
  g_assert (G_IS_SUBPROCESS (new_subprocess));

  g_set_object (&subprocess, new_subprocess);

  subprocess_ident = g_strdup (g_subprocess_get_identifier (subprocess));

  message ("Created process %s", subprocess_ident);
}

static void
subprocess_finished_cb (SysprofLocalProfiler *profiler,
                        GSubprocess          *new_subprocess)
{
  g_assert (SYSPROF_IS_LOCAL_PROFILER (profiler));
  g_assert (G_IS_SUBPROCESS (new_subprocess));

  subprocess_finished = TRUE;

  message ("Process %s exited", subprocess_ident);
}

static void
split_argv (int     argc,
            char  **argv,
            int    *our_argc,
            char ***our_argv,
            int    *sub_argc,
            char ***sub_argv)
{
  gboolean found_split = FALSE;

  *our_argc = 0;
  *our_argv = g_new0 (char *, 1);

  *sub_argc = 0;
  *sub_argv = g_new0 (char *, 1);

  for (int i = 0; i < argc; i++)
    {
      if (g_strcmp0 (argv[i], "--") == 0)
        {
          found_split = TRUE;
        }
      else if (found_split)
        {
          (*sub_argv) = g_realloc_n (*sub_argv, *sub_argc + 2, sizeof (char *));
          (*sub_argv)[*sub_argc] = g_strdup (argv[i]);
          (*sub_argv)[*sub_argc+1] = NULL;
          (*sub_argc)++;
        }
      else
        {
          (*our_argv) = g_realloc_n (*our_argv, *our_argc + 2, sizeof (char *));
          (*our_argv)[*our_argc] = g_strdup (argv[i]);
          (*our_argv)[*our_argc+1] = NULL;
          (*our_argc)++;
        }
    }
}

static void
warn_error (GError **error)
{
  if (*error)
    {
      g_warning ("%s", (*error)->message);
      g_clear_error (error);
    }
}

static GDBusConnection *
create_connection (GIOStream  *stream,
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

int
main (int   argc,
      char *argv[])
{
  g_autoptr(SysprofCaptureWriter) writer = NULL;
  g_autoptr(SysprofProfiler) profiler = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GDBusConnection) session_bus = NULL;
  g_autoptr(GDBusConnection) system_bus = NULL;
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GStrv) our_argv = NULL;
  g_auto(GStrv) sub_argv = NULL;
  GMainContext *main_context;
  int our_argc = -1;
  int sub_argc = -1;

  sysprof_clock_init ();

  g_set_prgname ("gnome-builder-sysprof");
  g_set_application_name ("gnome-builder-sysprof");

  /* Ignore SIGPIPE as we're using pipes to IPC */
  signal (SIGPIPE, SIG_IGN);

  /* Split argv into pre/post -- command split */
  split_argv (argc, argv, &our_argc, &our_argv, &sub_argc, &sub_argv);
  g_assert (our_argc >= 0);
  g_assert (sub_argc >= 0);

  /* Parse command line options pre -- */
  context = g_option_context_new ("-- COMMAND");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &our_argc, &our_argv, &error))
    {
      g_printerr ("%s\n", error->message);
      return EXIT_FAILURE;
    }

  /* Make sure we have a filename to capture to */
  if (capture_filename == NULL)
    {
      g_printerr ("You must provide --capture=PATH\n");
      return EXIT_FAILURE;
    }

  /* Setup main loop, we'll need it going forward for things
   * like async D-Bus, waiting for child processes, etc.
   */
  main_loop = g_main_loop_new (NULL, FALSE);

  /* First spin up our bus connections */
  if (!(session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error)))
    warn_error (&error);
  if (!(system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL)))
    warn_error (&error);

  /* Now setup our private p2p D-Bus connection to the controller */
  if (read_fd != -1 || write_fd != -1)
    {
      g_autoptr(GIOStream) stream = NULL;
      g_autoptr(GInputStream) in_stream = NULL;
      g_autoptr(GOutputStream) out_stream = NULL;

      /* Both must be set, not just one side */
      if (read_fd == -1 || write_fd == -1)
        {
          g_printerr ("You must specify both --read-fd and --write-fd\n");
          return EXIT_FAILURE;
        }

      /* We need these FDs non-blocking for async IO */
      if (!g_unix_set_fd_nonblocking (read_fd, TRUE, &error) ||
          !g_unix_set_fd_nonblocking (write_fd, TRUE, &error))
        {
          g_printerr ("Failed to set FDs in nonblocking mode: %s\n",
                      error->message);
          return EXIT_FAILURE;
        }

      /* Create stream using FDs provided to us */
      in_stream = g_unix_input_stream_new (read_fd, FALSE);
      out_stream = g_unix_output_stream_new (write_fd, FALSE);
      stream = g_simple_io_stream_new (in_stream, out_stream);

      /* Create connection using our private stream from the controller */
      if (!(connection = create_connection (stream, &error)))
        {
          g_printerr ("Failed to setup P2P D-Bus connection: %s\n",
                      error->message);
          return EXIT_FAILURE;
        }

      /* Now export our service at "/" (but don't start processing messages
       * until we start the profiler, further on.
       */
      service = g_object_new (IPC_TYPE_SYSPROF_IMPL, NULL);
      if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (service),
                                             connection, "/", &error))
        {
          g_printerr ("Failed to export service over D-Bus connection: %s",
                      error->message);
          return EXIT_FAILURE;
        }
    }

  /* Now start setting up our profiler */
  profiler = sysprof_local_profiler_new ();

  /* We might not even know our real subprocess in the case we are going
   * through another indirection layer like flatpak-spawn, so just assume
   * we're profiling the entire system as that will be necessary to include
   * the PID we really care about.
   */
  sysprof_profiler_set_whole_system (profiler, TRUE);

  /* If -- was ommitted or there are no commands, just profile the entire
   * system without spawning anything. Really only useful when testing the
   * agent without Builder.
   */
  if (sub_argc >= 0)
    {
      sysprof_profiler_set_spawn (profiler, TRUE);
      sysprof_profiler_set_spawn_inherit_environ (profiler, !clear_env);
      sysprof_profiler_set_spawn_argv (profiler, (const char * const *)sub_argv);
      sysprof_profiler_set_spawn_env (profiler, (const char * const *)env);

      if (directory != NULL)
        sysprof_profiler_set_spawn_cwd (profiler, directory);
    }

  /* Now open the writer for our session */
  if (!(writer = sysprof_capture_writer_new (capture_filename, BUFFER_SIZE)))
    {
      int errsv = errno;
      g_printerr ("Failed to open capture writer: %s\n",
                  g_strerror (errsv));
      return EXIT_FAILURE;
    }

  /* Attach writer to the profiler */
  sysprof_profiler_set_writer (profiler, writer);

  /* Add all request sources */
  add_source (profiler, TRUE, GBP_TYPE_SPAWN_SOURCE, NULL);
  add_source (profiler, TRUE, SYSPROF_TYPE_PROC_SOURCE, NULL);
  add_source (profiler, TRUE, SYSPROF_TYPE_SYMBOLS_SOURCE, NULL);
  add_source (profiler, aid_battery, SYSPROF_TYPE_BATTERY_SOURCE, NULL);
  add_source (profiler, aid_compositor, SYSPROF_TYPE_PROXY_SOURCE,
              "bus-type", G_BUS_TYPE_SESSION,
              "bus-name", "org.gnome.Shell",
              "object-path", "/org/gnome/Sysprof3/Profiler",
              NULL);
  add_source (profiler, aid_cpu, SYSPROF_TYPE_HOSTINFO_SOURCE, NULL);
  add_source (profiler, aid_disk, SYSPROF_TYPE_DISKSTAT_SOURCE, NULL);
  add_source (profiler, aid_energy, SYSPROF_TYPE_PROXY_SOURCE,
              "bus-type", G_BUS_TYPE_SYSTEM,
              "bus-name", "org.gnome.Sysprof3",
              "object-path", "/org/gnome/Sysprof3/RAPL",
              NULL);
  add_source (profiler, aid_gjs, SYSPROF_TYPE_GJS_SOURCE, NULL);
  add_source (profiler, aid_memory, SYSPROF_TYPE_MEMORY_SOURCE, NULL);
  add_source (profiler, aid_memprof, SYSPROF_TYPE_MEMPROF_SOURCE, NULL);
  add_source (profiler, aid_net, SYSPROF_TYPE_NETDEV_SOURCE, NULL);
  add_source (profiler, aid_perf, SYSPROF_TYPE_PERF_SOURCE, NULL);
  add_source (profiler, aid_tracefd, SYSPROF_TYPE_TRACEFD_SOURCE,
              "envvar", "SYSPROF_TRACE_FD",
              NULL);
  add_source (profiler, no_throttle, SYSPROF_TYPE_GOVERNOR_SOURCE,
              "disable-governor", TRUE,
              NULL);

  /* Bail when we've failed or finished and track the subprocess
   * so that we can deliver signals to it.
   */
  g_signal_connect (profiler,
                    "failed",
                    G_CALLBACK (profiler_failed_cb),
                    NULL);
  g_signal_connect (profiler,
                    "stopped",
                    G_CALLBACK (profiler_stopped_cb),
                    NULL);
  g_signal_connect (profiler,
                    "subprocess-spawned",
                    G_CALLBACK (subprocess_spawned_cb),
                    NULL);
  g_signal_connect (profiler,
                    "subprocess-finished",
                    G_CALLBACK (subprocess_finished_cb),
                    NULL);

  /* Start the profiler */
  sysprof_profiler_start (profiler);

  /* Now tell the connection to start processing messages that are
   * delivered from the controller, or signals destined back.
   */
  if (connection != NULL)
    g_dbus_connection_start_message_processing (connection);

  /* Wait for profiler to finish */
  g_main_loop_run (main_loop);

  /* Notify that some more work needs to proceed */
  message ("Extracting callgraph symbols");

  /* Let anything in-flight finish */
  main_context = g_main_loop_get_context (main_loop);
  while (g_main_context_pending (main_context))
    g_main_context_iteration (main_context, FALSE);

  /* Now make sure our bits are on disk */
  sysprof_capture_writer_flush (writer);

  /* Try to exit the same way as the subprocess did to propagate that
   * back into Builder who is watching *this* process.
   */
  if (subprocess_finished)
    {
      g_assert (G_IS_SUBPROCESS (subprocess));
      g_assert (g_subprocess_get_if_exited (subprocess) ||
                g_subprocess_get_if_signaled (subprocess));

      if (g_subprocess_get_if_signaled (subprocess))
        {
          int signum = g_subprocess_get_term_sig (subprocess);
          /* Try to exit in the same manner, or SIGKILL if that doesn't
           * work, or just EXIT_FAILURE as last resort.
           */
          raise (signum);
          raise (SIGKILL);
          return EXIT_FAILURE;
        }

      exit_code = g_subprocess_get_exit_status (subprocess);
    }

  return exit_code;
}
