/* ide-debugger-editor-addin.c
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

#define G_LOG_DOMAIN "ide-debugger-editor-addin"

#include "config.h"

#include <libide-code.h>
#include <libide-core.h>
#include <libide-debugger.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-gui.h>
#include <libide-io.h>
#include <libide-terminal.h>
#include <glib/gi18n.h>

#include "ide-debugger-breakpoints-view.h"
#include "ide-debugger-controls.h"
#include "ide-debugger-disassembly-view.h"
#include "ide-debugger-editor-addin.h"
#include "ide-debugger-libraries-view.h"
#include "ide-debugger-locals-view.h"
#include "ide-debugger-registers-view.h"
#include "ide-debugger-threads-view.h"

/**
 * SECTION:ide-debugger-editor-addin
 * @title: IdeDebuggerEditorAddin
 * @short_description: Debugger hooks for the editor perspective
 *
 * This class allows the debugger widgetry to hook into the editor. We add
 * various panels to the editor perpective and ensure they are only visible
 * when the process is being debugged.
 *
 * Since: 3.32
 */

struct _IdeDebuggerEditorAddin
{
  GObject                     parent_instance;

  DzlSignalGroup             *debug_manager_signals;
  DzlSignalGroup             *debugger_signals;

  IdeEditorSurface           *editor;
  IdeWorkbench               *workbench;

  IdeDebuggerDisassemblyView *disassembly_view;
  IdeDebuggerControls        *controls;
  IdeDebuggerBreakpointsView *breakpoints_view;
  IdeDebuggerLibrariesView   *libraries_view;
  IdeDebuggerLocalsView      *locals_view;
  DzlDockWidget              *panel;
  IdeDebuggerRegistersView   *registers_view;
  IdeDebuggerThreadsView     *threads_view;
  IdeTerminal                *log_view;
  GtkScrollbar               *log_view_scroller;
};

static void
debugger_log (IdeDebuggerEditorAddin *self,
              IdeDebuggerStream       stream,
              GBytes                 *content,
              IdeDebugger            *debugger)
{
  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_DEBUGGER_STREAM (stream));
  g_assert (content != NULL);
  g_assert (IDE_IS_DEBUGGER (debugger));

  if (stream == IDE_DEBUGGER_CONSOLE)
    {
      IdeLineReader reader;
      const gchar *str;
      gchar *line;
      gsize len;
      gsize line_len;

      str = g_bytes_get_data (content, &len);

      /*
       * Ingnore \n so we can add \r\n. Otherwise we get problematic
       * output in the terminal.
       */
      ide_line_reader_init (&reader, (gchar *)str, len);
      while (NULL != (line = ide_line_reader_next (&reader, &line_len)))
        {
          vte_terminal_feed (VTE_TERMINAL (self->log_view), line, line_len);

          if ((line + line_len) < (str + len))
            {
              if (line[line_len] == '\r' || line[line_len] == '\n')
                vte_terminal_feed (VTE_TERMINAL (self->log_view), "\r\n", 2);
            }
        }
    }
}

static void
debugger_stopped (IdeDebuggerEditorAddin *self,
                  IdeDebuggerStopReason   reason,
                  IdeDebuggerBreakpoint  *breakpoint,
                  IdeDebugger            *debugger)
{
  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_DEBUGGER_STOP_REASON (reason));
  g_assert (!breakpoint || IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));
  g_assert (IDE_IS_DEBUGGER (debugger));

  if (breakpoint != NULL)
    ide_debugger_editor_addin_navigate_to_breakpoint (self, breakpoint);

  IDE_EXIT;
}

static void
send_notification (IdeDebuggerEditorAddin *self,
                   const gchar            *title,
                   const gchar            *body,
                   const gchar            *icon_name,
                   gboolean                urgent)
{
  g_autoptr(IdeNotification) notif = NULL;
  g_autoptr(GIcon) icon = NULL;
  IdeContext *context;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));

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
  IdeDebuggerEditorAddin *self = user_data;
  IdeDebugManager *debug_manager;
  IdeContext *context;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));

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
debug_manager_notify_debugger (IdeDebuggerEditorAddin *self,
                               GParamSpec             *pspec,
                               IdeDebugManager        *debug_manager)
{
  IdeDebugger *debugger;
  IdeWorkspace *workspace;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_DEBUG_MANAGER (debug_manager));

  if (!gtk_widget_get_visible (GTK_WIDGET (self->panel)))
    {
      GtkWidget *stack = gtk_widget_get_parent (GTK_WIDGET (self->panel));

      gtk_widget_show (GTK_WIDGET (self->panel));

      if (GTK_IS_STACK (stack))
        gtk_stack_set_visible_child (GTK_STACK (stack), GTK_WIDGET (self->panel));
    }

  debugger = ide_debug_manager_get_debugger (debug_manager);

  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (self->editor))))
    gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                    "debugger",
                                    G_ACTION_GROUP (debugger));

  ide_debugger_breakpoints_view_set_debugger (self->breakpoints_view, debugger);
  ide_debugger_locals_view_set_debugger (self->locals_view, debugger);
  ide_debugger_libraries_view_set_debugger (self->libraries_view, debugger);
  ide_debugger_registers_view_set_debugger (self->registers_view, debugger);
  ide_debugger_threads_view_set_debugger (self->threads_view, debugger);

  dzl_signal_group_set_target (self->debugger_signals, debugger);
}

