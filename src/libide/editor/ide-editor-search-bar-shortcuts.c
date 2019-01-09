/* ide-editor-search-bar-shortcuts.c
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

#define G_LOG_DOMAIN "ide-editor-search-bar-shortcuts"

#include "config.h"

#include "ide-editor-private.h"
#include "ide-editor-search-bar.h"

static void
ide_editor_search_bar_shortcuts_activate_previous (GtkWidget *widget,
                                                   gpointer   user_data)
{
  IdeEditorSearchBar *self = user_data;
  IdeEditorSearch *search;

  g_assert (GTK_IS_WIDGET (widget));
  g_assert (IDE_IS_EDITOR_SEARCH_BAR (self));

  search = ide_editor_search_bar_get_search (self);

  if (search != NULL)
    {
      ide_editor_search_move (search, IDE_EDITOR_SEARCH_PREVIOUS);
      g_signal_emit_by_name (self, "stop-search");
    }
}

void
_ide_editor_search_bar_init_shortcuts (IdeEditorSearchBar *self)
{
  DzlShortcutController *controller;

  controller = dzl_shortcut_controller_find (GTK_WIDGET (self));

  dzl_shortcut_controller_add_command_callback (controller,
                                                "org.gnome.builder.editor.search-bar.activate-previous",
                                                "<Shift>Return",
                                                DZL_SHORTCUT_PHASE_BUBBLE,
                                                ide_editor_search_bar_shortcuts_activate_previous,
                                                self, NULL);

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor.search-bar.move-next",
                                              "Down",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "editor-search.move-next");

  dzl_shortcut_controller_add_command_action (controller,
                                              "org.gnome.builder.editor.search-bar.move-previous",
                                              "Up",
                                              DZL_SHORTCUT_PHASE_BUBBLE,
                                              "editor-search.move-previous");
}
