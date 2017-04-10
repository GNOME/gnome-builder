/* ide-debugger.c
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

#define G_LOG_DOMAIN "ide-debugger"

#include <glib/gi18n.h>

#include "ide-enums.h"
#include "ide-debug.h"

#include "debugger/ide-breakpoint.h"
#include "debugger/ide-debugger.h"
#include "runner/ide-runner.h"

G_DEFINE_INTERFACE (IdeDebugger, ide_debugger, IDE_TYPE_OBJECT)

enum {
  LOG,
  STOPPED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

gchar *
ide_debugger_real_get_name (IdeDebugger *self)
{
  return g_strdup (G_OBJECT_TYPE_NAME (self));
}

static gboolean
ide_debugger_real_supports_runner (IdeDebugger *self,
                                   IdeRunner   *runner,
                                   gint        *priority)
{
  return FALSE;
}

static void
ide_debugger_real_load_source_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  GtkSourceFileLoader *loader = (GtkSourceFileLoader *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GtkSourceBuffer *buffer;
  IdeBreakpoint *breakpoint;
  GtkTextIter iter;
  guint line;
  guint line_offset;

  IDE_ENTRY;

  g_assert (GTK_SOURCE_IS_FILE_LOADER (loader));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!gtk_source_file_loader_load_finish (loader, result, &error))
    {
      IDE_TRACE_MSG ("Failed to load buffer: %s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  buffer = gtk_source_file_loader_get_buffer (loader);
  g_assert (GTK_SOURCE_IS_BUFFER (buffer));

  breakpoint = g_task_get_task_data (task);
  g_assert (IDE_IS_BREAKPOINT (breakpoint));

  line = ide_breakpoint_get_line (breakpoint);
  line_offset = ide_breakpoint_get_line_offset (breakpoint);

  gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer),
                                           &iter,
                                           line,
                                           line_offset);

  gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

  g_task_return_pointer (task, g_object_ref (buffer), g_object_unref);

  IDE_EXIT;
}

static void
ide_debugger_real_load_source_async (IdeDebugger         *self,
                                     IdeBreakpoint       *breakpoint,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  g_autoptr(GtkSourceFileLoader) loader = NULL;
  g_autoptr(GtkSourceBuffer) buffer = NULL;
  g_autoptr(GtkSourceFile) source_file = NULL;
  GFile *file;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (IDE_IS_BREAKPOINT (breakpoint));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_object_ref (breakpoint), g_object_unref);
  g_task_set_source_tag (task, ide_debugger_real_load_source_async);

  file = ide_breakpoint_get_file (breakpoint);

  if (file == NULL)
    {
      const gchar *address = ide_breakpoint_get_address (breakpoint);

      if (address != NULL)
        g_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 /* translators: %s will be replaced with the address */
                                 _("Cannot locate source for address “%s”"),
                                 address);
      else
        g_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_NOT_SUPPORTED,
                                 _("Failed to locate source for breakpoint"));

      IDE_EXIT;
    }

  source_file = gtk_source_file_new ();
  gtk_source_file_set_location (source_file, file);
  buffer = gtk_source_buffer_new (NULL);
  loader = gtk_source_file_loader_new (buffer, source_file);

  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_LOW,
                                     NULL, NULL, NULL, NULL,
                                     ide_debugger_real_load_source_cb,
                                     g_steal_pointer (&task));

  IDE_EXIT;
}

static GtkSourceBuffer *
ide_debugger_real_load_source_finish (IdeDebugger   *self,
                                      GAsyncResult  *result,
                                      GError       **error)
{
  GtkSourceBuffer *ret;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (self));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_pointer (G_TASK (result), error);
  g_assert (!ret || GTK_SOURCE_IS_BUFFER (ret));

  IDE_RETURN (ret);
}

