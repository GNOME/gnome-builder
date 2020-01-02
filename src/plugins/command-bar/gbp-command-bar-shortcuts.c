/* gbp-command-bar-shortcuts.c
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

#define G_LOG_DOMAIN "gbp-command-bar-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "gbp-command-bar-private.h"

#define I_(s) g_intern_static_string(s)

static const DzlShortcutEntry command_bar_shortcuts[] = {
  { "org.gnome.builder.command-bar.reveal",
    DZL_SHORTCUT_PHASE_GLOBAL | DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    N_("Workspace Shortcuts"),
    N_("Command Bar"),
    N_("Show the workspace command bar") },
};

void
_gbp_command_bar_init_shortcuts (GbpCommandBar *self)
{
  DzlShortcutController *controller;

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             command_bar_shortcuts,
                                             G_N_ELEMENTS (command_bar_shortcuts),
                                             GETTEXT_PACKAGE);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.command-bar.reveal"),
                                              I_("<Primary>Return"),
                                              DZL_SHORTCUT_PHASE_GLOBAL | DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("win.reveal-command-bar"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.command-bar.dismiss"),
                                              I_("Escape"),
                                              DZL_SHORTCUT_PHASE_CAPTURE,
                                              I_("win.dismiss-command-bar"));
}