static void
debug_manager_notify_active (IdeDebuggerEditorAddin *self,
                             GParamSpec             *pspec,
                             IdeDebugManager        *debug_manager)
{
  gboolean reveal_child = FALSE;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
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

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->controls), reveal_child);
}

static void
on_frame_activated (IdeDebuggerEditorAddin *self,
                    IdeDebuggerThread      *thread,
                    IdeDebuggerFrame       *frame,
                    IdeDebuggerThreadsView *threads_view)
{
  IdeDebuggerAddress addr;
  const gchar *path;
  guint line;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
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
      g_autoptr(IdeLocation) location = NULL;
      g_autofree gchar *project_path = ide_context_build_filename (context, path, NULL);
      g_autoptr(GFile) file = g_file_new_for_path (project_path);

      location = ide_location_new (file, line, -1);
      ide_editor_surface_focus_location (self->editor, location);

      IDE_EXIT;
    }

  addr = ide_debugger_frame_get_address (frame);

  if (addr != IDE_DEBUGGER_ADDRESS_INVALID)
    {
      ide_debugger_editor_addin_navigate_to_address (self, addr);
      IDE_EXIT;
    }

  g_warning ("Failed to locate source or memory address for frame");

  IDE_EXIT;
}

static void
ide_debugger_editor_addin_add_ui (IdeDebuggerEditorAddin *self)
{
  GtkWidget *scroll_box;
  GtkWidget *box;
  GtkWidget *hpaned;
  GtkWidget *utilities;
  GtkWidget *overlay;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (self->editor));

#define OBSERVE_DESTROY(ptr) \
  g_signal_connect ((ptr), "destroy", G_CALLBACK (gtk_widget_destroyed), &(ptr))

  overlay = ide_editor_surface_get_overlay (self->editor);

  self->controls = g_object_new (IDE_TYPE_DEBUGGER_CONTROLS,
                                 "transition-duration", 500,
                                 "transition-type", GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP,
                                 "reveal-child", FALSE,
                                 "visible", TRUE,
                                 "halign", GTK_ALIGN_CENTER,
                                 "valign", GTK_ALIGN_END,
                                 NULL);
  OBSERVE_DESTROY (self->controls);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), GTK_WIDGET (self->controls));

  self->panel = g_object_new (DZL_TYPE_DOCK_WIDGET,
                              "title", _("Debugger"),
                              "icon-name", "builder-debugger-symbolic",
                              "visible", FALSE,
                              NULL);
  OBSERVE_DESTROY (self->panel);

  box = g_object_new (GTK_TYPE_NOTEBOOK,
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->panel), box);

  hpaned = g_object_new (DZL_TYPE_MULTI_PANED,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         "visible", TRUE,
                         NULL);
  gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (hpaned),
                                     "tab-label", _("Threads"),
                                     NULL);

  self->threads_view = g_object_new (IDE_TYPE_DEBUGGER_THREADS_VIEW,
                                     "hexpand", TRUE,
                                     "visible", TRUE,
                                     NULL);
  OBSERVE_DESTROY (self->threads_view);
  g_signal_connect_swapped (self->threads_view,
                            "frame-activated",
                            G_CALLBACK (on_frame_activated),
                            self);
  gtk_container_add (GTK_CONTAINER (hpaned), GTK_WIDGET (self->threads_view));

  self->locals_view = g_object_new (IDE_TYPE_DEBUGGER_LOCALS_VIEW,
                                    "width-request", 250,
                                    "visible", TRUE,
                                    NULL);
  OBSERVE_DESTROY (self->locals_view);
  gtk_container_add (GTK_CONTAINER (hpaned), GTK_WIDGET (self->locals_view));

  self->breakpoints_view = g_object_new (IDE_TYPE_DEBUGGER_BREAKPOINTS_VIEW,
                                         "visible", TRUE,
                                         NULL);
  OBSERVE_DESTROY (self->breakpoints_view);
  gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (self->breakpoints_view),
                                     "tab-label", _("Breakpoints"),
                                     NULL);

  self->libraries_view = g_object_new (IDE_TYPE_DEBUGGER_LIBRARIES_VIEW,
                                       "visible", TRUE,
                                       NULL);
  OBSERVE_DESTROY (self->libraries_view);
  gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (self->libraries_view),
                                     "tab-label", _("Libraries"),
                                     NULL);

  self->registers_view = g_object_new (IDE_TYPE_DEBUGGER_REGISTERS_VIEW,
                                       "visible", TRUE,
                                       NULL);
  OBSERVE_DESTROY (self->registers_view);
  gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (self->registers_view),
                                     "tab-label", _("Registers"),
                                     NULL);

  scroll_box = g_object_new (GTK_TYPE_BOX,
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
                             "visible", TRUE,
                             NULL);
  gtk_container_add_with_properties (GTK_CONTAINER (box), GTK_WIDGET (scroll_box),
                                     "tab-label", _("Log"),
                                     NULL);

  self->log_view = g_object_new (IDE_TYPE_TERMINAL,
                                 "hexpand", TRUE,
                                 "visible", TRUE,
                                 NULL);
  OBSERVE_DESTROY (self->log_view);
  gtk_container_add (GTK_CONTAINER (scroll_box), GTK_WIDGET (self->log_view));

  self->log_view_scroller = g_object_new (GTK_TYPE_SCROLLBAR,
                                          "adjustment", gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (self->log_view)),
                                          "orientation", GTK_ORIENTATION_VERTICAL,
                                          "visible", TRUE,
                                          NULL);
  gtk_container_add (GTK_CONTAINER (scroll_box), GTK_WIDGET (self->log_view_scroller));

  utilities = ide_editor_surface_get_utilities (self->editor);
  gtk_container_add (GTK_CONTAINER (utilities), GTK_WIDGET (self->panel));

