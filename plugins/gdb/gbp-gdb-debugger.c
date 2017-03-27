/* gbp-gdb-debugger.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-gdb-plugin"

#include <glib/gi18n.h>
#include <egg-signal-group.h>
#include <mi2-glib.h>

#include "gbp-gdb-debugger.h"

struct _GbpGdbDebugger
{
  IdeObject       parent;

  Mi2Client      *client;
  EggSignalGroup *client_signals;

  IdeRunner      *runner;
  EggSignalGroup *runner_signals;

  gint            mapped_fd;

  guint           can_step_in : 1;
  guint           can_step_over : 1;
  guint           can_continue : 1;
};

enum {
  PROP_0,
  PROP_CAN_STEP_IN,
  PROP_CAN_STEP_OVER,
  PROP_CAN_CONTINUE,
  N_PROPS
};

/* Globals */
static GParamSpec *properties [N_PROPS];

/* Forward Declarations */
static void      debugger_iface_init                            (IdeDebuggerInterface *iface);
static void      gbp_gdb_debugger_finalize                      (GObject              *object);
static gchar    *gbp_gdb_debugger_get_name                      (IdeDebugger          *debugger);
static void      gbp_gdb_debugger_notify_properties             (GbpGdbDebugger       *self);
static void      gbp_gdb_debugger_prepare                       (IdeDebugger          *debugger,
                                                                 IdeRunner            *runner);
static void      gbp_gdb_debugger_run                           (IdeDebugger          *debugger,
                                                                 IdeDebuggerRunType    run_type);
static gboolean  gbp_gdb_debugger_supports_runner               (IdeDebugger          *debugger,
                                                                 IdeRunner            *runner,
                                                                 gint                 *priority);
static void      gbp_gdb_debugger_on_runner_spawned             (GbpGdbDebugger       *self,
                                                                 const gchar          *identifier,
                                                                 IdeRunner            *runner);
static void      gbp_gdb_debugger_on_runner_exited              (GbpGdbDebugger       *self,
                                                                 IdeRunner            *runner);
static void      gbp_gdb_debugger_on_client_breakpoint_inserted (GbpGdbDebugger       *self,
                                                                 Mi2Breakpoint        *breakpoint,
                                                                 Mi2Client            *client);
static void      gbp_gdb_debugger_on_client_breakpoint_removed  (GbpGdbDebugger       *self,
                                                                 gint                  breakpoint_id,
                                                                 Mi2Client            *client);
static void      gbp_gdb_debugger_on_client_event               (GbpGdbDebugger       *self,
                                                                 Mi2EventMessage      *message,
                                                                 Mi2Client            *client);
static void      gbp_gdb_debugger_on_client_stopped             (GbpGdbDebugger       *self,
                                                                 Mi2StopReason         reason,
                                                                 Mi2EventMessage      *message,
                                                                 Mi2Client            *client);
static void      gbp_gdb_debugger_on_client_log                 (GbpGdbDebugger       *self,
                                                                 const gchar          *message,
                                                                 Mi2Client            *client);

/* Type initialization */
G_DEFINE_TYPE_WITH_CODE (GbpGdbDebugger, gbp_gdb_debugger, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_DEBUGGER, debugger_iface_init))

static void
gbp_gdb_debugger_notify_properties (GbpGdbDebugger *self)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_CONTINUE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_STEP_IN]);
  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CAN_STEP_OVER]);
}

static gchar *
gbp_gdb_debugger_get_name (IdeDebugger *debugger)
{
  g_assert (GBP_IS_GDB_DEBUGGER (debugger));

  return g_strdup (_("GNU Debugger"));
}

static gboolean
gbp_gdb_debugger_supports_runner (IdeDebugger *debugger,
                                  IdeRunner   *runner,
                                  gint        *priority)
{
  IdeRuntime *runtime;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (priority != NULL);

  runtime = ide_runner_get_runtime (runner);

  if (ide_runtime_contains_program_in_path (runtime, "gdb", NULL))
    {
      *priority = G_MAXINT;
      return TRUE;
    }

  return FALSE;
}