static void
ide_debugger_default_init (IdeDebuggerInterface *iface)
{
  iface->get_name = ide_debugger_real_get_name;
  iface->supports_runner = ide_debugger_real_supports_runner;
  iface->load_source_async = ide_debugger_real_load_source_async;
  iface->load_source_finish = ide_debugger_real_load_source_finish;

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-step-in",
                                                             "Can Step In",
                                                             "If we can advance the debugger, stepping into any function call in the line",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-step-over",
                                                             "Can Step Over",
                                                             "If we can advance the debugger, stepping over any function calls in the line",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("can-continue",
                                                             "Can Continue",
                                                             "If we can advance the debugger to the next breakpoint",
                                                             FALSE,
                                                             (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

  /**
   * IdeDebugger:log:
   * @self: A #IdeDebugger
   * @message: the log message
   *
   * The "log" signal is emitted when the debugger has informative information
   * to display to the user.
   */
  signals [LOG] =
    g_signal_new ("log",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerInterface, log),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * IdeDebugger::stopped:
   * @self: An #IdeDebugger
   * @reason: An #IdeDebuggerStopReason for why the stop occurred
   * @breakpoint: An #IdeBreakpoint
   *
   * The "stopped" signal should be emitted when the debugger has stopped at a
   * new location. @reason indicates the reson for the stop, and @breakpoint
   * describes the current location within the appliation.
   */
  signals [STOPPED] =
    g_signal_new ("stopped",
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (IdeDebuggerInterface, stopped),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, IDE_TYPE_DEBUGGER_STOP_REASON, IDE_TYPE_BREAKPOINT);
}

/**
 * ide_debugger_supports_runner:
 * @self: An #IdeDebugger
 * @runner: An #IdeRunner
 * @priority: (out): A location to set the priority
 *
 * This function checks to see if the debugger supports the runner. This
 * allows the debugger to verify the program type or other necessary
 * dependency information.
 *
 * Returns: %TRUE if the @self supports @runner.
 */
gboolean
ide_debugger_supports_runner (IdeDebugger *self,
                              IdeRunner   *runner,
                              gint        *priority)
{
  gboolean ret;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), FALSE);
  g_return_val_if_fail (IDE_IS_RUNNER (runner), FALSE);

  if (priority != NULL)
    *priority = G_MAXINT;

  ret = IDE_DEBUGGER_GET_IFACE (self)->supports_runner (self, runner, priority);

  IDE_TRACE_MSG ("Chceking if %s supports runner %s",
                 G_OBJECT_TYPE_NAME (self),
                 G_OBJECT_TYPE_NAME (runner));

  return ret;
}

/**
 * ide_debugger_get_name:
 * @self: A #IdeDebugger
 *
 * Gets the proper name of the debugger to display to the user.
 *
 * Returns: (transfer full): the display name for the debugger.
 */
gchar *
ide_debugger_get_name (IdeDebugger *self)
{
  gchar *ret = NULL;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);

  ret = IDE_DEBUGGER_GET_IFACE (self)->get_name (self);

  if (ret == NULL)
    ret = g_strdup (G_OBJECT_TYPE_NAME (self));

  return ret;
}

/**
 * ide_debugger_emit_stopped:
 * @self: A #IdeDebugger
 * @reason: the reason for the stop
 * @breakpoint: (nullable): an optional breakpoint that is reached
 *
 * This signal should be emitted when the debugger has stopped executing.
 *
 * If the stop is not related to the application exiting for any reason, then
 * you should provide a @breakpoint describing the stop location. That
 * breakpoint may be transient (see #IdeDebugger:transient) meaning that it is
 * only created for this stop event.
 */
void
ide_debugger_emit_stopped (IdeDebugger           *self,
                           IdeDebuggerStopReason  reason,
                           IdeBreakpoint         *breakpoint)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (!breakpoint || IDE_IS_BREAKPOINT (breakpoint));

#ifdef IDE_ENABLE_TRACE
  if (breakpoint != NULL)
    {
      g_autofree gchar *uri = NULL;
      const gchar *address;
      GFile *file;

      address = ide_breakpoint_get_address (breakpoint);
      file = ide_breakpoint_get_file (breakpoint);

      if (file != NULL)
        uri = g_file_get_uri (file);

      IDE_TRACE_MSG ("Stopped at breakpoint \"%s\" with address \"%s\"",
                     uri ? uri : "<none>",
                     address ? address : "<none>");
    }
#endif

  g_signal_emit (self, signals [STOPPED], 0, reason, breakpoint);
}

void
ide_debugger_prepare (IdeDebugger *self,
                      IdeRunner   *runner)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_RUNNER (runner));

  if (IDE_DEBUGGER_GET_IFACE (self)->prepare)
    IDE_DEBUGGER_GET_IFACE (self)->prepare (self, runner);
}

void
ide_debugger_run (IdeDebugger        *self,
                  IdeDebuggerRunType  run_type)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  if (IDE_DEBUGGER_GET_IFACE (self)->run)
    IDE_DEBUGGER_GET_IFACE (self)->run (self, run_type);
}

void
ide_debugger_emit_log (IdeDebugger *self,
                       const gchar *message)
{
  g_return_if_fail (IDE_IS_DEBUGGER (self));

  if (message != NULL)
    g_signal_emit (self, signals [LOG], 0, message);
}

void
ide_debugger_load_source_async (IdeDebugger         *self,
                                IdeBreakpoint       *breakpoint,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER (self));
  g_return_if_fail (IDE_IS_BREAKPOINT (breakpoint));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_DEBUGGER_GET_IFACE (self)->load_source_async (self, breakpoint, cancellable, callback, user_data);

  IDE_EXIT;
}

/**
 * ide_debugger_load_source_finish:
 * @self: a #IdeDebugger
 * @result: a #GAsyncResult
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to ide_debugger_load_source_async().
 *
 * Returns: (nullable) (transfer full): A #GtkSourceBuffer or %NULL
 */
GtkSourceBuffer *
ide_debugger_load_source_finish (IdeDebugger   *self,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  GtkSourceBuffer *ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_DEBUGGER (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  ret = IDE_DEBUGGER_GET_IFACE (self)->load_source_finish (self, result, error);

  g_return_val_if_fail (!ret || GTK_SOURCE_IS_BUFFER (ret), NULL);

  IDE_RETURN (ret);
}
