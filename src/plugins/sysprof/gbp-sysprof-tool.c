/* gbp-sysprof-tool.c
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

#define G_LOG_DOMAIN "gbp-sysprof-tool"

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <unistd.h>

#include <libide-gui.h>

#include "ipc-sysprof.h"

#include "gbp-sysprof-tool.h"

struct _GbpSysprofTool
{
  IdeRunTool parent_instance;

  /* Temporary file subprocess captures into */
  char *capture_file;

  /* Handle to our subprocess */
  IdeSubprocess *subprocess;

  /* IOStream used to communicate w/ subprocess */
  GIOStream *io_stream;

  /* Encoded protocol on top of @io_stream */
  GDBusConnection *connection;

  /* IPC service on @connection */
  IpcAgent *sysprof;

  /* Notification for status */
  IdeNotification *notif;
};

G_DEFINE_FINAL_TYPE (GbpSysprofTool, gbp_sysprof_tool, IDE_TYPE_RUN_TOOL)

static const char *
gbp_sysprof_tool_get_capture_file (GbpSysprofTool *self)
{
  g_assert (GBP_IS_SYSPROF_TOOL (self));

  if (self->capture_file == NULL)
    {
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      g_autoptr(GDateTime) now = NULL;
      g_autoptr(GFile) file = NULL;
      g_autofree char *now_str = NULL;
      g_autofree char *initial_name = NULL;
      g_autofree char *filename = NULL;

      now = g_date_time_new_now_local ();
      now_str = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");
      initial_name = g_strdup_printf (_("System Capture from %s.syscap"), now_str);
      g_strdelimit (initial_name, G_DIR_SEPARATOR_S, '-');
      file = ide_context_build_file (context, initial_name);
      self->capture_file = g_file_get_path (file);
    }

  return self->capture_file;
}

static gboolean
gbp_sysprof_tool_handler (IdeRunContext       *run_context,
                          const char * const  *argv,
                          const char * const  *env,
                          const char          *cwd,
                          IdeUnixFDMap        *unix_fd_map,
                          gpointer             user_data,
                          GError             **error)
{
  GbpSysprofTool *self = user_data;
  g_autoptr(IdeSettings) settings = NULL;
  g_autoptr(GIOStream) io_stream = NULL;
  IdeContext *context;
  const char *capture_file;
  guint n_fds;
  int read_fd;
  int write_fd;

  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (argv != NULL);
  g_assert (env != NULL);
  g_assert (IDE_IS_UNIX_FD_MAP (unix_fd_map));
  g_assert (GBP_IS_SYSPROF_TOOL (self));

  context = ide_object_get_context (IDE_OBJECT (self));
  settings = ide_context_ref_settings (context, "org.gnome.builder.sysprof");

  /* Run sysprof-agent */
  ide_run_context_append_argv (run_context, "sysprof-agent");

  /* Pass along FDs after stderr to next process */
  n_fds = ide_unix_fd_map_get_length (unix_fd_map);
  for (guint i = 3; i < n_fds; i++)
    {
      int source_fd;
      int dest_fd;

      source_fd = ide_unix_fd_map_peek (unix_fd_map, i, &dest_fd);

      if (source_fd != -1)
        ide_run_context_append_formatted (run_context, "--forward-fd=%d", dest_fd);
    }

  /* Setup a read/write FD to use to control subprocess via D-Bus */
  read_fd = ide_unix_fd_map_get_max_dest_fd (unix_fd_map) + 1;
  write_fd = read_fd + 1;
  if (!(io_stream = ide_unix_fd_map_create_stream (unix_fd_map, read_fd, write_fd, error)))
    return FALSE;

  /* Use a GIOStream to communicate via p2p D-Bus to subprocess */
  ide_run_context_append_formatted (run_context, "--read-fd=%d", read_fd);
  ide_run_context_append_formatted (run_context, "--write-fd=%d", write_fd);

  /* Setup temporary file to write to */
  capture_file = gbp_sysprof_tool_get_capture_file (self);
  ide_run_context_append_formatted (run_context, "--capture=%s", capture_file);

  if (cwd != NULL)
    {
      ide_run_context_append_argv (run_context, "--directory");
      ide_run_context_append_argv (run_context, cwd);
    }

  ide_run_context_append_argv (run_context, "--decode");

  if (ide_settings_get_boolean (settings, "cpu-aid"))
    ide_run_context_append_argv (run_context, "--cpu");

  if (ide_settings_get_boolean (settings, "perf-aid"))
    ide_run_context_append_argv (run_context, "--perf");

  if (ide_settings_get_boolean (settings, "memory-aid"))
    ide_run_context_append_argv (run_context, "--memory");

  if (ide_settings_get_boolean (settings, "memprof-aid"))
    ide_run_context_append_argv (run_context, "--memprof");

  if (ide_settings_get_boolean (settings, "diskstat-aid"))
    ide_run_context_append_argv (run_context, "--disk");

  if (ide_settings_get_boolean (settings, "netdev-aid"))
    ide_run_context_append_argv (run_context, "--net");

  if (ide_settings_get_boolean (settings, "energy-aid"))
    ide_run_context_append_argv (run_context, "--energy");

  if (ide_settings_get_boolean (settings, "battery-aid"))
    ide_run_context_append_argv (run_context, "--battery");

  if (ide_settings_get_boolean (settings, "compositor-aid"))
    ide_run_context_append_argv (run_context, "--compositor");

  if (ide_settings_get_boolean (settings, "gjs-aid"))
    ide_run_context_append_argv (run_context, "--gjs");

  if (!ide_settings_get_boolean (settings, "allow-throttle"))
    ide_run_context_append_argv (run_context, "--no-throttle");

  if (ide_settings_get_boolean (settings, "allow-tracefd"))
    ide_run_context_append_argv (run_context, "--tracefd");

  if (ide_settings_get_boolean (settings, "session-bus"))
    ide_run_context_append_argv (run_context, "--session-bus");

  if (ide_settings_get_boolean (settings, "system-bus"))
    ide_run_context_append_argv (run_context, "--system-bus");

  if (ide_settings_get_boolean (settings, "scheduler-details"))
    ide_run_context_append_argv (run_context, "--scheduler");

  for (guint i = 0; env[i]; i++)
    ide_run_context_append_formatted (run_context, "--env=%s", env[i]);

  ide_run_context_append_argv (run_context, "--");
  ide_run_context_append_args (run_context, argv);

  g_set_object (&self->io_stream, io_stream);

  if (!ide_run_context_merge_unix_fd_map (run_context, unix_fd_map, error))
    return FALSE;

  return TRUE;
}

