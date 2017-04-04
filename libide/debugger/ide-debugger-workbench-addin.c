/* ide-debugger-workbench-addin.c
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

#define G_LOG_DOMAIN "ide-debugger-workbench-addin"

#include <glib/gi18n.h>

#include <egg-signal-group.h>
#include <gtksourceview/gtksource.h>

#include "debugger/ide-breakpoint.h"
#include "debugger/ide-debug-manager.h"
#include "debugger/ide-debugger-controls.h"
#include "debugger/ide-debugger-perspective.h"
#include "debugger/ide-debugger-workbench-addin.h"
#include "workbench/ide-workbench-message.h"

struct _IdeDebuggerWorkbenchAddin
{
  GObject                 parent_instance;
  IdeWorkbench           *workbench;
  IdeDebuggerControls    *controls;
  IdeWorkbenchMessage    *message;
  IdeDebuggerPerspective *perspective;
  EggSignalGroup         *debug_manager_signals;
};

static void
debugger_run_handler (IdeRunManager *run_manager,
                      IdeRunner     *runner,
                      gpointer       user_data)
{
  IdeDebuggerWorkbenchAddin *self = user_data;
  IdeDebugManager *debug_manager;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (self));

  ide_workbench_set_visible_perspective_name (self->workbench, "debugger");

  /*
   * Get the currently configured debugger and attach it to our runner.
   * It might need to prepend arguments like `gdb', `pdb', `mdb', etc.
   */
  context = ide_object_get_context (IDE_OBJECT (run_manager));
  debug_manager = ide_context_get_debug_manager (context);

  if (!ide_debug_manager_start (debug_manager, runner, &error))
    {
      ide_workbench_message_set_subtitle (self->message, error->message);
      gtk_widget_show (GTK_WIDGET (self->message));
    }

  IDE_EXIT;
}

static void
debug_manager_notify_active (IdeDebuggerWorkbenchAddin *self,
                             GParamSpec                *pspec,
                             IdeDebugManager           *debug_manager)
{
  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_DEBUG_MANAGER (debug_manager));

  /*
   * Instead of using a property binding, we use this signal callback so
   * that we can adjust the reveal-child and visible. Otherwise the widgets
   * will take up space+padding when reveal-child is FALSE.
   */

  if (ide_debug_manager_get_active (debug_manager))
    {
      gtk_widget_show (GTK_WIDGET (self->controls));
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls), TRUE);
    }
  else
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls), FALSE);
    }
}

static void
controls_notify_child_revealed (IdeDebuggerWorkbenchAddin *self,
                                GParamSpec                *pspec,
                                IdeDebuggerControls       *controls)
{
  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_DEBUGGER_CONTROLS (controls));

  if (!gtk_revealer_get_child_revealed (GTK_REVEALER (controls)))
    gtk_widget_hide (GTK_WIDGET (controls));
}

static void
debug_manager_breakpoint_reached (IdeDebuggerWorkbenchAddin *self,
                                  IdeBreakpoint             *breakpoint,
                                  IdeDebugger               *debugger)
{
  g_autoptr(IdeUri) uri = NULL;
  GFile *file;
  guint line;

  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  if (NULL == (file = ide_breakpoint_get_file (breakpoint)) ||
      NULL == (uri = ide_uri_new_from_file (file)))
    return;

  if (0 != (line = ide_breakpoint_get_line (breakpoint)))
    {
      g_autofree gchar *fragment = g_strdup_printf ("L%u", line);
      ide_uri_set_fragment (uri, fragment);
    }

  ide_workbench_addin_open_async (IDE_WORKBENCH_ADDIN (self),
                                  uri,
                                  NULL,
                                  IDE_WORKBENCH_OPEN_FLAGS_NONE,
                                  NULL, NULL, NULL);
}

