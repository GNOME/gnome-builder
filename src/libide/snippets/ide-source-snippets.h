/* ide-source-snippets.h
 *
 * Copyright (C) 2013 Christian Hergert <christian@hergert.me>
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

#include <gio/gio.h>

#include "ide-source-snippet.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPETS (ide_source_snippets_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippets, ide_source_snippets, IDE, SOURCE_SNIPPETS, GObject)

void               ide_source_snippets_add     (IdeSourceSnippets *snippets,
                                                IdeSourceSnippet  *snippet);
void               ide_source_snippets_clear   (IdeSourceSnippets *snippets);
void               ide_source_snippets_merge   (IdeSourceSnippets *snippets,
                                                IdeSourceSnippets *other);
guint              ide_source_snippets_count   (IdeSourceSnippets *self);
IdeSourceSnippets *ide_source_snippets_new     (void);
void               ide_source_snippets_foreach (IdeSourceSnippets *snippets,
                                                const gchar       *prefix,
                                                GFunc              foreach_func,
                                                gpointer           user_data);

G_END_DECLS
