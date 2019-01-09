/* gb-beautifier-helper.h
 *
 * Copyright 2016 sebastien lafargue <slafargue@gnome.org>
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

#include <glib-object.h>

#include "libide-editor.h"

#include "gb-beautifier-config.h"
#include "gb-beautifier-editor-addin.h"

G_BEGIN_DECLS

void          gb_beautifier_helper_create_tmp_file_async          (GbBeautifierEditorAddin  *self,
                                                                   const gchar              *text,
                                                                   GAsyncReadyCallback       callback,
                                                                   GCancellable             *cancellable,
                                                                   gpointer                  user_data);
GFile        *gb_beautifier_helper_create_tmp_file_finish         (GbBeautifierEditorAddin  *self,
                                                                   GAsyncResult             *result,
                                                                   GError                  **error);

const gchar  *gb_beautifier_helper_get_lang_id                    (GbBeautifierEditorAddin  *self,
                                                                   IdeSourceView            *view);
gchar        *gb_beautifier_helper_match_and_replace              (const gchar              *str,
                                                                   const gchar              *pattern,
                                                                   const gchar              *replacement);
void          gb_beautifier_helper_config_entry_remove_temp_files (GbBeautifierEditorAddin  *self,
                                                                   GbBeautifierConfigEntry  *config_entry);
void          gb_beautifier_helper_remove_temp_for_path           (GbBeautifierEditorAddin  *self,
                                                                   const gchar              *path);
void          gb_beautifier_helper_remove_temp_for_file           (GbBeautifierEditorAddin  *self,
                                                                   GFile                    *file);

G_END_DECLS
