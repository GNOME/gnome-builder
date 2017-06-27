/* ide-editor-view-shortcuts.c
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

#include <dazzle.h>

#include "ide-editor-private.h"

static DzlShortcutEntry editor_view_shortcuts[] = {
};

void
_ide_editor_view_init_shortcuts (IdeEditorView *self)
{
  g_return_if_fail (IDE_IS_EDITOR_VIEW (self));

  dzl_shortcut_manager_add_shortcut_entries (NULL,
                                             editor_view_shortcuts,
                                             G_N_ELEMENTS (editor_view_shortcuts),
                                             GETTEXT_PACKAGE);
}
