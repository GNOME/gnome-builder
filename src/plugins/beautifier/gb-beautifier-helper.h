/* gb-beautifier-helper.h
 *
 * Copyright (C) 2016 sebastien lafargue <slafargue@gnome.org>
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

void          gb_beautifier_helper_create_tmp_file_async    (GbBeautifierEditorAddin  *self,
                                                             const gchar              *text,
                                                             GAsyncReadyCallback       callback,
                                                             GCancellable             *cancellable,
                                                             gpointer                  user_data);
GFile        *gb_beautifier_helper_create_tmp_file_finish   (GbBeautifierEditorAddin  *self,
                                                             GAsyncResult             *result,
                                                             GError                  **error);

const gchar  *gb_beautifier_helper_get_lang_id              (GbBeautifierEditorAddin  *self,
                                                             IdeSourceView            *view);
void          gb_beautifier_helper_remove_tmp_file          (GbBeautifierEditorAddin  *self,
                                                             GFile                    *tmp_file);

G_END_DECLS
