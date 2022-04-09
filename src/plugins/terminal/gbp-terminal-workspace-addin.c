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

#include "gbp-terminal-workspace-addin.h"

struct _GbpTerminalWorkspaceAddin
{
  GObject             parent_instance;

  IdeWorkspace       *workspace;

  IdePane            *bottom_dock;
  IdeTerminalPage    *bottom;

  IdePane            *run_panel;
  IdeTerminalPage    *run_terminal;
};

static IdeRuntime *
find_runtime (IdeWorkspace *workspace)
{
  IdeContext *context;
  IdeConfigManager *config_manager;
  IdeConfig *config;

  g_assert (IDE_IS_WORKSPACE (workspace));

  context = ide_workspace_get_context (workspace);
  config_manager = ide_config_manager_from_context (context);
  config = ide_config_manager_get_current (config_manager);

  return ide_config_get_runtime (config);
}

static gchar *
find_builddir (IdeWorkspace *workspace)
{
  IdeContext *context;
  IdeBuildManager *build_manager;
  IdePipeline *pipeline;
  const gchar *builddir = NULL;

  if ((context = ide_workspace_get_context (workspace)) &&
      (build_manager = ide_build_manager_from_context (context)) &&
      (pipeline = ide_build_manager_get_pipeline (build_manager)) &&
      (builddir = ide_pipeline_get_builddir (pipeline)) &&
      g_file_test (builddir, G_FILE_TEST_IS_DIR))
    return g_strdup (builddir);

  return NULL;
}

static void
new_terminal_activate (GSimpleAction *action,
                       GVariant      *param,
                       gpointer       user_data)
{
  GbpTerminalWorkspaceAddin *self = user_data;
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_autoptr(IdePanelPosition) position = NULL;
  g_autofree gchar *cwd = NULL;
  IdePage *page;
  IdeRuntime *runtime = NULL;
  IdeContext *context;
  const gchar *name;
  GtkWidget *current_frame = NULL;
  IdePage *current_page;
  const gchar *uri = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));

  /* If we are creating a new terminal while we already have a terminal
   * focused, then try to copy some details from that terminal.
   */
  if ((current_page = ide_workspace_get_most_recent_page (self->workspace)))
    {
      if (IDE_IS_TERMINAL_PAGE (current_page))
        uri = ide_terminal_page_get_current_directory_uri (IDE_TERMINAL_PAGE (current_page));
      current_frame = gtk_widget_get_ancestor (GTK_WIDGET (current_page), IDE_TYPE_FRAME);
    }

  context = ide_workspace_get_context (self->workspace);
  name = g_action_get_name (G_ACTION (action));

  /* Only allow plain terminals unless this is a project */
  if (!ide_context_has_project (context) &&
      !ide_str_equal0 (name, "debug-terminal"))
    name = "new-terminal";

  if (ide_str_equal0 (name, "new-terminal-in-config"))
    {
      IdeConfigManager *config_manager = ide_config_manager_from_context (context);
      IdeConfig *config = ide_config_manager_get_current (config_manager);

      cwd = find_builddir (self->workspace);
      launcher = ide_terminal_launcher_new_for_config (config);
    }
  else if (ide_str_equal0 (name, "new-terminal-in-runtime"))
    {
      runtime = find_runtime (self->workspace);
      cwd = find_builddir (self->workspace);
      launcher = ide_terminal_launcher_new_for_runtime (runtime);
    }
  else if (ide_str_equal0 (name, "debug-terminal"))
    {
      launcher = ide_terminal_launcher_new_for_debug ();
    }
  else if (ide_str_equal0 (name, "new-terminal-in-runner"))
    {
      runtime = find_runtime (self->workspace);
      launcher = ide_terminal_launcher_new_for_runner (runtime);
    }

  if (ide_str_equal0 (name, "new-terminal-in-dir"))
    {
      page = ide_workspace_get_most_recent_page (self->workspace);

      if (IDE_IS_EDITOR_PAGE (page))
        {
          IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (page));

          if (buffer != NULL)
            {
              GFile *file = ide_buffer_get_file (buffer);
              g_autoptr(GFile) parent = g_file_get_parent (file);

              cwd = g_file_get_path (parent);
            }
        }
    }

  if (cwd != NULL)
    {
      ide_terminal_launcher_set_cwd (launcher, cwd);
    }
  else if (uri != NULL)
    {
      g_autoptr(GFile) file = g_file_new_for_uri (uri);

      if (g_file_is_native (file))
        ide_terminal_launcher_set_cwd (launcher, g_file_peek_path (file));
    }

  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "launcher", launcher,
                       "respawn-on-exit", FALSE,
                       "visible", TRUE,
                       NULL);

  if (current_frame != NULL)
    position = ide_frame_get_position (IDE_FRAME (current_frame));
  else
    position = ide_panel_position_new ();

  ide_workspace_add_page (self->workspace, page, position);
}

