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
  GObject          parent_instance;

  IdeRunManager   *run_manager;
  gulong           run_manager_run_handler;
  gulong           run_manager_stopped_handler;

  IdeWorkspace    *workspace;

  IdePane         *app_pane;
  IdeTerminalPage *app_page;
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
  { "new-in-host", terminal_on_host_action, "s" },
  { "new-in-subprocess", terminal_as_subprocess_action, "s" },
  { "new-in-pipeline", terminal_in_pipeline_action, "s" },
  { "new-in-runtime", terminal_in_runtime_action, "s" },
})

static void
gbp_terminal_workspace_addin_add_page (GbpTerminalWorkspaceAddin *self,
                                       IdeTerminalRunLocality     locality,
                                       const char                *cwd)
{
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_autoptr(PanelPosition) position = NULL;
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
    position = panel_position_new ();

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

  gbp_terminal_workspace_addin_add_page (self, IDE_TERMINAL_RUN_IN_RUNTIME, cwd);
}

static void
on_run_manager_run (GbpTerminalWorkspaceAddin *self,
                    IdeRunContext             *run_context,
                    IdeRunManager             *run_manager)
{
  g_autoptr(GDateTime) now = NULL;
  g_autofree char *formatted = NULL;
  g_autofree char *tmp = NULL;
  g_autoptr(VtePty) pty = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUN_CONTEXT (run_context));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  pty = ide_pty_new_sync (NULL);

  ide_terminal_page_set_pty (self->app_page, pty);

  ide_run_context_push (run_context, NULL, NULL, NULL);
  ide_run_context_set_pty (run_context, pty);
  ide_run_context_setenv (run_context, "TERM", "xterm-256color");
  ide_run_context_setenv (run_context, "INSIDE_GNOME_BUILDER", PACKAGE_VERSION);

  now = g_date_time_new_now_local ();
  tmp = g_date_time_format (now, "%X");

  /* translators: %s is replaced with the current local time of day */
  formatted = g_strdup_printf (_("Application started at %s"), tmp);
  ide_terminal_page_feed (self->app_page, formatted);
  ide_terminal_page_feed (self->app_page, "\r\n");

  panel_widget_raise (PANEL_WIDGET (self->app_pane));

  IDE_EXIT;
}

static gboolean
message_from_idle_cb (gpointer data)
{
  GbpTerminalWorkspaceAddin *self = data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));

  if (self->app_page != NULL)
    {
      ide_terminal_page_feed (self->app_page, _("Application exited"));
      ide_terminal_page_feed (self->app_page, "\r\n");
    }

  return G_SOURCE_REMOVE;
}

static void
on_run_manager_stopped (GbpTerminalWorkspaceAddin *self,
                        IdeRunManager             *run_manager)
{
  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_RUN_MANAGER (run_manager));

  /* Wait to feed the widget until the main loop so that we
   * are more likely to finish flushing out contents from the
   * child PTY before we write our contents.
   */
  g_idle_add_full (G_PRIORITY_LOW + 1000,
                   message_from_idle_cb,
                   g_object_ref (self),
                   g_object_unref);

  IDE_EXIT;
}

static void
gbp_terminal_workspace_addin_load (IdeWorkspaceAddin *addin,
                                   IdeWorkspace      *workspace)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  g_autoptr(PanelPosition) position = NULL;
  IdeContext *context;
  IdePage *page;
  IdePane *pane;

  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  self->workspace = workspace;

  context = ide_workspace_get_context (workspace);

  /* Only allow activating runtime/pipeline terminals if we have a project
   * (and therefore a build pipeline we can use).
   */
  if (ide_context_has_project (context))
    {
      gbp_terminal_workspace_addin_set_action_enabled (self, "terminal-in-pipeline", TRUE);
      gbp_terminal_workspace_addin_set_action_enabled (self, "terminal-in-runtime", TRUE);
    }

  /* Always add the terminal panel to primary/editor workspaces */
  position = panel_position_new ();
  panel_position_set_area (position, PANEL_AREA_BOTTOM);
  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "respawn-on-exit", TRUE,
                       "visible", TRUE,
                       NULL);
  pane = g_object_new (IDE_TYPE_PANE,
                       "id", "terminal-bottom-panel",
                       "title", _("Terminal"),
                       "icon-name", "builder-terminal-symbolic",
                       "child", page,
                       NULL);
  ide_workspace_add_pane (workspace, pane, position);

  /* Setup panel for application output */
  if (IDE_IS_PRIMARY_WORKSPACE (workspace))
    {
      IdeRunManager *run_manager = ide_run_manager_from_context (context);
      VtePty *pty = ide_pty_new_sync (NULL);

      self->app_page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                                     "respawn-on-exit", FALSE,
                                     "manage-spawn", FALSE,
                                     "pty", pty,
                                     NULL);
      self->app_pane = g_object_new (IDE_TYPE_PANE,
                                     "id", "app-output-panel",
                                     "title", _("Application Output"),
                                     "icon-name", "builder-run-start-symbolic",
                                     "child", self->app_page,
                                     NULL);
      ide_workspace_add_pane (workspace, self->app_pane, position);

      g_set_object (&self->run_manager, run_manager);
      self->run_manager_run_handler =
        g_signal_connect_object (self->run_manager,
                                 "run",
                                 G_CALLBACK (on_run_manager_run),
                                 self,
                                 G_CONNECT_SWAPPED);
      self->run_manager_stopped_handler =
        g_signal_connect_object (self->run_manager,
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

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_PRIMARY_WORKSPACE (workspace) ||
            IDE_IS_EDITOR_WORKSPACE (workspace));

  if (self->run_manager)
    {
      g_clear_signal_handler (&self->run_manager_run_handler, self->run_manager);
      g_clear_signal_handler (&self->run_manager_stopped_handler, self->run_manager);
      g_clear_object (&self->run_manager);
    }

  self->app_page = NULL;
  g_clear_pointer ((PanelWidget **)&self->app_pane, panel_widget_close);

  self->workspace = NULL;
}