static void
gbp_gdb_debugger_prepare (IdeDebugger *debugger,
                          IdeRunner   *runner)
{
  static gchar *prepend_argv[] = { "gdb", "--interpreter", "mi2", "--args" };
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;
  int tty_fd;

  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  /* Prepend arguments in reverse to preserve ordering */
  for (guint i = G_N_ELEMENTS (prepend_argv); i > 0; i--)
    ide_runner_prepend_argv (runner, prepend_argv[i-1]);

  /* Connect to all our important signals */
  egg_signal_group_set_target (self->runner_signals, runner);

  /*
   * We steal and remap the PTY fd into the process so that gdb does not get
   * the controlling terminal, but instead allow us to ask gdb to setup the
   * inferrior with that same PTY.
   */
  if (-1 != (tty_fd = ide_runner_steal_tty (runner)))
    self->mapped_fd = ide_runner_take_fd (runner, tty_fd, -1);

  /* We need access to stdin/stdout for communicating with gdb */
  ide_runner_set_flags (runner, G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_run (IdeDebugger        *debugger,
                      IdeDebuggerRunType  run_type)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)debugger;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (self));

  self->can_continue = FALSE;
  self->can_step_in = FALSE;
  self->can_step_over = FALSE;

  gbp_gdb_debugger_notify_properties (self);

  switch (run_type)
    {
    case IDE_DEBUGGER_RUN_STEP_IN:
      break;

    case IDE_DEBUGGER_RUN_STEP_OVER:
      break;

    case IDE_DEBUGGER_RUN_CONTINUE:
      mi2_client_continue_async (self->client, FALSE, NULL, NULL, NULL);
      break;

    default:
      g_assert_not_reached ();
    }

  IDE_EXIT;
}

static void
debugger_iface_init (IdeDebuggerInterface *iface)
{
  iface->get_name        = gbp_gdb_debugger_get_name;
  iface->prepare         = gbp_gdb_debugger_prepare;
  iface->run             = gbp_gdb_debugger_run;
  iface->supports_runner = gbp_gdb_debugger_supports_runner;
}

static void
gbp_gdb_debugger_client_listen_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  Mi2Client *client = (Mi2Client *)object;
  g_autoptr(GbpGdbDebugger) self = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (MI2_IS_CLIENT (client));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (GBP_IS_GDB_DEBUGGER (self));

  if (!mi2_client_listen_finish (client, result, &error))
    g_warning ("%s", error->message);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_runner_spawned (GbpGdbDebugger *self,
                                    const gchar    *identifier,
                                    IdeRunner      *runner)
{
  g_autoptr(GIOStream) io_stream = NULL;
  g_autofree gchar *inferior_command = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (self->client == NULL);
  g_assert (identifier != NULL);
  g_assert (IDE_IS_RUNNER (runner));

  /* Create an IOStream to track pipe communication with gdb */
  io_stream = g_simple_io_stream_new (ide_runner_get_stdout (runner),
                                      ide_runner_get_stdin (runner));

  /* Setup our mi2 client to RPC to gdb */
  self->client = mi2_client_new (io_stream);

  /* Connect to all our signals up front necessary to control gdb */
  egg_signal_group_set_target (self->client_signals, self->client);

  /* Now ask the mi2 client to start procesing data */
  mi2_client_listen_async (self->client,
                           NULL,
                           gbp_gdb_debugger_client_listen_cb,
                           g_object_ref (self));

  /* Ask gdb to use our mapped in FD for the TTY when spawning the child */
  inferior_command = g_strdup_printf ("-gdb-set inferior-tty /proc/self/fd/%d", self->mapped_fd);
  mi2_client_exec_async (self->client, inferior_command, NULL, NULL, NULL);

  /* Now ask gdb to start running the program */
  mi2_client_run_async (self->client, NULL, NULL, NULL);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_runner_exited (GbpGdbDebugger *self,
                                   IdeRunner      *runner)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (IDE_IS_RUNNER (runner));

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_client_breakpoint_inserted (GbpGdbDebugger *self,
                                                Mi2Breakpoint  *breakpoint,
                                                Mi2Client      *client)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (MI2_IS_BREAKPOINT (breakpoint));
  g_assert (MI2_IS_CLIENT (client));

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_client_breakpoint_removed (GbpGdbDebugger *self,
                                               gint            breakpoint_id,
                                               Mi2Client      *client)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (breakpoint_id > 0);
  g_assert (MI2_IS_CLIENT (client));

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_client_event (GbpGdbDebugger  *self,
                                  Mi2EventMessage *message,
                                  Mi2Client       *client)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (MI2_IS_EVENT_MESSAGE (message));
  g_assert (MI2_IS_CLIENT (client));

}

