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

  DzlDockWidget      *bottom_dock;
  IdeTerminalPage    *bottom;

  DzlDockWidget      *run_panel;
  IdeTerminalPage    *run_terminal;

  IdeTerminalPopover *popover;
};

static void
terminal_text_inserted_cb (IdeTerminalPage *page,
                           DzlDockItem     *item)
{
  g_assert (IDE_IS_TERMINAL_PAGE (page));
  g_assert (DZL_IS_DOCK_ITEM (item));

  dzl_dock_item_needs_attention (item);
}

static void
bind_needs_attention (DzlDockItem     *item,
                      IdeTerminalPage *page)
{
  g_assert (DZL_IS_DOCK_ITEM (item));
  g_assert (IDE_IS_TERMINAL_PAGE (page));

  g_signal_connect_object (page,
                           "text-inserted",
                           G_CALLBACK (terminal_text_inserted_cb),
                           item,
                           0);
}

static void
new_terminal_workspace (GSimpleAction *action,
                        GVariant      *param,
                        gpointer       user_data)
{
  GbpTerminalWorkspaceAddin *self = user_data;
  IdeTerminalWorkspace *workspace;
  IdeWorkbench *workbench;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));

  workbench = ide_widget_get_workbench (GTK_WIDGET (self->workspace));
  workspace = g_object_new (IDE_TYPE_TERMINAL_WORKSPACE,
                            "application", IDE_APPLICATION_DEFAULT,
                            NULL);
  ide_workbench_add_workspace (workbench, IDE_WORKSPACE (workspace));
  ide_workbench_focus_workspace (workbench, IDE_WORKSPACE (workspace));
}

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
  g_autofree gchar *cwd = NULL;
  IdeTerminalPage *page;
  IdeSurface *surface;
  IdeRuntime *runtime = NULL;
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

  name = g_action_get_name (G_ACTION (action));

  if (ide_str_equal0 (name, "new-terminal-in-runtime"))
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
  else
    {
      if (self->popover != NULL)
        {
          runtime = ide_terminal_popover_get_runtime (self->popover);
          launcher = ide_terminal_launcher_new_for_runtime (runtime);
        }
      else
        {
          IdeContext *context = ide_widget_get_context (GTK_WIDGET (self->workspace));
          launcher = ide_terminal_launcher_new (context);
        }
    }

  if (!(surface = ide_workspace_get_surface_by_name (self->workspace, "editor")) &&
      !(surface = ide_workspace_get_surface_by_name (self->workspace, "terminal")))
    return;

  ide_workspace_set_visible_surface (self->workspace, surface);

  if (IDE_IS_EDITOR_SURFACE (surface) && ide_str_equal0 (name, "new-terminal-in-dir"))
    {
      IdePage *editor = ide_editor_surface_get_active_page (IDE_EDITOR_SURFACE (surface));

      if (IDE_IS_EDITOR_PAGE (editor))
        {
          IdeBuffer *buffer = ide_editor_page_get_buffer (IDE_EDITOR_PAGE (editor));

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
    gtk_container_add (GTK_CONTAINER (current_frame), GTK_WIDGET (page));
  else
    gtk_container_add (GTK_CONTAINER (surface), GTK_WIDGET (page));

  ide_widget_reveal_and_grab (GTK_WIDGET (page));
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
      IdeSurface *surface;
      GtkWidget *bottom_pane;

      self->run_terminal = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                                         "manage-spawn", FALSE,
                                         "pty", pty,
                                         "visible", TRUE,
                                         NULL);
      g_signal_connect (self->run_terminal,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->run_terminal);

      self->run_panel = g_object_new (DZL_TYPE_DOCK_WIDGET,
                                      "child", self->run_terminal,
                                      "expand", TRUE,
                                      "icon-name", "media-playback-start-symbolic",
                                      "title", _("Application Output"),
                                      "visible", TRUE,
                                      NULL);
      bind_needs_attention (DZL_DOCK_ITEM (self->run_panel), self->run_terminal);
      g_signal_connect (self->run_panel,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->run_panel);

      surface = ide_workspace_get_surface_by_name (self->workspace, "editor");
      g_assert (IDE_IS_EDITOR_SURFACE (surface));

      bottom_pane = ide_editor_surface_get_utilities (IDE_EDITOR_SURFACE (surface));
      gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->run_panel));
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

  dzl_dock_item_present (DZL_DOCK_ITEM (self->run_panel));

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
  { "new-terminal-workspace", new_terminal_workspace },
  { "new-terminal", new_terminal_activate },
  { "new-terminal-in-runner", new_terminal_activate },
  { "new-terminal-in-runtime", new_terminal_activate },
  { "new-terminal-in-dir", new_terminal_activate },
  { "debug-terminal", new_terminal_activate },
};

#define I_ g_intern_string

static const DzlShortcutEntry gbp_terminal_shortcut_entries[] = {
  { "org.gnome.builder.workspace.new-terminal",
    0, NULL,
    N_("Workspace shortcuts"),
    N_("General"),
    N_("Terminal") },

  { "org.gnome.builder.workspace.new-terminal-in-runtime",
    0, NULL,
    N_("Workspace shortcuts"),
    N_("General"),
    N_("Terminal in Build Runtime") },

  { "org.gnome.builder.workspace.new-terminal-in-runner",
    0, NULL,
    N_("Workspace shortcuts"),
    N_("General"),
    N_("Terminal in Runtime") },
};