#undef OBSERVE_DESTROY
}

static void
ide_debugger_editor_addin_load (IdeEditorAddin   *addin,
                                IdeEditorSurface *editor)
{
  IdeDebuggerEditorAddin *self = (IdeDebuggerEditorAddin *)addin;
  IdeContext *context;
  IdeRunManager *run_manager;
  IdeDebugManager *debug_manager;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  self->editor = editor;
  self->workbench = ide_widget_get_workbench (GTK_WIDGET (editor));

  if (!ide_workbench_has_project (self->workbench))
    return;

  context = ide_widget_get_context (GTK_WIDGET (editor));
  run_manager = ide_run_manager_from_context (context);
  debug_manager = ide_debug_manager_from_context (context);

  ide_debugger_editor_addin_add_ui (self);

  ide_run_manager_add_handler (run_manager,
                               "debugger",
                               _("Run with Debugger"),
                               "builder-debugger-symbolic",
                               "F5",
                               debugger_run_handler,
                               g_object_ref (self),
                               g_object_unref);

  self->debugger_signals = dzl_signal_group_new (IDE_TYPE_DEBUGGER);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "log",
                                    G_CALLBACK (debugger_log),
                                    self);

  dzl_signal_group_connect_swapped (self->debugger_signals,
                                    "stopped",
                                    G_CALLBACK (debugger_stopped),
                                    self);

  self->debug_manager_signals = dzl_signal_group_new (IDE_TYPE_DEBUG_MANAGER);

  dzl_signal_group_connect_swapped (self->debug_manager_signals,
                                    "notify::active",
                                    G_CALLBACK (debug_manager_notify_active),
                                    self);

  dzl_signal_group_connect_swapped (self->debug_manager_signals,
                                    "notify::debugger",
                                    G_CALLBACK (debug_manager_notify_debugger),
                                    self);

  dzl_signal_group_set_target (self->debug_manager_signals, debug_manager);

  IDE_EXIT;
}

static void
ide_debugger_editor_addin_unload (IdeEditorAddin   *addin,
                                  IdeEditorSurface *editor)
{
  IdeDebuggerEditorAddin *self = (IdeDebuggerEditorAddin *)addin;
  IdeRunManager *run_manager;
  IdeWorkspace *workspace;
  IdeContext *context;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_assert (IDE_IS_EDITOR_SURFACE (editor));

  if (!ide_workbench_has_project (self->workbench))
    return;

  context = ide_workbench_get_context (self->workbench);
  run_manager = ide_run_manager_from_context (context);

  if ((workspace = ide_widget_get_workspace (GTK_WIDGET (editor))))
    gtk_widget_insert_action_group (GTK_WIDGET (workspace), "debugger", NULL);

  /* Remove the handler to initiate the debugger */
  ide_run_manager_remove_handler (run_manager, "debugger");

  g_clear_object (&self->debugger_signals);
  g_clear_object (&self->debug_manager_signals);

  if (self->panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->panel));
  if (self->controls != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->controls));
  if (self->disassembly_view != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->disassembly_view));

  self->editor = NULL;
  self->workbench = NULL;

  IDE_EXIT;
}

