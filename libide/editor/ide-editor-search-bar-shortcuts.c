/* ide-editor-search-bar-shortcuts.c
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

#define G_LOG_DOMAIN "ide-editor-search-bar-shortcuts"

#include "editor/ide-editor-private.h"
#include "editor/ide-editor-search-bar.h"

void
_ide_editor_search_bar_init_shortcuts (IdeEditorSearchBar *self)
{
  DzlShortcutController *controller;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor.search-bar.move-next",
                                              "Down",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "editor-view.move-next-search-result");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor.search-bar.move-previous",
                                              "Up",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "editor-view.move-previous-search-result");
}