static void
on_run_manager_run (GbpTerminalWorkspaceAddin *self,
                    IdeRunner                 *runner,
                    IdeRunManager             *run_manager)
{
  IdeEnvironment *env;
  VtePty *pty = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autofree gchar *formatted = NULL;
  g_autofree gchar *tmp = NULL;

  IDE_ENTRY;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUNNER (runner));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  /*
   * We need to create a new or re-use our existing terminal page
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
      g_autoptr(IdePanelPosition) position = NULL;

      self->run_terminal = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                                         "manage-spawn", FALSE,
                                         "pty", pty,
                                         NULL);
      self->run_panel = g_object_new (IDE_TYPE_PANE,
                                      "child", self->run_terminal,
                                      "expand", TRUE,
                                      "icon-name", "builder-run-start-symbolic",
                                      "title", _("Application Output"),
                                      NULL);

      position = ide_panel_position_new ();
      ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_BOTTOM);
      ide_workspace_add_pane (self->workspace, self->run_panel, position);
    }
  else
    {
      ide_terminal_page_set_pty (self->run_terminal, pty);
    }

  ide_runner_set_pty (runner, pty);

  env = ide_runner_get_environment (runner);
  ide_environment_setenv (env, "TERM", "xterm-256color");
  ide_environment_setenv (env, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);

  now = g_date_time_new_now_local ();
  tmp = g_date_time_format (now, "%X");

  /* translators: %s is replaced with the current local time of day */
  formatted = g_strdup_printf (_("Application started at %s\r\n"), tmp);
  ide_terminal_page_feed (self->run_terminal, formatted);

failure:
  g_clear_object (&pty);

  IDE_EXIT;
}

static void
on_run_manager_stopped (GbpTerminalWorkspaceAddin *self,
                        IdeRunManager             *run_manager)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  ide_terminal_page_feed (self->run_terminal, _("Application exited\r\n"));
}

static const GActionEntry terminal_actions[] = {
  { "new-terminal", new_terminal_activate },
  { "new-terminal-in-config", new_terminal_activate },
  { "new-terminal-in-runner", new_terminal_activate },
  { "new-terminal-in-runtime", new_terminal_activate },
  { "new-terminal-in-dir", new_terminal_activate },
  { "debug-terminal", new_terminal_activate },
};

static void
gbp_terminal_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  IdeWorkbench *workbench;
  g_autoptr(IdePanelPosition) position = NULL;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  self->workspace = workspace;

  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   terminal_actions,
                                   G_N_ELEMENTS (terminal_actions),
                                   self);


  position = ide_panel_position_new ();
  ide_panel_position_set_edge (position, PANEL_DOCK_POSITION_BOTTOM);

  self->bottom = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                               "respawn-on-exit", TRUE,
                               "visible", TRUE,
                               NULL);
  self->bottom_dock = g_object_new (IDE_TYPE_PANE,
                                    "title", _("Terminal"),
                                    "icon-name", "builder-terminal-symbolic",
                                    "child", self->bottom,
                                    NULL);
  ide_workspace_add_pane (workspace, IDE_PANE (self->bottom_dock), position);

  workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));

  if (ide_workbench_has_project (workbench) && IDE_IS_PRIMARY_WORKSPACE (workspace))
    {
      /* Setup terminals when a project is run */
      context = ide_widget_get_context (GTK_WIDGET (workspace));
      run_manager = ide_run_manager_from_context (context);
      g_signal_connect_object (run_manager,
                               "run",
                               G_CALLBACK (on_run_manager_run),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (run_manager,
                               "stopped",
                               G_CALLBACK (on_run_manager_stopped),
                               self,
                               G_CONNECT_SWAPPED);
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
      IdeRunManager *run_manager;
      IdeContext *context;

      context = ide_widget_get_context (GTK_WIDGET (workspace));
      run_manager = ide_run_manager_from_context (context);
      g_signal_handlers_disconnect_by_func (run_manager,
                                            G_CALLBACK (on_run_manager_run),
                                            self);
      g_signal_handlers_disconnect_by_func (run_manager,
                                            G_CALLBACK (on_run_manager_stopped),
                                            self);
    }

  for (guint i = 0; i < G_N_ELEMENTS (terminal_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), terminal_actions[i].name);

  g_clear_pointer ((PanelWidget **)&self->bottom_dock, panel_widget_close);
  g_clear_pointer ((PanelWidget **)&self->run_panel, panel_widget_close);

  self->bottom = NULL;
  self->bottom_dock = NULL;
  self->run_terminal = NULL;
  self->run_panel = NULL;

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_terminal_workspace_addin_load;
  iface->unload = gbp_terminal_workspace_addin_unload;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GbpTerminalWorkspaceAddin, gbp_terminal_workspace_addin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_terminal_workspace_addin_class_init (GbpTerminalWorkspaceAddinClass *klass)
{
}

static void
gbp_terminal_workspace_addin_init (GbpTerminalWorkspaceAddin *self)
{
}
