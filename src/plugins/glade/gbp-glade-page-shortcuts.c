/* gbp-glade-page-shortcuts.c
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

#define G_LOG_DOMAIN "gbp-glade-page-shortcuts"

#include "config.h"

#include <dazzle.h>
#include <glib/gi18n.h>

#include "gbp-glade-private.h"
#include "gbp-glade-page.h"

#define I_(s) (g_intern_static_string(s))

static DzlShortcutEntry glade_view_shortcuts[] = {
  { "org.gnome.builder.glade-view.save",
    0, NULL,
    N_("Glade shortcuts"),
    N_("Designer"),
    N_("Save the interface design") },

  { "org.gnome.builder.glade-view.preview",
    0, NULL,
    N_("Glade shortcuts"),
    N_("Designer"),
    N_("Preview the interface design") },

  { "org.gnome.builder.glade-view.undo",
    0, NULL,
    N_("Glade shortcuts"),
    N_("Designer"),
    N_("Undo the last command") },

  { "org.gnome.builder.glade-view.redo",
    0, NULL,
    N_("Glade shortcuts"),
    N_("Designer"),
    N_("Redo the next command") },
};

void
_gbp_glade_page_init_shortcuts (GtkWidget *widget)
{
  DzlShortcutController *controller;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  controller = dzl_shortcut_controller_find (widget);

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.save"),
                                              "<Primary>s",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.save"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.preview"),
                                              "<Control><Alt>p",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.preview"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.undo"),
                                              "<Control>z",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.undo"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.redo"),
                                              "<Control><Shift>z",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.redo"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.copy"),
                                              "<Primary>c",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.copy"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.cut"),
                                              "<Primary>x",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.cut"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.paste"),
                                              "<Primary>v",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.paste"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.glade-view.delete"),
                                              "Delete",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("glade-view.delete"));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             glade_view_shortcuts,
                                             G_N_ELEMENTS (glade_view_shortcuts),
                                             GETTEXT_PACKAGE);
}
