/* ide-frame-shortcuts.c
 *
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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


#include "config.h"

#include <glib/gi18n.h>

#include "ide-frame.h"
#include "ide-gui-private.h"

#define I_(s) g_intern_static_string(s)

static const DzlShortcutEntry frame_shortcuts[] = {
  { "org.gnome.builder.frame.move-right",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Move document to the right") },

  { "org.gnome.builder.frame.move-left",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Move document to the left") },

  { "org.gnome.builder.frame.previous-document",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Switch to the previous document") },

  { "org.gnome.builder.frame.next-document",
    DZL_SHORTCUT_PHASE_CAPTURE,
    NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Switch to the next document") },

  { "org.gnome.builder.frame.close-page",
    DZL_SHORTCUT_PHASE_BUBBLE,
    NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Close the document") },
};

void
_ide_frame_init_shortcuts (IdeFrame *self)
{
  DzlShortcutController *controller;

  g_return_if_fail (IDE_IS_FRAME (self));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             frame_shortcuts,
                                             G_N_ELEMENTS (frame_shortcuts),
                                             GETTEXT_PACKAGE);

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.frame.move-right"),
                                              I_("<Primary><Alt>Page_Down"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("frame.move-right"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.frame.move-left"),
                                              I_("<Primary><Alt>Page_Up"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("frame.move-left"));

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.frame.next-document"),
                                              I_("<Primary><Shift>Page_Down"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("change-current-page"),
                                              1, G_TYPE_INT, 1);

  dzl_shortcut_controller_add_command_signal (controller,
                                              I_("org.gnome.builder.frame.previous-document"),
                                              I_("<Primary><Shift>Page_Up"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("change-current-page"),
                                              1, G_TYPE_INT, -1);

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.frame.close-page"),
                                              I_("<Primary>w"),
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("frame.close-page"));
}