static void
gbp_terminal_workspace_addin_setup_shortcuts (GbpTerminalWorkspaceAddin *self,
                                              IdeWorkspace              *workspace)
{
  DzlShortcutController *controller;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_WORKSPACE (workspace));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (workspace));

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workspace.new-terminal",
                                              I_("<primary><shift>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workspace.new-terminal-in-runtime",
                                              I_("<primary><alt><shift>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal-in-runtime");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workspace.new-terminal-in-runner",
                                              I_("<primary><alt>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal-in-runner");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             gbp_terminal_shortcut_entries,
                                             G_N_ELEMENTS (gbp_terminal_shortcut_entries),
                                             GETTEXT_PACKAGE);
}

static void
gbp_terminal_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  IdeWorkbench *workbench;
  IdeSurface *surface;
  GtkWidget *utilities;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace) ||
            IDE_IS_TERMINAL_WORKSPACE (workspace));

  self->workspace = workspace;

  gbp_terminal_workspace_addin_setup_shortcuts (self, workspace);
  g_action_map_add_action_entries (G_ACTION_MAP (workspace),
                                   terminal_actions,
                                   G_N_ELEMENTS (terminal_actions),
                                   self);

  if ((surface = ide_workspace_get_surface_by_name (workspace, "editor")) &&
      IDE_IS_EDITOR_SURFACE (surface) &&
      (utilities = ide_editor_surface_get_utilities (IDE_EDITOR_SURFACE (surface))))
    {
      IdeRunManager *run_manager;
      IdeContext *context;

      self->bottom_dock = g_object_new (DZL_TYPE_DOCK_WIDGET,
                                        "title", _("Terminal"),
                                        "icon-name", "utilities-terminal-symbolic",
                                        "visible", TRUE,
                                        NULL);
      g_signal_connect (self->bottom_dock,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->bottom_dock);
      gtk_container_add (GTK_CONTAINER (utilities), GTK_WIDGET (self->bottom_dock));

      self->bottom = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                                   "respawn-on-exit", TRUE,
                                   "visible", TRUE,
                                   NULL);
      bind_needs_attention (DZL_DOCK_ITEM (self->bottom_dock), self->bottom);
      g_signal_connect (self->bottom,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->bottom);
      gtk_container_add (GTK_CONTAINER (self->bottom_dock), GTK_WIDGET (self->bottom));

      workbench = ide_widget_get_workbench (GTK_WIDGET (workspace));

      if (ide_workbench_has_project (workbench))
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

  /* Add terminal popover button */
  if (IDE_IS_TERMINAL_WORKSPACE (workspace))
    {
      DzlMenuButton *menu_button;
      IdeHeaderBar *header_bar;
      GtkButton *button;
      GtkBox *box;

      box = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "hexpand", FALSE,
                          "visible", TRUE,
                          NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (box), "linked");

      button = g_object_new (GTK_TYPE_BUTTON,
                             "focus-on-click", FALSE,
                             "action-name", "win.new-terminal",
                             "child", g_object_new (GTK_TYPE_IMAGE,
                                                    "icon-name", "container-terminal-symbolic",
                                                    "visible", TRUE,
                                                    NULL),
                             "visible", TRUE,
                             NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (button), "image-button");
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (button));

      header_bar = ide_workspace_get_header_bar (workspace);
      ide_header_bar_add_primary (header_bar, GTK_WIDGET (box));

      self->popover = g_object_new (IDE_TYPE_TERMINAL_POPOVER,
                                    NULL);
      g_signal_connect (self->popover,
                        "destroy",
                        G_CALLBACK (gtk_widget_destroyed),
                        &self->popover);

      menu_button = g_object_new (DZL_TYPE_MENU_BUTTON,
                                  "focus-on-click", FALSE,
                                  "icon-name", "pan-down-symbolic",
                                  "popover", self->popover,
                                  "show-arrow", FALSE,
                                  "visible", TRUE,
                                  NULL);
      dzl_gtk_widget_add_style_class (GTK_WIDGET (menu_button), "image-button");
      dzl_gtk_widget_add_style_class (GTK_WIDGET (menu_button), "run-arrow-button");
      gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (menu_button));
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
            IDE_IS_EDITOR_WORKSPACE (workspace) ||
            IDE_IS_TERMINAL_WORKSPACE (workspace));

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

  if (self->popover != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->popover));

  for (guint i = 0; i < G_N_ELEMENTS (terminal_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (workspace), terminal_actions[i].name);

  if (self->bottom_dock != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->bottom_dock));

  if (self->run_panel != NULL)
    gtk_widget_destroy (GTK_WIDGET (self->run_panel));

  g_assert (self->bottom == NULL);
  g_assert (self->bottom_dock == NULL);

  g_assert (self->run_terminal == NULL);
  g_assert (self->run_panel == NULL);

  self->workspace = NULL;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_terminal_workspace_addin_load;
  iface->unload = gbp_terminal_workspace_addin_unload;
}

G_DEFINE_TYPE_WITH_CODE (GbpTerminalWorkspaceAddin, gbp_terminal_workspace_addin, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_WORKSPACE_ADDIN, workspace_addin_iface_init))

static void
gbp_terminal_workspace_addin_class_init (GbpTerminalWorkspaceAddinClass *klass)
{
}

static void
gbp_terminal_workspace_addin_init (GbpTerminalWorkspaceAddin *self)
{
}
