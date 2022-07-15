/* ide-search-bar.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <libide-gtk.h>
#include <libide-sourceview.h>

G_BEGIN_DECLS

#define IDE_TYPE_EDITOR_SEARCH_BAR (ide_editor_search_bar_get_type())

typedef enum
{
  IDE_EDITOR_SEARCH_BAR_MODE_SEARCH,
  IDE_EDITOR_SEARCH_BAR_MODE_REPLACE,
} IdeEditorSearchBarMode;

G_DECLARE_FINAL_TYPE (IdeEditorSearchBar, ide_editor_search_bar, IDE, EDITOR_SEARCH_BAR, GtkWidget)

struct _IdeEditorSearchBar
{
  GtkWidget                parent_instance;

  GtkSourceSearchContext  *context;
  GtkSourceSearchSettings *settings;

  GtkGrid                 *grid;
  IdeSearchEntry          *search_entry;
  GtkEntry                *replace_entry;
  GtkButton               *replace_button;
  GtkButton               *replace_all_button;
  GtkToggleButton         *replace_mode_button;

  guint                    offset_when_shown;

  guint                    can_move : 1;
  guint                    can_replace : 1;
  guint                    can_replace_all : 1;
  guint                    hide_after_move : 1;
  guint                    scroll_to_first_match : 1;
  guint                    jump_back_on_hide : 1;
};

void     _ide_editor_search_bar_attach              (IdeEditorSearchBar     *self,
                                                     IdeBuffer              *buffer);
void     _ide_editor_search_bar_detach              (IdeEditorSearchBar     *self);
void     _ide_editor_search_bar_set_mode            (IdeEditorSearchBar     *self,
                                                     IdeEditorSearchBarMode  mode);
void     _ide_editor_search_bar_move_next           (IdeEditorSearchBar     *self,
                                                     gboolean                hide_after_move);
void     _ide_editor_search_bar_move_previous       (IdeEditorSearchBar     *self,
                                                     gboolean                hide_after_move);
gboolean _ide_editor_search_bar_get_can_move        (IdeEditorSearchBar     *self);
gboolean _ide_editor_search_bar_get_can_replace     (IdeEditorSearchBar     *self);
gboolean _ide_editor_search_bar_get_can_replace_all (IdeEditorSearchBar     *self);
void     _ide_editor_search_bar_replace             (IdeEditorSearchBar     *self);
void     _ide_editor_search_bar_replace_all         (IdeEditorSearchBar     *self);
void     _ide_editor_search_bar_grab_focus          (IdeEditorSearchBar     *self);

G_END_DECLS
