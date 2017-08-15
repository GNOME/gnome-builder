/* ide-layout-stack-shortcuts.c
 *
 * Copyright (C) 2017 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <glib/gi18n.h>

#include "ide-layout-stack.h"
#include "ide-layout-private.h"

#define I_(s) g_intern_static_string(s)

static const DzlShortcutEntry stack_shortcuts[] = {
  { "org.gnome.builder.layoutstack.move-right",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Files"),
    NC_("shortcut window", "Move document to the right") },

  { "org.gnome.builder.layoutstack.move-left",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Files"),
    NC_("shortcut window", "Move document to the left") },

  { "org.gnome.builder.layoutstack.previous-document",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Files"),
    NC_("shortcut window", "Switch to the previous document") },

  { "org.gnome.builder.layoutstack.next-document",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Files"),
    NC_("shortcut window", "Switch to the next document") },

  { "org.gnome.builder.layoutstack.close-view",
    DZL_SHORTCUT_PHASE_BUBBLE,
    NULL,
    NC_("shortcut window", "Editor shortcuts"),
    NC_("shortcut window", "Files"),
    NC_("shortcut window", "Close the document") },
};

void
_ide_layout_stack_init_shortcuts (IdeLayoutStack *self)
{
  DzlShortcutController *controller;

  g_return_if_fail (IDE_IS_LAYOUT_STACK (self));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             stack_shortcuts,
                                             G_N_ELEMENTS (stack_shortcuts),
                                             GETTEXT_PACKAGE);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.layoutstack.move-right"),
                                              I_("<Primary><Alt>Page_Down"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("layoutstack.move-right"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.layoutstack.move-left"),
                                              I_("<Primary><Alt>Page_Up"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("layoutstack.move-left"));

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.layoutstack.next-document"),
                                              I_("<Primary><Shift>Page_Down"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("change-current-page"),
                                              1, G_TYPE_INT, 1);

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.layoutstack.previous-document"),
                                              I_("<Primary><Shift>Page_Up"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("change-current-page"),
                                              1, G_TYPE_INT, -1);

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.layoutstack.close-view"),
                                              I_("<Primary>w"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("layoutstack.close-view"));
}
