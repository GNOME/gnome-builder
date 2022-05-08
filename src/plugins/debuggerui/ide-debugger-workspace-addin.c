/* ide-debugger-workspace-addin.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-debugger-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-code.h>
#include <libide-core.h>
#include <libide-debugger.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gtk.h>
#include <libide-gui.h>
#include <libide-io.h>

#include "ide-debugger-breakpoints-view.h"
#include "ide-debugger-controls.h"
#include "ide-debugger-disassembly-view.h"
#include "ide-debugger-workspace-addin.h"
#include "ide-debugger-libraries-view.h"
#include "ide-debugger-locals-view.h"
#include "ide-debugger-registers-view.h"
#include "ide-debugger-threads-view.h"
#include "ide-debugger-log-view.h"

/**
 * SECTION:ide-debugger-workspace-addin
 * @title: IdeDebuggerWorkspaceAddin
 * @short_description: Debugger hooks for the workspace perspective
 *
 * This class allows the debugger widgetry to hook into the workspace. We add
 * various panels to the workspace perpective and ensure they are only visible
 * when the process is being debugged.
 *
 * Since: 3.32
 */

struct _IdeDebuggerWorkspaceAddin
{
  GObject                     parent_instance;

  IdeSignalGroup             *debug_manager_signals;
  IdeSignalGroup             *debugger_signals;

  IdeWorkspace               *workspace;
  IdeWorkbench               *workbench;

  IdeRunManager              *run_manager;

  IdeDebuggerDisassemblyView *disassembly_view;
  IdeDebuggerControls        *controls;
  IdeDebuggerBreakpointsView *breakpoints_view;
  IdeDebuggerLibrariesView   *libraries_view;
  IdeDebuggerLocalsView      *locals_view;
  IdePane                    *panel;
  IdeDebuggerRegistersView   *registers_view;
  IdeDebuggerThreadsView     *threads_view;
  IdeDebuggerLogView         *log_view;
};

static void
debugger_stopped (IdeDebuggerWorkspaceAddin *self,
                  IdeDebuggerStopReason   reason,
                  IdeDebuggerBreakpoint  *breakpoint,
                  IdeDebugger            *debugger)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  if (breakpoint != NULL)
    ide_debugger_workspace_addin_navigate_to_breakpoint (self, breakpoint);

  IDE_EXIT;
}

static void
send_notification (IdeDebuggerWorkspaceAddin *self,
                   const gchar            *title,
                   const gchar            *body,
                   const gchar            *icon_name,
                   gboolean                urgent)
{
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GIcon) icon = NULL;
  IdeContext *context;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));

  context = ide_workbench_get_context (self->workbench);

  if (icon_name)
    icon = g_themed_icon_new (icon_name);

  notif = g_object_new (IDE_TYPE_NOTIFICATION,
                        "has-progress", FALSE,
                        "icon", icon,
                        "title", title,
                        "body", body,
                        "urgent", TRUE,
                        NULL);
  ide_notification_attach (notif, IDE_OBJECT (context));
  ide_notification_withdraw_in_seconds (notif, 30);
}

static void
debugger_run_handler (IdeRunManager *run_manager,
                      IdeRunner     *runner,
                      gpointer       user_data)
{
  IdeDebuggerWorkspaceAddin *self = user_data;
  IdeDebugManager *debug_manager;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));

  /*
   * Get the currently configured debugger and attach it to our runner.
   * It might need to prepend arguments like `gdb', `pdb', `mdb', etc.
   */
  context = ide_object_get_context (IDE_OBJECT (run_manager));
  debug_manager = ide_debug_manager_from_context (context);

  if (!ide_debug_manager_start (debug_manager, runner, &error))
    send_notification (self,
                       _("Failed to start the debugger"),
                       error->message,
                       "computer-fail-symbolic",
                       TRUE);

  IDE_EXIT;
}

