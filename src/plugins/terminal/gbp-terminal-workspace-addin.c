/* gbp-terminal-workspace-addin.c
 *
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-terminal-workspace-addin"

#include "config.h"

#include <glib/gi18n.h>
#include <libide-editor.h>
#include <libide-foundry.h>
#include <libide-terminal.h>
#include <libide-gui.h>

#include "ide-terminal-run-command-private.h"

#include "gbp-terminal-workspace-addin.h"

struct _GbpTerminalWorkspaceAddin
{
  GObject       parent_instance;
  IdeWorkspace *workspace;
};

static void terminal_on_host_action       (GbpTerminalWorkspaceAddin *self,
                                           GVariant                  *param);
static void terminal_as_subprocess_action (GbpTerminalWorkspaceAddin *self,
                                           GVariant                  *param);
static void terminal_in_pipeline_action   (GbpTerminalWorkspaceAddin *self,
                                           GVariant                  *param);
static void terminal_in_runtime_action    (GbpTerminalWorkspaceAddin *self,
                                           GVariant                  *param);

IDE_DEFINE_ACTION_GROUP (GbpTerminalWorkspaceAddin, gbp_terminal_workspace_addin, {
  { "terminal-on-host", terminal_on_host_action, "s" },
  { "terminal-as-subprocess", terminal_as_subprocess_action, "s" },
  { "terminal-in-pipeline", terminal_in_pipeline_action, "s" },
  { "terminal-in-runtime", terminal_in_runtime_action, "s" },
})

static void
gbp_terminal_workspace_addin_add_page (GbpTerminalWorkspaceAddin *self,
                                       IdeTerminalRunLocality     locality,
                                       const char                *cwd)
{
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_autoptr(IdePanelPosition) position = NULL;
  g_autoptr(IdeRunCommand) run_command = NULL;
  IdeContext *context;
  IdePage *current_page;
  IdePage *page;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (self->workspace));
  g_assert (locality >= IDE_TERMINAL_RUN_ON_HOST);
  g_assert (locality < IDE_TERMINAL_RUN_LAST);

  run_command = ide_terminal_run_command_new (locality);

  if (!ide_str_empty0 (cwd))
    ide_run_command_set_cwd (run_command, cwd);

  context = ide_workspace_get_context (self->workspace);
  launcher = ide_terminal_launcher_new (context, run_command);

  if ((current_page = ide_workspace_get_most_recent_page (self->workspace)))
    position = ide_page_get_position (current_page);

  if (position == NULL)
    position = ide_panel_position_new ();

  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "respawn-on-exit", FALSE,
                       "manage-spawn", TRUE,
                       "launcher", launcher,
                       "visible", TRUE,
                       NULL);
  ide_workspace_add_page (self->workspace, page, position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
terminal_on_host_action (GbpTerminalWorkspaceAddin *self,
                         GVariant                  *param)
{
  const char *cwd = g_variant_get_string (param, NULL);
  g_autofree char *project_dir = NULL;

  if (ide_str_empty0 (cwd))
    {
      IdeContext *context = ide_workspace_get_context (self->workspace);
      g_autoptr(GFile) workdir = ide_context_ref_workdir (context);
      cwd = project_dir = g_file_get_path (workdir);
    }

  gbp_terminal_workspace_addin_add_page (self, IDE_TERMINAL_RUN_ON_HOST, cwd);
}

static void
terminal_as_subprocess_action (GbpTerminalWorkspaceAddin *self,
                               GVariant                  *param)
{
  const char *cwd = g_variant_get_string (param, NULL);

  if (ide_str_empty0 (cwd))
    cwd = g_get_home_dir ();

  gbp_terminal_workspace_addin_add_page (self, IDE_TERMINAL_RUN_AS_SUBPROCESS, cwd);
}

static void
terminal_in_pipeline_action (GbpTerminalWorkspaceAddin *self,
                             GVariant                  *param)
{
  gbp_terminal_workspace_addin_add_page (self,
                                         IDE_TERMINAL_RUN_IN_PIPELINE,
                                         g_variant_get_string (param, NULL));
}

static void
terminal_in_runtime_action (GbpTerminalWorkspaceAddin *self,
                            GVariant                  *param)
{
  const char *cwd = g_variant_get_string (param, NULL);

  if (ide_str_empty0 (cwd))
    cwd = g_get_home_dir ();

  gbp_terminal_workspace_addin_add_page (self, IDE_TERMINAL_RUN_AS_SUBPROCESS, cwd);
  gbp_terminal_workspace_addin_add_page (self, IDE_TERMINAL_RUN_IN_RUNTIME, cwd);
}

static void
on_run_manager_run (IdeRunManager   *run_manager,
                    IdeRunContext   *run_context,
                    IdeTerminalPage *page)
{
  g_autoptr(GDateTime) now = NULL;
  g_autofree char *formatted = NULL;
  g_autofree char *tmp = NULL;
  VtePty *pty;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_TERMINAL_PAGE (page));

  pty = ide_terminal_page_get_pty (page);

  ide_run_context_push (run_context, NULL, NULL, NULL);
  ide_run_context_set_pty (run_context, pty);
  ide_run_context_setenv (run_context, "TERM", "xterm-256color");
  ide_run_context_setenv (run_context, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);

  now = g_date_time_new_now_local ();
  tmp = g_date_time_format (now, "%X");

  /* translators: %s is replaced with the current local time of day */
  formatted = g_strdup_printf (_("Application started at %s\r\n"), tmp);
  ide_terminal_page_feed (page, formatted);

  panel_widget_raise (PANEL_WIDGET (page));

  IDE_EXIT;
}

