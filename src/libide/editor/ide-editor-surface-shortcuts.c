/* ide-editor-surface-shortcuts.c
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

#define G_LOG_DOMAIN "ide-editor-surface-shortcuts"

#include "config.h"

#include <glib/gi18n.h>
#include <dazzle.h>

#include "ide-editor-private.h"

#define I_(s) g_intern_static_string(s)

static const DzlShortcutEntry editor_surface_entries[] = {
  { "org.gnome.builder.editor.new-file",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Create a new document") },

  { "org.gnome.builder.editor.open-file",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Open a document") },

  { "org.gnome.builder.editor.navigation-panel",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Panels"),
    N_("Toggle navigation panel") },

  { "org.gnome.builder.editor.utilities-panel",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Panels"),
    N_("Toggle utilities panel") },

  { "org.gnome.builder.editor.close-all",
    0, NULL,
    N_("Editor shortcuts"),
    N_("Files"),
    N_("Close all files") },
};

void
_ide_editor_surface_init_shortcuts (IdeEditorSurface *self)
{
  DzlShortcutController *controller;

  g_assert (IDE_IS_EDITOR_SURFACE (self));

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.new-file"),
                                              I_("<Primary>n"),
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("editor.new-file"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.open-file"),
                                              I_("<Primary>o"),
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("editor.open-file"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.navigation-panel"),
                                              I_("F9"),
                                              DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("dockbin.left-visible"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.utilities-panel"),
                                              I_("<Control>F9"),
                                              DZL_SHORTCUT_PHASE_CAPTURE | DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("dockbin.bottom-visible"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.close-all"),
                                              I_("<Primary><Shift>w"),
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              I_("editor.close-all"));

  dzl_shortcut_controller_add_command_action (controller,
                                              I_("org.gnome.builder.editor.focus"),
                                              "<alt>1",
                                              DZL_SHORTCUT_PHASE_GLOBAL,
                                              "win.surface('editor')");

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             editor_surface_entries,
                                             G_N_ELEMENTS (editor_surface_entries),
                                             GETTEXT_PACKAGE);
}