static void
debug_manager_notify_debugger (IdeDebuggerWorkspaceAddin *self,
                               GParamSpec                *pspec,
                               IdeDebugManager           *debug_manager)
{
  IdeDebugger *debugger;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_DEBUG_MANAGER (debug_manager));

  panel_widget_raise (PANEL_WIDGET (self->panel));

  debugger = ide_debug_manager_get_debugger (debug_manager);
  gtk_widget_insert_action_group (GTK_WIDGET (self->workspace), "debugger", G_ACTION_GROUP (debugger));

  ide_debugger_breakpoints_view_set_debugger (self->breakpoints_view, debugger);
  ide_debugger_locals_view_set_debugger (self->locals_view, debugger);
  ide_debugger_libraries_view_set_debugger (self->libraries_view, debugger);
  ide_debugger_registers_view_set_debugger (self->registers_view, debugger);
  ide_debugger_threads_view_set_debugger (self->threads_view, debugger);
  ide_debugger_log_view_set_debugger (self->log_view, debugger);

  ide_signal_group_set_target (self->debugger_signals, debugger);
}

static void
debug_manager_notify_active (IdeDebuggerWorkspaceAddin *self,
                             GParamSpec             *pspec,
                             IdeDebugManager        *debug_manager)
{
  gboolean reveal_child = FALSE;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_DEBUG_MANAGER (debug_manager));

  /*
   * Instead of using a property binding, we use this signal callback so
   * that we can adjust the reveal-child and visible. Otherwise the widgets
   * will take up space+padding when reveal-child is FALSE.
   */

  if (ide_debug_manager_get_active (debug_manager))
    {
      gtk_widget_show (GTK_WIDGET (self->controls));
      reveal_child = TRUE;
    }

  ide_debugger_controls_set_reveal_child (self->controls, reveal_child);
}

static void
on_frame_activated (IdeDebuggerWorkspaceAddin *self,
                    IdeDebuggerThread      *thread,
                    IdeDebuggerFrame       *frame,
                    IdeDebuggerThreadsView *threads_view)
{
  IdeDebuggerAddress addr;
  const gchar *path;
  guint line;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_DEBUGGER_THREAD (thread));
  g_assert (IDE_IS_DEBUGGER_FRAME (frame));
  g_assert (IDE_IS_DEBUGGER_THREADS_VIEW (threads_view));

  ide_debugger_locals_view_load_async (self->locals_view, thread, frame, NULL, NULL, NULL);

  path = ide_debugger_frame_get_file (frame);
  line = ide_debugger_frame_get_line (frame);

  if (line > 0)
    line--;

  if (path != NULL)
    {
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (threads_view));
      g_autofree gchar *project_path = ide_context_build_filename (context, path, NULL);
      g_autoptr(GFile) file = g_file_new_for_path (project_path);
      g_autoptr(IdePanelPosition) position = ide_panel_position_new ();
      g_autoptr(IdeLocation) location = NULL;

      location = ide_location_new (file, line, -1);
      ide_editor_focus_location (self->workspace, position, location);

      IDE_EXIT;
    }

  addr = ide_debugger_frame_get_address (frame);

  if (addr != IDE_DEBUGGER_ADDRESS_INVALID)
    {
      ide_debugger_workspace_addin_navigate_to_address (self, addr);
      IDE_EXIT;
    }

  g_warning ("Failed to locate source or memory address for frame");

  IDE_EXIT;
}

