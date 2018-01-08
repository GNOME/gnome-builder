/* gb-beautifier-private.h
 *
 * Copyright © 2016 sebastien lafargue <slafargue@gnome.org>
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

#pragma once

#include <glib-object.h>

#include "ide.h"
#include "gb-beautifier-editor-addin.h"

G_BEGIN_DECLS

struct _GbBeautifierEditorAddin
{
  GObject                parent_instance;

  IdeContext            *context;
  IdeEditorPerspective  *editor;
  IdeLayoutView         *current_view;
  GArray                *entries;

  gchar                 *tmp_dir;

  gboolean               has_default;
};

GbBeautifierEditorAddin    *gb_beautifier_editor_addin_get_editor_perspective    (GbBeautifierEditorAddin *self);

G_END_DECLS
