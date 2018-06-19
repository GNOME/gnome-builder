/* gb-terminal-workbench-addin.c
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
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

#include "gb-terminal-view.h"
#include "gb-terminal-workbench-addin.h"

#define I_(s) g_intern_static_string(s)

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

static IdeRuntime *
find_runtime (IdeWorkbench *workbench)
{
  IdeContext *context;
  IdeConfigurationManager *config_manager;
  IdeConfiguration *config;

  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);
  config_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (config_manager);

  return ide_configuration_get_runtime (config);
}

static gchar *
find_builddir (IdeWorkbench *workbench)
{
  IdeContext *context;
  IdeBuildManager *build_manager;
  IdeBuildPipeline *pipeline;
  const gchar *builddir = NULL;

  if ((context = ide_workbench_get_context (workbench)) &&
      (build_manager = ide_context_get_build_manager (context)) &&
      (pipeline = ide_build_manager_get_pipeline (build_manager)) &&
      (builddir = ide_build_pipeline_get_builddir (pipeline)) &&
      g_file_test (builddir, G_FILE_TEST_IS_DIR))
    return g_strdup (builddir);

  return NULL;
}

static void
new_terminal_activate (GSimpleAction *action,
                       GVariant      *param,
                       gpointer       user_data)
{
  GbTerminalWorkbenchAddin *self = user_data;
  g_autofree gchar *cwd = NULL;
  GbTerminalView *view;
  IdePerspective *perspective;
  IdeRuntime *runtime = NULL;
  const gchar *name;
  gboolean run_on_host = TRUE;
  gboolean use_runner = FALSE;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  name = g_action_get_name (G_ACTION (action));

  if (g_strcmp0 (name, "new-terminal-in-runtime") == 0)
    {
      runtime = find_runtime (self->workbench);
      cwd = find_builddir (self->workbench);
    }
  else if (g_strcmp0 (name, "debug-terminal") == 0)
    run_on_host = FALSE;

  if (g_strcmp0 (name, "new-terminal-in-runner") == 0)
    {
      runtime = find_runtime (self->workbench);
      use_runner = TRUE;
    }

  perspective = ide_workbench_get_perspective_by_name (self->workbench, "editor");
  ide_workbench_set_visible_perspective (self->workbench, perspective);

  if (g_strcmp0 (name, "new-terminal-in-dir") == 0)
    {
      IdeLayoutView *editor;

      editor = ide_editor_perspective_get_active_view (IDE_EDITOR_PERSPECTIVE (perspective));

      if (IDE_IS_EDITOR_VIEW (editor))
        {
          IdeBuffer *buffer;

          buffer = ide_editor_view_get_buffer (IDE_EDITOR_VIEW (editor));

          if (buffer != NULL)
            {
              IdeFile *file = ide_buffer_get_file (buffer);
              GFile *gfile = ide_file_get_file (file);
              g_autoptr(GFile) parent = g_file_get_parent (gfile);

              cwd = g_file_get_path (parent);
            }
        }
    }

  view = g_object_new (GB_TYPE_TERMINAL_VIEW,
                       "cwd", cwd,
                       "run-on-host", run_on_host,
                       "runtime", runtime,
                       "use-runner", use_runner,
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
  g_autoptr(GDateTime) now = NULL;
  g_autofree gchar *formatted = NULL;
  g_autofree gchar *tmp = NULL;

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
      dzl_set_weak_pointer (&self->run_terminal, view);

      panel = g_object_new (DZL_TYPE_DOCK_WIDGET,
                            "child", self->run_terminal,
                            "expand", TRUE,
                            "icon-name", "system-run-symbolic",
                            "title", _("Application Output"),
                            "visible", TRUE,
                            NULL);
      dzl_set_weak_pointer (&self->run_panel, panel);

      perspective = ide_workbench_get_perspective_by_name (self->workbench, "editor");
      g_assert (IDE_IS_EDITOR_PERSPECTIVE (perspective));

      bottom_pane = ide_editor_perspective_get_utilities (IDE_EDITOR_PERSPECTIVE (perspective));
      gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->run_panel));
    }
  else
    {
      gb_terminal_view_set_pty (self->run_terminal, pty);
    }

  if (-1 != (tty_fd = ide_vte_pty_create_slave (pty)))
    {
      ide_runner_set_tty (runner, tty_fd);
      close (tty_fd);
    }

  env = ide_runner_get_environment (runner);
  ide_environment_setenv (env, "TERM", "xterm-256color");
  ide_environment_setenv (env, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);

  now = g_date_time_new_now_local ();
  tmp = g_date_time_format (now, "%H:%M:%S");

  /* translators: %s is replaced with the current local time of day */
  formatted = g_strdup_printf (_("Application started at %s\r\n"), tmp);

  gb_terminal_view_feed (self->run_terminal, formatted);

  dzl_dock_item_present (DZL_DOCK_ITEM (self->run_panel));