static void
ide_debugger_workspace_addin_add_ui (IdeDebuggerWorkspaceAddin *self)
{
  g_autoptr(IdePanelPosition) position = NULL;
  IdeHeaderBar *header_bar;
  GtkNotebook *notebook;
  PanelPaned *hpaned;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (self->workspace != NULL);

  self->controls = g_object_new (IDE_TYPE_DEBUGGER_CONTROLS,
                                 "visible", FALSE,
                                 NULL);

  header_bar = ide_workspace_get_header_bar (self->workspace);
  ide_header_bar_add (header_bar,
                      IDE_HEADER_BAR_POSITION_RIGHT_OF_CENTER,
                      100,
                      GTK_WIDGET (self->controls));

  ide_pane_observe (g_object_new (IDE_TYPE_PANE,
                                  "title", _("Debugger"),
                                  "icon-name", "builder-debugger-symbolic",
                                  NULL),
                    (IdePane **)&self->panel);

  notebook = g_object_new (GTK_TYPE_NOTEBOOK, NULL);
  panel_widget_set_child (PANEL_WIDGET (self->panel), GTK_WIDGET (notebook));

  hpaned = g_object_new (PANEL_TYPE_PANED,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         NULL);
  gtk_notebook_append_page (notebook, GTK_WIDGET (hpaned), gtk_label_new (_("Threads")));

  self->threads_view = g_object_new (IDE_TYPE_DEBUGGER_THREADS_VIEW,
                                     "hexpand", TRUE,
                                     NULL);
  g_signal_connect_swapped (self->threads_view,
                            "frame-activated",
                            G_CALLBACK (on_frame_activated),
                            self);
  panel_paned_append (hpaned, GTK_WIDGET (self->threads_view));

  self->locals_view = g_object_new (IDE_TYPE_DEBUGGER_LOCALS_VIEW,
                                    "width-request", 250,
                                    NULL);
  panel_paned_append (hpaned, GTK_WIDGET (self->locals_view));

  self->breakpoints_view = g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS_VIEW, NULL);
  gtk_notebook_append_page (notebook,
                            GTK_WIDGET (self->breakpoints_view),
                            gtk_label_new (_("Breakpoints")));

  self->libraries_view = g_object_new (IDE_TYPE_DEBUGGER_LIBRARIES_VIEW, NULL);
  gtk_notebook_append_page (notebook,
                            GTK_WIDGET (self->libraries_view),
                            gtk_label_new (_("Libraries")));

  self->registers_view = g_object_new (IDE_TYPE_DEBUGGER_REGISTERS_VIEW, NULL);
  gtk_notebook_append_page (notebook,
                            GTK_WIDGET (self->registers_view),
                            gtk_label_new (_("Registers")));

  self->log_view = g_object_new (IDE_TYPE_DEBUGGER_LOG_VIEW, NULL);
  gtk_notebook_append_page (notebook,
                            GTK_WIDGET (self->log_view),
                            gtk_label_new (_("Console")));

  position = ide_panel_position_new ();
  ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_BOTTOM);

  ide_workspace_add_pane (self->workspace, IDE_PANE (self->panel), position);
}

static void
ide_debugger_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  IdeDebuggerWorkspaceAddin *self = (IdeDebuggerWorkspaceAddin *)addin;
  IdeDebugManager *debug_manager;
  IdeRunManager *run_manager;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  self->workspace = workspace;
  self->workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));

  if (!ide_workbench_has_project (self->workbench) || !IDE_IS_PRIMARY_WORKSPACE (workspace))
    return;

  context = ide_widget_get_context (GTK_WIDGET (workspace));
  run_manager = ide_run_manager_from_context (context);
  debug_manager = ide_debug_manager_from_context (context);

  self->run_manager = g_object_ref (run_manager);

  ide_debugger_workspace_addin_add_ui (self);

  ide_run_manager_add_handler (run_manager,
                               "debugger",
                               _("Run with Debugger"),
                               "builder-debugger-symbolic",
                               "F5",
                               debugger_run_handler,
                               g_object_ref (self),
                               g_object_unref);

  self->debugger_signals = ide_signal_group_new (IDE_TYPE_DEBUGGER);

  ide_signal_group_connect_swapped (self->debugger_signals,
                                    "log",
                                    G_CALLBACK (ide_debugger_log_view_debugger_log),
                                    self->log_view);

  ide_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (debugger_stopped),
                                    self);

  self->debug_manager_signals = ide_signal_group_new (IDE_TYPE_DEBUG_MANAGER);

  ide_signal_group_connect_swapped (self->debug_manager_signals,
                                    "notify::active",
                                    G_CALLBACK (debug_manager_notify_active),
                                    self);

  ide_signal_group_connect_swapped (self->debug_manager_signals,
                                    "notify::debugger",
                                    G_CALLBACK (debug_manager_notify_debugger),
                                    self);

  ide_signal_group_set_target (self->debug_manager_signals, debug_manager);

  IDE_EXIT;
}