static void
gbp_terminal_workspace_addin_save_session_page_cb (IdePage  *page,
                                                   gpointer  user_data)
{
  IdeSession *session = user_data;

  g_assert (IDE_IS_PAGE (page));
  g_assert (IDE_IS_SESSION (session));

  if (IDE_IS_TERMINAL_PAGE (page))
    {
      g_autoptr(PanelPosition) position = ide_page_get_position (page);
      g_autoptr(IdeSessionItem) item = ide_session_item_new ();
      IdeTerminal *terminal = ide_terminal_page_get_terminal (IDE_TERMINAL_PAGE (page));
      const char *title = panel_widget_get_title (PANEL_WIDGET (page));
      g_autofree char *text = g_strchomp (vte_terminal_get_text_format (VTE_TERMINAL (terminal), VTE_FORMAT_TEXT));
      IdeWorkspace *workspace = ide_widget_get_workspace (GTK_WIDGET (page));
      const char *id = ide_workspace_get_id (workspace);
      int columns = vte_terminal_get_column_count (VTE_TERMINAL (terminal));
      int rows = vte_terminal_get_row_count (VTE_TERMINAL (terminal));
      g_autoptr(GString) text_with_suffix = g_string_new (text);

      if (!ide_terminal_page_has_exited (IDE_TERMINAL_PAGE (page)))
        {
          g_string_append (text_with_suffix, "\r\n");
          g_string_append (text_with_suffix, "\r\n");
          g_string_append_printf (text_with_suffix, "[%s]", _("Process completed"));
        }

      ide_session_item_set_module_name (item, "terminal");
      ide_session_item_set_type_hint (item, "IdeTerminalPage");
      ide_session_item_set_workspace (item, id);
      ide_session_item_set_position (item, position);
      ide_session_item_set_metadata (item, "title", "s", title);
      ide_session_item_set_metadata (item, "text", "s", text_with_suffix->str);
      ide_session_item_set_metadata (item, "size", "(ii)", columns, rows);

      if (page == ide_workspace_get_most_recent_page (workspace))
        ide_session_item_set_metadata (item, "has-focus", "b", TRUE);

      ide_session_append (session, item);
    }
}

static void
gbp_terminal_workspace_addin_save_session (IdeWorkspaceAddin *addin,
                                           IdeSession        *session)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  ide_workspace_foreach_page (self->workspace,
                              gbp_terminal_workspace_addin_save_session_page_cb,
                              session);

  IDE_EXIT;
}

static void
gbp_terminal_workspace_addin_restore_page (GbpTerminalWorkspaceAddin *self,
                                           IdeSessionItem            *item)
{
  g_autofree char *text = NULL;
  g_autofree char *title = NULL;
  IdeTerminalPage *page;
  PanelPosition *position;
  gboolean has_focus;
  int columns;
  int rows;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION_ITEM (item));

  if (!(position = ide_session_item_get_position (item)) ||
      !ide_session_item_get_metadata (item, "text", "s", &text) ||
      !ide_session_item_get_metadata (item, "title", "s", &title))
    return;

  if (!ide_session_item_get_metadata (item, "size", "(ii)", &columns, &rows))
    columns = rows = 0;

  page = ide_terminal_page_new_completed (title, text, columns, rows);
  ide_workspace_add_page (self->workspace, IDE_PAGE (page), position);

  if (ide_session_item_get_metadata (item, "has-focus", "b", &has_focus) && has_focus)
    {
      panel_widget_raise (PANEL_WIDGET (page));
      gtk_widget_grab_focus (GTK_WIDGET (page));
    }
}

static void
gbp_terminal_workspace_addin_restore_session_item (IdeWorkspaceAddin *addin,
                                                   IdeSession        *session,
                                                   IdeSessionItem    *item)
{
  GbpTerminalWorkspaceAddin *self = (GbpTerminalWorkspaceAddin *)addin;
  const char *type_hint;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_TERMINAL_WORKSPACE_ADDIN (self));
  g_assert (IDE_IS_SESSION (session));
  g_assert (IDE_IS_WORKSPACE (self->workspace));

  type_hint = ide_session_item_get_type_hint (item);

  if (ide_str_equal0 (type_hint, "IdeTerminalPage"))
    gbp_terminal_workspace_addin_restore_page (self, item);

  IDE_EXIT;
}

static void
workspace_addin_iface_init (IdeWorkspaceAddinInterface *iface)
{
  iface->load = gbp_terminal_workspace_addin_load;
  iface->unload = gbp_terminal_workspace_addin_unload;
  iface->save_session = gbp_terminal_workspace_addin_save_session;
  iface->restore_session_item = gbp_terminal_workspace_addin_restore_session_item;
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
  gbp_terminal_workspace_addin_set_action_enabled (self, "terminal-in-pipeline", FALSE);
  gbp_terminal_workspace_addin_set_action_enabled (self, "terminal-in-runtime", FALSE);
}
