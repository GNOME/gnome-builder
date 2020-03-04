/* ide-greeter-workspace-shortcuts.c
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

#define G_LOG_DOMAIN "ide-greeter-workspace-shortcuts"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-greeter-private.h"
#include "ide-greeter-workspace.h"

#define I_(s) (g_intern_static_string(s))

void
_ide_greeter_workspace_init_shortcuts (IdeGreeterWorkspace *self)
{
  DzlShortcutController *controller;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.greeter.close"),
                                              "<Primary>w",
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              I_("win.close"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.greeter.back-button"),
                                              "<alt>Left",
                                              DZL_SHORTCUT_PHASE_DISPATCH,
                                              I_("win.surface('sections')"));
}
