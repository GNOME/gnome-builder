/* ide-workspace-actions.c
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

#define G_LOG_DOMAIN "ide-workspace-actions"

#include "config.h"

#include <libide-commands.h>

#include "ide-gui-global.h"
#include "ide-workspace-private.h"

static void
ide_workspace_actions_close (GSimpleAction *action,
                             GVariant      *param,
                             gpointer       user_data)
{
  IdeWorkspace *self = user_data;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (IDE_IS_WORKSPACE (self));

  gtk_window_close (GTK_WINDOW (self));
}

static void
ide_workspace_actions_command (GSimpleAction *action,
                               GVariant      *param,
                               gpointer       user_data)
{
  IdeCommandManager *command_manager;
  IdeWorkspace *self = user_data;
  const gchar *command;
  IdeContext *context;

  g_assert (G_IS_SIMPLE_ACTION (action));
  g_assert (param != NULL);
  g_assert (g_variant_is_of_type (param, G_VARIANT_TYPE_STRING));
  g_assert (IDE_IS_WORKSPACE (self));

  command = g_variant_get_string (param, NULL);
  context = ide_widget_get_context (GTK_WIDGET (self));
  command_manager = ide_command_manager_from_context (context);

  ide_command_manager_execute (command_manager, GTK_WIDGET (self), command);
}

static const GActionEntry actions[] = {
  { "close", ide_workspace_actions_close },
  { "command", ide_workspace_actions_command, "s" },
};

void
_ide_workspace_init_actions (IdeWorkspace *self)
{
  g_return_if_fail (IDE_IS_WORKSPACE (self));

  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}
