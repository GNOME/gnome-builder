/* ide-editor-page-private.h
 *
 * Copyright 2017-2022 Christian Hergert <chergert@redhat.com>
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

#include <libide-plugins.h>

#include "ide-editor-page.h"

G_BEGIN_DECLS

struct _IdeEditorPage
{
  IdePage                  parent_instance;

  /* Owned references */
  IdeExtensionSetAdapter  *addins;
  IdeBuffer               *buffer;

  /* Settings Management */
  IdeBindingGroup         *buffer_file_settings;
  IdeBindingGroup         *view_file_settings;

  /* Template widgets */
  IdeSourceView           *view;
  GtkScrolledWindow       *scroller;
  GtkSourceMap            *map;
  GtkRevealer             *map_revealer;

  guint                    completion_blocked : 1;
};

void _ide_editor_page_class_actions_init (IdeEditorPageClass *klass);
void _ide_editor_page_settings_init      (IdeEditorPage *self);
void _ide_editor_page_settings_reload    (IdeEditorPage *self);

G_END_DECLS