static void
gbp_sysprof_tool_prepare_to_run (IdeRunTool    *run_tool,
                                 IdePipeline   *pipeline,
                                 IdeRunCommand *run_command,
                                 IdeRunContext *run_context)
{
  GbpSysprofTool *self = (GbpSysprofTool *)run_tool;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (self));
  g_assert (IDE_IS_PIPELINE (pipeline));
  g_assert (IDE_IS_RUN_COMMAND (run_command));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));

  /* If we have sysprof-agent in the runtime, then use that since we get
   * a chance to make things like LD_PRELOAD work. Otherwise, fallback to
   * using our own wrapper in our context which is more restrictive.
   */
  if (ide_pipeline_contains_program_in_path (pipeline, "sysprof-agent", NULL))
    ide_run_context_push (run_context,
                          gbp_sysprof_tool_handler,
                          g_object_ref (run_tool),
                          g_object_unref);
  else
    /* Use our bundled version */
    ide_run_context_push_at_base (run_context,
                                  gbp_sysprof_tool_handler,
                                  g_object_ref (run_tool),
                                  g_object_unref);

  IDE_EXIT;
}

static void
gbp_sysprof_tool_force_exit (IdeRunTool *run_tool)
{
  GbpSysprofTool *self = (GbpSysprofTool *)run_tool;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (self));

  if (self->sysprof)
    ipc_agent_call_force_exit (self->sysprof, 0, -1, NULL, NULL, NULL);
  else if (self->subprocess)
    ide_subprocess_force_exit (self->subprocess);
  else
    g_warning ("Cannot force exit, no subprocess");

  IDE_EXIT;
}

static void
gbp_sysprof_tool_send_signal (IdeRunTool *run_tool,
                              int         signum)
{
  GbpSysprofTool *self = (GbpSysprofTool *)run_tool;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (self));

  if (self->sysprof)
    ipc_agent_call_send_signal (self->sysprof, signum, 0, -1, NULL, NULL, NULL);
  else if (self->subprocess)
    ide_subprocess_send_signal (self->subprocess, signum);
  else
    g_warning ("Cannot send signal %d, no subprocess", signum);

  IDE_EXIT;
}

static void
proxy_log_to_notif (IpcAgent        *sysprof,
                    const char      *message,
                    IdeNotification *notif)
{
  g_autofree char *title = NULL;

  g_assert (IPC_IS_AGENT (sysprof));
  g_assert (IDE_IS_NOTIFICATION (notif));

  if (ide_str_empty0 (message))
    return;

  title = g_strdup_printf ("Sysprof: %s", message);

  ide_notification_set_title (notif, title);
  ide_object_message (IDE_OBJECT (notif), "%s", title);
}

