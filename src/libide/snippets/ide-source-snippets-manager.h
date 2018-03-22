/* ide-source-snippets-manager.h
 *
 * Copyright 2013 Christian Hergert <christian@hergert.me>
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

#include <gtksourceview/gtksource.h>

#include "ide-version-macros.h"

#include "ide-object.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPETS_MANAGER (ide_source_snippets_manager_get_type())

IDE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (IdeSourceSnippetsManager, ide_source_snippets_manager, IDE, SOURCE_SNIPPETS_MANAGER, GObject)

IDE_AVAILABLE_IN_ALL
void               ide_source_snippets_manager_load_async          (IdeSourceSnippetsManager  *self,
                                                                    GCancellable              *cancellable,
                                                                    GAsyncReadyCallback        callback,
                                                                    gpointer                   user_data);
IDE_AVAILABLE_IN_ALL
gboolean           ide_source_snippets_manager_load_finish         (IdeSourceSnippetsManager  *self,
                                                                    GAsyncResult              *result,
                                                                    GError                   **error);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippets *ide_source_snippets_manager_get_for_language    (IdeSourceSnippetsManager  *self,
                                                                    GtkSourceLanguage         *language);
IDE_AVAILABLE_IN_ALL
IdeSourceSnippets *ide_source_snippets_manager_get_for_language_id (IdeSourceSnippetsManager  *self,
                                                                    const gchar               *language_id);

G_END_DECLS