failure:

  g_clear_object (&pty);

  IDE_EXIT;
}

static void
on_run_manager_stopped (GbTerminalWorkbenchAddin *self,
                        IdeRunManager            *run_manager)
{
  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  gb_terminal_view_feed (self->run_terminal, _("Application exited\r\n"));
}

static const DzlShortcutEntry gb_terminal_shortcut_entries[] = {
  { "org.gnome.builder.workbench.new-terminal",
    0, NULL,
    NC_("shortcut window", "Workbench shortcuts"),
    NC_("shortcut window", "General"),
    NC_("shortcut window", "Terminal") },

  { "org.gnome.builder.workbench.new-terminal-in-runtime",
    0, NULL,
    NC_("shortcut window", "Workbench shortcuts"),
    NC_("shortcut window", "General"),
    NC_("shortcut window", "Terminal in Build Runtime") },

  { "org.gnome.builder.workbench.new-terminal-in-runner",
    0, NULL,
    NC_("shortcut window", "Workbench shortcuts"),
    NC_("shortcut window", "General"),
    NC_("shortcut window", "Terminal in Runtime") },
};

static void
gb_terminal_workbench_setup_shortcuts (GbTerminalWorkbenchAddin *self,
                                       IdeWorkbench             *workbench)
{
  DzlShortcutController *controller;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (workbench));

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workbench.new-terminal",
                                              I_("<primary><shift>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workbench.new-terminal-in-runtime",
                                              I_("<primary><alt><shift>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal-in-runtime");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.workbench.new-terminal-in-runner",
                                              I_("<primary><alt>t"),
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              "win.new-terminal-in-runner");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             gb_terminal_shortcut_entries,
                                             G_N_ELEMENTS (gb_terminal_shortcut_entries),
                                             GETTEXT_PACKAGE);
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
  static const GActionEntry actions[] = {
    { "new-terminal", new_terminal_activate },
    { "new-terminal-in-runner", new_terminal_activate },
    { "new-terminal-in-runtime", new_terminal_activate },
    { "new-terminal-in-dir", new_terminal_activate },
    { "debug-terminal", new_terminal_activate },
  };

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));
  g_assert (IDE_IS_WORKBENCH (workbench));

  context = ide_workbench_get_context (workbench);

  dzl_set_weak_pointer (&self->workbench, workbench);

  g_action_map_add_action_entries (G_ACTION_MAP (workbench), actions, G_N_ELEMENTS (actions), self);
  gb_terminal_workbench_setup_shortcuts (self, workbench);

  if (self->panel_terminal == NULL)
    {
      self->panel_dock_widget = g_object_new (DZL_TYPE_DOCK_WIDGET,
                                              "expand", TRUE,
                                              "icon-name", "utilities-terminal-symbolic",
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
  g_assert (IDE_IS_EDITOR_PERSPECTIVE (perspective));

  bottom_pane = ide_editor_perspective_get_utilities (IDE_EDITOR_PERSPECTIVE (perspective));
  gtk_container_add (GTK_CONTAINER (bottom_pane), GTK_WIDGET (self->panel_dock_widget));

  run_manager = ide_context_get_run_manager (context);
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

static void
gb_terminal_workbench_addin_unload (IdeWorkbenchAddin *addin,
                                    IdeWorkbench      *workbench)
{
  GbTerminalWorkbenchAddin *self = (GbTerminalWorkbenchAddin *)addin;
  IdeRunManager *run_manager;
  IdeContext *context;

  g_assert (GB_IS_TERMINAL_WORKBENCH_ADDIN (self));

  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal");
  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal-in-runner");
  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal-in-runtime");
  g_action_map_remove_action (G_ACTION_MAP (self->workbench), "new-terminal-in-dir");

  context = ide_workbench_get_context (workbench);

  run_manager = ide_context_get_run_manager (context);
  g_signal_handlers_disconnect_by_func (run_manager,
                                        G_CALLBACK (on_run_manager_run),
                                        self);
  g_signal_handlers_disconnect_by_func (run_manager,
                                        G_CALLBACK (on_run_manager_stopped),
                                        self);

  if (self->panel_dock_widget != NULL)
    {
      gtk_widget_destroy (self->panel_dock_widget);
      dzl_clear_weak_pointer (&self->panel_dock_widget);
    }

  if (self->run_panel != NULL)
    {
      gtk_widget_destroy (self->run_panel);
      dzl_clear_weak_pointer (&self->run_panel);
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