static void
gbp_sysprof_tool_started (IdeRunTool    *run_tool,
                          IdeSubprocess *subprocess)
{
  GbpSysprofTool *self = (GbpSysprofTool *)run_tool;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(IpcAgent) sysprof = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (self));
  g_assert (IDE_IS_SUBPROCESS (subprocess));

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  self->notif = ide_notification_new ();
  ide_notification_set_title (self->notif, _("Profiling Application…"));
  ide_notification_set_body (self->notif, _("Symbol decoding will begin after application exits"));
  ide_notification_set_icon_name (self->notif, "builder-profiler-symbolic");
  ide_notification_set_urgent (self->notif, TRUE);
  ide_notification_attach (self->notif, IDE_OBJECT (self));

  g_set_object (&self->subprocess, subprocess);

  if (self->io_stream == NULL)
    {
      g_warning ("No stream to communicate with subprocess, control unavailable");
      IDE_EXIT;
    }

  connection = g_dbus_connection_new_sync (self->io_stream, NULL,
                                           G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
                                           NULL, NULL, &error);

  if (connection == NULL)
    {
      g_warning ("Failed to create GDBusConncetion, cantrol unavailable: %s",
                 error->message);
      IDE_EXIT;
    }

  sysprof = ipc_agent_proxy_new_sync (connection,
                                      G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                      NULL, "/", NULL, &error);

  if (sysprof == NULL)
    {
      g_warning ("Failed to create GDBusProxy, cantrol unavailable: %s",
                 error->message);
      IDE_EXIT;
    }

  g_signal_connect_object (sysprof,
                           "log",
                           G_CALLBACK (proxy_log_to_notif),
                           self->notif,
                           0);

  g_debug ("Control proxy to subprocess created");
  g_set_object (&self->sysprof, sysprof);
  g_set_object (&self->connection, connection);

  /* We can have long running operations, so set no timeout */
  g_dbus_proxy_set_default_timeout (G_DBUS_PROXY (sysprof), G_MAXINT);

  /* Now start processing messages */
  g_dbus_connection_set_exit_on_close (connection, FALSE);

  /* Now we can start processing things */
  g_debug ("Starting to process peer messages");
  g_dbus_connection_start_message_processing (connection);

  IDE_EXIT;
}

static void
gbp_sysprof_tool_stopped (IdeRunTool *run_tool)
{
  GbpSysprofTool *self = (GbpSysprofTool *)run_tool;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_SYSPROF_TOOL (run_tool));

  if (self->capture_file != NULL)
    {
      g_autoptr(GFile) file = g_file_new_for_path (self->capture_file);
      IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
      IdeWorkbench *workbench = ide_workbench_from_context (context);

      ide_workbench_open_async (workbench,
                                file,
                                "open-with-external",
                                IDE_BUFFER_OPEN_FLAGS_NONE,
                                NULL,
                                NULL, NULL, NULL);

      g_clear_pointer (&self->capture_file, g_free);
    }

  g_clear_object (&self->subprocess);
  g_clear_object (&self->sysprof);
  g_clear_object (&self->connection);
  g_clear_object (&self->io_stream);

  if (self->notif)
    {
      ide_notification_withdraw (self->notif);
      g_clear_object (&self->notif);
    }

  IDE_EXIT;
}

static void
gbp_sysprof_tool_destroy (IdeObject *object)
{
  GbpSysprofTool *self = (GbpSysprofTool *)object;

  g_clear_object (&self->sysprof);
  g_clear_object (&self->connection);
  g_clear_object (&self->io_stream);
  g_clear_object (&self->subprocess);

  g_clear_pointer (&self->capture_file, g_free);

  IDE_OBJECT_CLASS (gbp_sysprof_tool_parent_class)->destroy (object);
}

static void
gbp_sysprof_tool_class_init (GbpSysprofToolClass *klass)
{
  IdeObjectClass *i_object_class = IDE_OBJECT_CLASS (klass);
  IdeRunToolClass *run_tool_class = IDE_RUN_TOOL_CLASS (klass);

  i_object_class->destroy = gbp_sysprof_tool_destroy;

  run_tool_class->prepare_to_run = gbp_sysprof_tool_prepare_to_run;
  run_tool_class->send_signal = gbp_sysprof_tool_send_signal;
  run_tool_class->force_exit = gbp_sysprof_tool_force_exit;
  run_tool_class->started = gbp_sysprof_tool_started;
  run_tool_class->stopped = gbp_sysprof_tool_stopped;
}

static void
gbp_sysprof_tool_init (GbpSysprofTool *self)
{
  ide_run_tool_set_icon_name (IDE_RUN_TOOL (self), "builder-profiler-symbolic");
}
