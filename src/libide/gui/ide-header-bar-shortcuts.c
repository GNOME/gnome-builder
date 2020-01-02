/* ide-header-bar-shortcuts.c
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

#define G_LOG_DOMAIN "ide-header-bar-shortcuts"

#include "config.h"

#include <glib/gi18n.h>

#include "ide-gui-private.h"

#define I_(s) (g_intern_static_string(s))

static DzlShortcutEntry workspace_shortcuts[] = {
  { "org.gnome.builder.workspace.show-menu",
    0, NULL,
    N_("Window shortcuts"),
    N_("General"),
    N_("Show window menu") },

  { "org.gnome.builder.workspace.fullscreen",
    0, NULL,
    N_("Window shortcuts"),
    N_("General"),
    N_("Toggle window to fullscreen") },
};

void
_ide_header_bar_init_shortcuts (IdeHeaderBar *self)
{
  DzlShortcutController *controller;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.workspace.show-menu"),
                                              "F10",
                                              DZL_SHORTCUT_PHASE_BUBBLE | DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("win.show-menu"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.workspace.fullscreen"),
                                              "F11",
                                              DZL_SHORTCUT_PHASE_DISPATCH | DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("win.fullscreen"));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             workspace_shortcuts,
                                             G_N_ELEMENTS (workspace_shortcuts),
                                             GETTEXT_PACKAGE);
}
