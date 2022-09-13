/* gbp-shellcmd-search-result.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-shellcmd-search-result"

#include "config.h"

#include <glib/gi18n.h>

#include <libide-editor.h>
#include <libide-gui.h>
#include <libide-terminal.h>

#include "gbp-shellcmd-search-result.h"

struct _GbpShellcmdSearchResult
{
  IdeSearchResult        parent_instance;
  GbpShellcmdRunCommand *run_command;
};

G_DEFINE_FINAL_TYPE (GbpShellcmdSearchResult, gbp_shellcmd_search_result, IDE_TYPE_SEARCH_RESULT)

static void
gbp_shellcmd_search_result_activate (IdeSearchResult *result,
                                     GtkWidget       *last_focus)
{
  GbpShellcmdSearchResult *self = (GbpShellcmdSearchResult *)result;
  g_autoptr(IdeTerminalLauncher) launcher = NULL;
  g_autoptr(PanelPosition) position = NULL;
  IdeWorkspace *workspace;
  IdeContext *context;
  const char *title;
  IdePage *page;

  IDE_ENTRY;

  g_assert (IDE_IS_SEARCH_RESULT (result));
  g_assert (GTK_IS_WIDGET (last_focus));

  if (!(workspace = ide_widget_get_workspace (last_focus)) ||
      !(context = ide_workspace_get_context (workspace)))
    IDE_EXIT;

  if (!IDE_IS_PRIMARY_WORKSPACE (workspace) &&
      !IDE_IS_EDITOR_WORKSPACE (workspace))
    IDE_EXIT;

  if (!(title = ide_run_command_get_display_name (IDE_RUN_COMMAND (self->run_command))))
    title = _("Untitled command");

  launcher = ide_terminal_launcher_new (context, IDE_RUN_COMMAND (self->run_command));

  page = g_object_new (IDE_TYPE_TERMINAL_PAGE,
                       "close-on-exit", FALSE,
                       "icon-name", "text-x-script-symbolic",
                       "launcher", launcher,
                       "manage-spawn", TRUE,
                       "respawn-on-exit", FALSE,
                       "title", title,
                       NULL);

  position = panel_position_new ();

  ide_workspace_add_page (workspace, page, position);
  panel_widget_raise (PANEL_WIDGET (page));
  gtk_widget_grab_focus (GTK_WIDGET (page));

  IDE_EXIT;
}

static void
gbp_shellcmd_search_result_dispose (GObject *object)
{
  GbpShellcmdSearchResult *self = (GbpShellcmdSearchResult *)object;

  g_clear_object (&self->run_command);

  G_OBJECT_CLASS (gbp_shellcmd_search_result_parent_class)->dispose (object);
}

static void
gbp_shellcmd_search_result_class_init (GbpShellcmdSearchResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeSearchResultClass *search_result_class = IDE_SEARCH_RESULT_CLASS (klass);

  object_class->dispose = gbp_shellcmd_search_result_dispose;

  search_result_class->activate = gbp_shellcmd_search_result_activate;
}

static void
gbp_shellcmd_search_result_init (GbpShellcmdSearchResult *self)
{
}

/**
 * gbp_shellcmd_search_result_new:
 * @run_command: (transfer full): a #GbpShellcmdRunCommand
 * @gicon: the icon to use
 * @prio: the score of the match
 *
 * Returns: (transfer full): a new GbpShellcmdSearchResult
 */
GbpShellcmdSearchResult *
gbp_shellcmd_search_result_new (GbpShellcmdRunCommand *run_command,
                                GIcon                 *gicon,
                                guint                  prio)
{
  GbpShellcmdSearchResult *self;
  g_autofree char *subtitle = NULL;

  g_return_val_if_fail (GBP_IS_SHELLCMD_RUN_COMMAND (run_command), NULL);

  subtitle = gbp_shellcmd_run_command_dup_subtitle (run_command);

  self = g_object_new (GBP_TYPE_SHELLCMD_SEARCH_RESULT,
                       "title", ide_run_command_get_display_name (IDE_RUN_COMMAND (run_command)),
                       "subtitle", subtitle,
                       "gicon", gicon,
                       "score", (float)(prio > 0 ? 1. / prio : 0.),
                       NULL);

  self->run_command = g_steal_pointer (&run_command);

  return self;
}