static void
ide_debugger_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  IdeDebuggerWorkspaceAddin *self = (IdeDebuggerWorkspaceAddin *)addin;
  IdeHeaderBar *header_bar;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  if (!ide_workbench_has_project (self->workbench))
    return;

  gtk_widget_insert_action_group (GTK_WIDGET (self->workspace), "debugger", NULL);
  ide_run_manager_remove_handler (self->run_manager, "debugger");

  header_bar = ide_workspace_get_header_bar (workspace);
  ide_header_bar_remove (header_bar, GTK_WIDGET (self->controls));
  self->controls = NULL;

  g_clear_object (&self->debugger_signals);
  g_clear_object (&self->debug_manager_signals);
  g_clear_object (&self->run_manager);

  ide_clear_pane ((IdePane **)&self->panel);
  ide_clear_page ((IdePage **)&self->disassembly_view);

  self->workspace = NULL;
  self->workbench = NULL;

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = ide_debugger_workspace_addin_load;
  iface->unload = ide_debugger_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeDebuggerWorkspaceAddin, ide_debugger_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
ide_debugger_workspace_addin_class_init (IdeDebuggerWorkspaceAddinClass *klass)
{
}

static void
ide_debugger_workspace_addin_init (IdeDebuggerWorkspaceAddin *self)
{
}

void
ide_debugger_workspace_addin_navigate_to_file (IdeDebuggerWorkspaceAddin *self,
                                               GFile                     *file,
                                               guint                      line)
{
  g_autoptr(IdeLocation) location = NULL;

  g_return_if_fail (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_return_if_fail (G_IS_FILE (file));

  location = ide_location_new (file, line, -1);
  ide_editor_focus_location (self->workspace, NULL, location);
}

static void
ide_debugger_workspace_addin_disassemble_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(IdeDebuggerWorkspaceAddin) self = user_data;
  g_autoptr(GPtrArray) instructions = NULL;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));

  instructions = ide_debugger_disassemble_finish (debugger, result, &error);

  if (instructions == NULL)
    {
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  if (self->workspace == NULL)
    IDE_EXIT;

  if (self->disassembly_view == NULL)
    {
      g_autoptr(IdePanelPosition) position = ide_panel_position_new ();

      ide_page_observe (g_object_new (IDE_TYPE_DEBUGGER_DISASSEMBLY_VIEW, NULL),
                        (IdePage **)&self->disassembly_view);
      ide_workspace_add_page (self->workspace, IDE_PAGE (self->disassembly_view), position);
    }

  ide_debugger_disassembly_view_set_instructions (self->disassembly_view, instructions);

  /* TODO: Set current instruction */

  panel_widget_raise (PANEL_WIDGET (self->disassembly_view));

  IDE_EXIT;
}

void
ide_debugger_workspace_addin_navigate_to_address (IdeDebuggerWorkspaceAddin *self,
                                                  IdeDebuggerAddress         address)
{
  IdeDebugger *debugger;
  IdeDebuggerAddressRange range;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_return_if_fail (address != IDE_DEBUGGER_ADDRESS_INVALID);

  if (NULL == (debugger = ide_signal_group_get_target (self->debugger_signals)))
    IDE_EXIT;

  if (address < 0x10)
    range.from = 0;
  else
    range.from = address - 0x10;

  if (G_MAXUINT64 - 0x20 < address)
    range.to = G_MAXUINT64;
  else
    range.to = address + 0x20;

  ide_debugger_disassemble_async (debugger,
                                  &range,
                                  NULL,
                                  ide_debugger_workspace_addin_disassemble_cb,
                                  g_object_ref (self));

  IDE_EXIT;

}

void
ide_debugger_workspace_addin_navigate_to_breakpoint (IdeDebuggerWorkspaceAddin *self,
                                                     IdeDebuggerBreakpoint     *breakpoint)
{
  IdeDebuggerAddress address;
  const gchar *path;
  guint line;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_WORKSPACE_ADDIN (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  address = ide_debugger_breakpoint_get_address (breakpoint);
  path = ide_debugger_breakpoint_get_file (breakpoint);
  line = ide_debugger_breakpoint_get_line (breakpoint);

  if (line > 0)
    line--;

  if (path != NULL)
    {
      g_autoptr(GFile) file = g_file_new_for_path (path);
      ide_debugger_workspace_addin_navigate_to_file (self, file, line);
    }
  else if (address != IDE_DEBUGGER_ADDRESS_INVALID)
    {
      ide_debugger_workspace_addin_navigate_to_address (self, address);
    }

  IDE_EXIT;
}
