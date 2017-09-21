/* ide-source-snippet-parser.h
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

#ifndef IDE_SOURCE_SNIPPET_PARSER_H
#define IDE_SOURCE_SNIPPET_PARSER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPET_PARSER (ide_source_snippet_parser_get_type())

G_DECLARE_FINAL_TYPE (IdeSourceSnippetParser, ide_source_snippet_parser,
                      IDE, SOURCE_SNIPPET_PARSER, GObject)

IdeSourceSnippetParser *ide_source_snippet_parser_new            (void);
gboolean                ide_source_snippet_parser_load_from_file (IdeSourceSnippetParser  *parser,
                                                                  GFile                   *file,
                                                                  GError                 **error);
GList                  *ide_source_snippet_parser_get_snippets   (IdeSourceSnippetParser  *parser);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPET_PARSER_H */