static void
gbp_gdb_debugger_on_client_stopped (GbpGdbDebugger  *self,
                                    Mi2StopReason    reason,
                                    Mi2EventMessage *message,
                                    Mi2Client       *client)
{
  IDE_ENTRY;

  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (MI2_IS_EVENT_MESSAGE (message));
  g_assert (MI2_IS_CLIENT (client));

  switch (reason)
    {
    case MI2_STOP_BREAKPOINT_HIT:
      self->can_continue = TRUE;
      self->can_step_in = TRUE;
      self->can_step_over = TRUE;
      break;

    case MI2_STOP_EXITED_NORMALLY:
    case MI2_STOP_UNKNOWN:
    default:
      self->can_continue = FALSE;
      self->can_step_in = FALSE;
      self->can_step_over = FALSE;
      break;
    }

  gbp_gdb_debugger_notify_properties (self);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_on_client_log (GbpGdbDebugger *self,
                                const gchar    *message,
                                Mi2Client      *client)
{
  g_assert (GBP_IS_GDB_DEBUGGER (self));
  g_assert (message != NULL);
  g_assert (MI2_IS_CLIENT (client));

  ide_debugger_emit_log (IDE_DEBUGGER (self), message);
}

static void
gbp_gdb_debugger_finalize (GObject *object)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;

  IDE_ENTRY;

  g_clear_object (&self->client_signals);
  g_clear_object (&self->client);
  g_clear_object (&self->runner_signals);
  g_clear_object (&self->runner);

  G_OBJECT_CLASS (gbp_gdb_debugger_parent_class)->finalize (object);

  IDE_EXIT;
}

static void
gbp_gdb_debugger_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GbpGdbDebugger *self = (GbpGdbDebugger *)object;

  switch (prop_id)
    {
    case PROP_CAN_STEP_IN:
      g_value_set_boolean (value, self->can_step_in);
      break;

    case PROP_CAN_STEP_OVER:
      g_value_set_boolean (value, self->can_step_over);
      break;

    case PROP_CAN_CONTINUE:
      g_value_set_boolean (value, self->can_continue);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gbp_gdb_debugger_class_init (GbpGdbDebuggerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_gdb_debugger_finalize;
  object_class->get_property = gbp_gdb_debugger_get_property;

  properties [PROP_CAN_STEP_IN] =
    g_param_spec_boolean ("can-step-in", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties [PROP_CAN_STEP_OVER] =
    g_param_spec_boolean ("can-step-over", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties [PROP_CAN_CONTINUE] =
    g_param_spec_boolean ("can-continue", NULL, NULL,
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gbp_gdb_debugger_init (GbpGdbDebugger *self)
{
  IDE_ENTRY;

  self->runner_signals = egg_signal_group_new (IDE_TYPE_RUNNER);

  egg_signal_group_connect_object (self->runner_signals,
                                   "spawned",
                                   G_CALLBACK (gbp_gdb_debugger_on_runner_spawned),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->runner_signals,
                                   "exited",
                                   G_CALLBACK (gbp_gdb_debugger_on_runner_exited),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->client_signals = egg_signal_group_new (MI2_TYPE_CLIENT);

  egg_signal_group_connect_object (self->client_signals,
                                   "breakpoint-inserted",
                                   G_CALLBACK (gbp_gdb_debugger_on_client_breakpoint_inserted),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->client_signals,
                                   "breakpoint-removed",
                                   G_CALLBACK (gbp_gdb_debugger_on_client_breakpoint_removed),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->client_signals,
                                   "event",
                                   G_CALLBACK (gbp_gdb_debugger_on_client_event),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->client_signals,
                                   "stopped",
                                   G_CALLBACK (gbp_gdb_debugger_on_client_stopped),
                                   self,
                                   G_CONNECT_SWAPPED);
  egg_signal_group_connect_object (self->client_signals,
                                   "log",
                                   G_CALLBACK (gbp_gdb_debugger_on_client_log),
                                   self,
                                   G_CONNECT_SWAPPED);

  IDE_EXIT;
}
