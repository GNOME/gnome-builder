/* gb-terminal-workbench-addin.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "gb-terminal-workbench-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <ide.h>
#include <vte/vte.h>

#include "gb-terminal-util.h"
#include "gb-terminal-view.h"
#include "gb-terminal-workbench-addin.h"

struct _GbTerminalWorkbenchAddin
{
  GObject         parent_instance;

  IdeWorkbench   *workbench;

  GbTerminalView *panel_terminal;
  GtkWidget      *panel_dock_widget;

  GbTerminalView *run_terminal;
  GtkWidget      *run_panel;
};

static void workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface);

G_DEFINE_TYPE_EXTENDED (GbTerminalWorkbenchAddin,
                        gb_terminal_workbench_addin,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKBENCH_ADDIN, workbench_addin_iface_init))

static void
new_terminal_activate_cb (GSimpleAction            *action,
                          GVariant                 *param,
                          GbTerminalWorkbenchAddin *self)
{
  GbTerminalView *view;
  IdePerspective *perspective;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  perspective = ide_workbench_get_perspective_by_name (self->workbench, "editor");
  ide_workbench_set_visible_perspective (self->workbench, perspective);

  view = g_object_new (GB_TYPE_TERMINAL_VIEW,
                       "visible", TRUE,
                       NULL);
  gtk_container_add (GTK_CONTAINER (perspective), GTK_WIDGET (view));
  ide_workbench_focus (self->workbench, GTK_WIDGET (view));
}

static void
on_run_manager_run (GbTerminalWorkbenchAddin *self,
                    IdeRunner                *runner,
                    IdeRunManager            *run_manager)
{
  IdeEnvironment *env;
  VtePty *pty = NULL;
  int tty_fd;

  IDE_ENTRY;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  /*
   * We need to create a new or re-use our existing terminal view
   * for run output. Additionally, we need to override the stdin,
   * stdout, and stderr file-descriptors to our pty master for the
   * terminal instance.
   */

  pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);

  if (pty == NULL)
    {
      g_warning ("Failed to allocate PTY for run output");
      IDE_GOTO (failure);
    }

  if (self->run_terminal == NULL)
    {
      IdePerspective *perspective;
      GbTerminalView *view;
      GtkWidget *bottom_pane;
      GtkWidget *panel;

      view = g_object_new (GB_TYPE_TERMINAL_VIEW,
                           "manage-spawn", FALSE,
                           "pty", pty,
                           "visible", TRUE,
                           NULL);
      ide_set_weak_pointer (&self->run_terminal, view);

      panel = g_object_new (PNL_TYPE_DOCK_WIDGET,
                            "child", self->run_terminal,
                            "expand", TRUE,
                            "title", _("Run Output"),
                            "visible", TRUE,
                            NULL);
      ide_set_weak_pointer (&self->run_panel, panel);

      perspective = ide_workbench_get_perspective_by_name (self->workbench, "editor");
      g_assert (IDE_IS_LAYOUT (perspective));

      bottom_pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (perspective));
      gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->run_panel));
    }
  else
    {
      gb_terminal_view_set_pty (self->run_terminal, pty);
    }

  if (-1 != (tty_fd = gb_vte_pty_create_slave (pty)))
    {
      ide_runner_set_tty (runner, tty_fd);
      close (tty_fd);
    }

  env = ide_runner_get_environment (runner);
  ide_environment_setenv (env, "TERM", "xterm-256color");
  ide_environment_setenv (env, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);

failure:

  g_clear_object (&pty);

  IDE_EXIT;
}

static void
gb_terminal_workbench_addin_load (IdeWorkbenchAddin *addin,
                                  IdeWorkbench      *workbench)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;
  IdePerspective *perspective;
  GtkWidget *bottom_pane;
  IdeContext *context;
  IdeRunManager *run_manager;
  g_autoptr(GSimpleAction) action = NULL;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);

  ide_set_weak_pointer (&self->workbench, workbench);

  action = g_simple_action_new ("new-terminal", NULL);
  g_signal_connect_object (action,
                           "activate",
                           G_CALLBACK (new_terminal_activate_cb),
                           self,
                           0);
  g_action_map_add_action (G_ACTION_MAP (workbench), G_ACTION (action));

  if (self->panel_terminal == NULL)
    {
      self->panel_dock_widget = g_object_new (PNL_TYPE_DOCK_WIDGET,
                                              "expand", TRUE,
                                              "title", _("Terminal"),
                                              "visible", TRUE,
                                              NULL);
      self->panel_terminal = g_object_new (GB_TYPE_TERMINAL_VIEW,
                                           "visible", TRUE,
                                           NULL);
      gtk_container_add (GTK_CONTAINER (self->panel_dock_widget),
                         GTK_WIDGET (self->panel_terminal));

      g_object_add_weak_pointer (G_OBJECT (self->panel_terminal),
                                 (gpointer *)&self->panel_terminal);
      g_object_add_weak_pointer (G_OBJECT (self->panel_dock_widget),
                                 (gpointer *)&self->panel_dock_widget);
    }

  perspective = ide_workbench_get_perspective_by_name (workbench, "editor");
  g_assert (IDE_IS_LAYOUT (perspective));

  bottom_pane = pnl_dock_bin_get_bottom_edge (PNL_DOCK_BIN (perspective));
  gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->panel_dock_widget));

  run_manager = ide_context_get_run_manager (context);
  g_signal_connect_object (run_manager,
                           "run",
                           G_CALLBACK (on_run_manager_run),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
gb_terminal_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal");

  if (self->panel_dock_widget != NULL)
    {
      gtk_widget_destroy (self->panel_dock_widget);
      ide_clear_weak_pointer (&self->panel_dock_widget);
    }

  if (self->run_panel != NULL)
    {
      gtk_widget_destroy (self->run_panel);
      ide_clear_weak_pointer (&self->run_panel);
    }
}

static void
gb_terminal_workbench_addin_class_init (GbTerminalWorkbenchAddinClass *klass)
{
}

static void
gb_terminal_workbench_addin_init (GbTerminalWorkbenchAddin *self)
{
}

static void
workbench_addin_iface_init (IdeWorkbenchAddinInterface *iface)
{
  iface->load = gb_terminal_workbench_addin_load;
  iface->unload = gb_terminal_workbench_addin_unload;
}
