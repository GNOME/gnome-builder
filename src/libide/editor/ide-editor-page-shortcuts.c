/* ide-editor-page-shortcuts.c
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

#include <dazzle.h>
#include <glib/gi18n.h>

#include "ide-editor-private.h"

#define I_(s) (g_intern_static_string(s))

static DzlShortcutEntry editor_view_shortcuts[] = {
  { "org.gnome.builder.editor-page.save",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Save the document") },

  { "org.gnome.builder.editor-page.save-as",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Save the document with a new name") },

  { "org.gnome.builder.editor-page.print",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Print the document") },

  { "org.gnome.builder.editor-page.find",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Find") },

  { "org.gnome.builder.editor-page.find-replace",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Find and replace") },

  { "org.gnome.builder.editor-page.next-match",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Move to the next match") },

  { "org.gnome.builder.editor-page.prev-match",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Move to the previous match") },

  { "org.gnome.builder.editor-page.next-error",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Move to the next error") },

  { "org.gnome.builder.editor-page.prev-error",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Find and replace"),
    N_("Move to the previous error") },
};

void
_ide_editor_page_init_shortcuts (IdeEditorPage *self)
{
  DzlShortcutController *controller;

  g_return_if_fail (IDE_IS_EDITOR_PAGE (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.find"),
                                              "<Primary>f",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.find"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.find-replace"),
                                              "<Primary>h",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.find-replace"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.next-match"),
                                              "<Primary>g",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.move-next-search-result"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.prev-match"),
                                              "<Primary><Shift>g",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.move-previous-search-result"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.next-error"),
                                              "<alt>n",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.move-next-error"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.prev-error"),
                                              "<alt>p",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.move-previous-error"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.save"),
                                              "<Primary>s",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.save"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.save-as"),
                                              "<Primary><Shift>s",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.save-as"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor-page.print"),
                                              "<Primary>p",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              I_("editor-page.print"));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             editor_view_shortcuts,
                                             G_N_ELEMENTS (editor_view_shortcuts),
                                             GETTEXT_PACKAGE);
}