static void
ide_debugger_workbench_addin_load (IdeWorkbenchAddin *addin,
                                   IdeWorkbench      *workbench)
{
  IdeDebuggerWorkbenchAddin *self = (IdeDebuggerWorkbenchAddin *)addin;
  IdeWorkbenchHeaderBar *headerbar;
  IdeDebugManager *debug_manager;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  self->workbench = workbench;

  context = ide_workbench_get_context (workbench);
  debug_manager = ide_context_get_debug_manager (context);
  run_manager = ide_context_get_run_manager (context);

  gtk_widget_insert_action_group (GTK_WIDGET (workbench),
                                  "debugger",
                                  G_ACTION_GROUP (debug_manager));

  headerbar = ide_workbench_get_headerbar (workbench);

  self->debug_manager_signals = egg_signal_group_new (IDE_TYPE_DEBUG_MANAGER);

  egg_signal_group_connect_object (self->debug_manager_signals,
                                   "notify::active",
                                   G_CALLBACK (debug_manager_notify_active),
                                   self,
                                   G_CONNECT_SWAPPED);

  egg_signal_group_connect_object (self->debug_manager_signals,
                                   "breakpoint-reached",
                                   G_CALLBACK (debug_manager_breakpoint_reached),
                                   self,
                                   G_CONNECT_SWAPPED);

  self->controls = g_object_new (IDE_TYPE_DEBUGGER_CONTROLS,
                                 "transition-duration", 500,
                                 "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT,
                                 "reveal-child", FALSE,
                                 "visible", FALSE,
                                 NULL);
  g_signal_connect_object (self->controls,
                           "notify::child-revealed",
                           G_CALLBACK (controls_notify_child_revealed),
                           self,
                           G_CONNECT_SWAPPED);
  ide_workbench_header_bar_insert_left (headerbar,
                                        GTK_WIDGET (self->controls),
                                        GTK_PACK_START,
                                        100);

  ide_run_manager_add_handler (run_manager,
                               "debugger",
                               _("Run with Debugger"),
                               "builder-debugger-symbolic",
                               "F5",
                               debugger_run_handler,
                               g_object_ref (self),
                               g_object_unref);

  self->perspective = g_object_new (IDE_TYPE_DEBUGGER_PERSPECTIVE,
                                    "visible", TRUE,
                                    NULL);
  g_object_bind_property (debug_manager, "debugger",
                          self->perspective, "debugger",
                          G_BINDING_SYNC_CREATE);
  ide_workbench_add_perspective (workbench, IDE_PERSPECTIVE (self->perspective));

  self->message = g_object_new (IDE_TYPE_WORKBENCH_MESSAGE,
                                "id", "org.gnome.builder.debugger.failure",
                                "show-close-button", TRUE,
                                "title", _("Failed to initialize the debugger"),
                                NULL);
  ide_workbench_push_message (workbench, self->message);
}

static void
ide_debugger_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                     IdeWorkbench      *workbench)
{
  IdeDebuggerWorkbenchAddin *self = (IdeDebuggerWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (IDE_IS_WORKBENCH_ADDIN (addin));
  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  run_manager = ide_context_get_run_manager (context);

  g_clear_object (&self->debug_manager_signals);

  /* Remove the handler to initiate the debugger */
  ide_run_manager_remove_handler (run_manager, "debugger");

  /* Remove our debugger control widgets */
  g_clear_pointer (&self->controls, gtk_widget_destroy);

  /* Remove actions from activation */
  gtk_widget_insert_action_group (GTK_WIDGET (workbench), "debugger", NULL);

  /* Remove the debugging perspective from the UI */
  ide_workbench_remove_perspective (workbench, IDE_PERSPECTIVE (self->perspective));
  self->perspective = NULL;

  self->workbench = NULL;
}

static void
ide_debugger_workbench_addin_open_async (IdeWorkbenchAddin     *addin,
                                         IdeUri                *uri,
                                         const gchar           *content_type,
                                         IdeWorkbenchOpenFlags  flags,
                                         GCancellable          *cancellable,
                                         GAsyncReadyCallback    callback,
                                         gpointer               user_data)
{
  IdeDebuggerWorkbenchAddin *self = (IdeDebuggerWorkbenchAddin *)addin;
  g_autoptr(GtkSourceFileLoader) loader = NULL;
  g_autoptr(GtkSourceBuffer) buffer = NULL;
  g_autoptr(GtkSourceFile) sfile = NULL;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GFile) file = NULL;
  GtkSourceView *view;

  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (self));
  g_assert (uri != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_debugger_workbench_addin_open_async);

  file = ide_uri_to_file (uri);

  buffer = gtk_source_buffer_new (NULL);
  view = g_object_new (GTK_SOURCE_TYPE_VIEW,
                       "buffer", buffer,
                       "visible", TRUE,
                       NULL);

  sfile = gtk_source_file_new ();
  gtk_source_file_set_location (sfile, file);

  loader = gtk_source_file_loader_new (buffer, sfile);
  gtk_source_file_loader_load_async (loader,
                                     G_PRIORITY_LOW,
                                     cancellable,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
}

static gboolean
ide_debugger_workbench_addin_open_finish (IdeWorkbenchAddin  *addin,
                                          GAsyncResult       *result,
                                          GError            **error)
{
  g_assert (IDE_IS_DEBUGGER_WORKBENCH_ADDIN (addin));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = ide_debugger_workbench_addin_load;
  iface->unload = ide_debugger_workbench_addin_unload;
  iface->open_async = ide_debugger_workbench_addin_open_async;
  iface->open_finish = ide_debugger_workbench_addin_open_finish;
}

G_DEFINE_TYPE_WITH_CODE (IdeDebuggerWorkbenchAddin, ide_debugger_workbench_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
ide_debugger_workbench_addin_class_init (IdeDebuggerWorkbenchAddinClass *klass)
{
}

static void
ide_debugger_workbench_addin_init (IdeDebuggerWorkbenchAddin *self)
{
}