static void
on_run_manager_stopped (IdeRunManager   *run_manager,
                        IdeTerminalPage *page)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_RUN_MANAGER (run_manager));
  g_assert (IDE_IS_TERMINAL_PAGE (page));

  ide_terminal_page_feed (page, _("Application exited"));
  ide_terminal_page_feed (page, "\r\n");

  IDE_EXIT;
}

static void
gbp_terminal_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  g_autoptr(IdePanelPosition) position = NULL;
  IdePage *page;
  IdePane *pane;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  self->workspace = workspace;

  gtk_widget_insert_action_group (GTK_WIDGET (workspace),
                                  "terminal",
                                  G_ACTION_GROUP (self));

  /* Always add the terminal panel to primary/editor workspaces */
  position = ide_panel_position_new ();
  ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_BOTTOM);
  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "respawn-on-exit", TRUE,
                       "visible", TRUE,
                       NULL);
  pane = g_object_new (IDE_TYPE_PANE,
                       "title", _("Terminal"),
                       "icon-name", "builder-terminal-symbolic",
                       "child", page,
                       NULL);
  ide_workspace_add_pane (workspace, pane, position);

  /* Setup panel for application output */
  if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    {
      IdeWorkbench *workbench = ide_workspace_get_workbench (workspace);
      IdeContext *context = ide_workbench_get_context (workbench);
      IdeRunManager *run_manager = ide_run_manager_from_context (context);
      VtePty *pty = vte_pty_new_sync (VTE_PTY_DEFAULT, NULL, NULL);

      page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                           "respawn-on-exit", FALSE,
                           "manage-spawn", FALSE,
                           "pty", pty,
                           NULL);
      pane = g_object_new (IDE_TYPE_PANE,
                           "title", _("Application Output"),
                           "icon-name", "builder-run-start-symbolic",
                           "child", page,
                           NULL);
      ide_workspace_add_pane (workspace, pane, position);

      g_signal_connect_object (run_manager,
                               "run",
                               G_CALLBACK (on_run_manager_run),
                               page,
                               0);
      g_signal_connect_object (run_manager,
                               "stopped",
                               G_CALLBACK (on_run_manager_stopped),
                               page,
                               0);
    }
}

static void
gbp_terminal_workspace_addin_unload (IdeWorkspaceAddin *addin,
                                     IdeWorkspace      *workspace)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));

  if (ide_workbench_has_project (workbench))
    {
      IdeContext *context = ide_widget_get_context (GTK_WIDGET (workspace));
      IdeRunManager *run_manager = ide_run_manager_from_context (context);

      g_signal_handlers_disconnect_by_func (run_manager,
                                            G_CALLBACK (on_run_manager_run),
                                            self);
      g_signal_handlers_disconnect_by_func (run_manager,
                                            G_CALLBACK (on_run_manager_stopped),
                                            self);
    }

  gtk_widget_insert_action_group (GTK_WIDGET (workspace), "terminal", NULL);

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_terminal_workspace_addin_load;
  iface->unload = gbp_terminal_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTerminalWorkspaceAddin, gbp_terminal_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, gbp_terminal_workspace_addin_init_action_group)
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_terminal_workspace_addin_class_init (GbpTerminalWorkspaceAddinClass *klass)
{
}

static void
gbp_terminal_workspace_addin_init (GbpTerminalWorkspaceAddin *self)
{
}