static void
editor_addin_iface_init (IdeEditorAddinInterface *iface)
{
  iface->load = ide_debugger_editor_addin_load;
  iface->unload = ide_debugger_editor_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (IdeDebuggerEditorAddin, ide_debugger_editor_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_EDITOR_ADDIN, editor_addin_iface_init))

static void
ide_debugger_editor_addin_class_init (IdeDebuggerEditorAddinClass *klass)
{
}

static void
ide_debugger_editor_addin_init (IdeDebuggerEditorAddin *self)
{
}

void
ide_debugger_editor_addin_navigate_to_file (IdeDebuggerEditorAddin *self,
                                            GFile                  *file,
                                            guint                   line)
{
  g_autoptr(IdeLocation) location = NULL;

  g_return_if_fail (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_return_if_fail (G_IS_FILE (file));

  location = ide_location_new (file, line, -1);

  ide_editor_surface_focus_location (self->editor, location);
}

static void
ide_debugger_editor_addin_disassemble_cb (GObject      *object,
                                          GAsyncResult *result,
                                          gpointer      user_data)
{
  IdeDebugger *debugger = (IdeDebugger *)object;
  g_autoptr(IdeDebuggerEditorAddin) self = user_data;
  g_autoptr(GPtrArray) instructions = NULL;
  g_autoptr(GError) error = NULL;
  GtkWidget *stack;

  IDE_ENTRY;

  g_assert (IDE_IS_DEBUGGER (debugger));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));

  instructions = ide_debugger_disassemble_finish (debugger, result, &error);

  if (instructions == NULL)
    {
      g_warning ("%s", error->message);
      IDE_EXIT;
    }

  if (self->editor == NULL)
    IDE_EXIT;

  if (self->disassembly_view == NULL)
    {
      IdeGrid *grid = ide_editor_surface_get_grid (self->editor);

      self->disassembly_view = g_object_new (IDE_TYPE_DEBUGGER_DISASSEMBLY_VIEW,
                                             "visible", TRUE,
                                             NULL);
      g_signal_connect (self->disassembly_view,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->disassembly_view);
      gtk_container_add (GTK_CONTAINER (grid), GTK_WIDGET (self->disassembly_view));
    }

  ide_debugger_disassembly_view_set_instructions (self->disassembly_view, instructions);

  /* TODO: Set current instruction */

  /* FIXME: It would be nice if we had a nicer API for this */
  stack = gtk_widget_get_ancestor (GTK_WIDGET (self->disassembly_view), IDE_TYPE_FRAME);
  if (stack != NULL)
    ide_frame_set_visible_child (IDE_FRAME (stack),
                                        IDE_PAGE (self->disassembly_view));

  IDE_EXIT;
}

void
ide_debugger_editor_addin_navigate_to_address (IdeDebuggerEditorAddin *self,
                                               IdeDebuggerAddress      address)
{
  IdeDebugger *debugger;
  IdeDebuggerAddressRange range;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_return_if_fail (address != IDE_DEBUGGER_ADDRESS_INVALID);

  if (NULL == (debugger = dzl_signal_group_get_target (self->debugger_signals)))
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
                                  ide_debugger_editor_addin_disassemble_cb,
                                  g_object_ref (self));

  IDE_EXIT;

}

void
ide_debugger_editor_addin_navigate_to_breakpoint (IdeDebuggerEditorAddin *self,
                                                  IdeDebuggerBreakpoint  *breakpoint)
{
  IdeDebuggerAddress address;
  const gchar *path;
  guint line;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_DEBUGGER_EDITOR_ADDIN (self));
  g_return_if_fail (IDE_IS_DEBUGGER_BREAKPOINT (breakpoint));

  address = ide_debugger_breakpoint_get_address (breakpoint);
  path = ide_debugger_breakpoint_get_file (breakpoint);
  line = ide_debugger_breakpoint_get_line (breakpoint);

  if (line > 0)
    line--;

  if (path != NULL)
    {
      g_autoptr(GFile) file = g_file_new_for_path (path);
      ide_debugger_editor_addin_navigate_to_file (self, file, line);
    }
  else if (address != IDE_DEBUGGER_ADDRESS_INVALID)
    {
      ide_debugger_editor_addin_navigate_to_address (self, address);
    }

  IDE_EXIT;
}
