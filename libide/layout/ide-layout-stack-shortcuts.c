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

static const DzlShortcutEntry stack_shortcuts[] = {
  { "org.gnome.builder.layoutstack.move-right",
    NULL,
    N_("Editing"),
    N_("Navigation"),
    N_("Move document right"),
    N_("Move the document to the frame on the right") },

  { "org.gnome.builder.layoutstack.move-left",
    NULL,
    N_("Editing"),
    N_("Navigation"),
    N_("Move document left"),
    N_("Move the document to the frame on the left") },

  { "org.gnome.builder.layoutstack.previous-document",
    NULL,
    N_("Editing"),
    N_("Navigation"),
    N_("Focus next document"),
    N_("Focus the next document in the stack") },

  { "org.gnome.builder.layoutstack.previous-document",
    NULL,
    N_("Editing"),
    N_("Navigation"),
    N_("Focus next document"),
    N_("Focus the next document in the stack") },

  { "org.gnome.builder.layoutstack.close-view",
    NULL,
    N_("Editing"),
    N_("Navigation"),
    N_("Close current view"),
    N_("Closes the currently focused view") },
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
                                              "org.gnome.builder.layoutstack.move-right",
                                              "<Control><Alt>Page_Down",
                                              "layoutstack.move-right");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.layoutstack.move-left",
                                              "<Control><Alt>Page_Up",
                                              "layoutstack.move-left");

  dzl_shortcut_controller_add_command_signal (controller,
                                              "org.gnome.builder.layoutstack.next-document",
                                              "<Control><Shift>Page_Down",
                                              "change-current-page", 1,
                                              G_TYPE_INT, 1);

  dzl_shortcut_controller_add_command_signal (controller,
                                              "org.gnome.builder.layoutstack.previous-document",
                                              "<Control><Shift>Page_Up",
                                              "change-current-page", 1,
                                              G_TYPE_INT, -1);

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.layoutstack.close-view",
                                              "<Control>w",
                                              "layoutstack.close-view");
}
