/* ide-snippet-parser.h
 *
 * Copyright 2013-2019 Christian Hergert <christian@hergert.me>
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

#if !defined (IDE_SOURCEVIEW_INSIDE) && !defined (IDE_SOURCEVIEW_COMPILATION)
# error "Only <libide-sourceview.h> can be included directly."
#endif

#include <gio/gio.h>

#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_PARSER (ide_snippet_parser_get_type())

IDE_AVAILABLE_IN_3_32
G_DECLARE_FINAL_TYPE (IdeSnippetParser, ide_snippet_parser, IDE, SNIPPET_PARSER, GObject)

IDE_AVAILABLE_IN_3_32
IdeSnippetParser *ide_snippet_parser_new            (void);
IDE_AVAILABLE_IN_3_32
gboolean          ide_snippet_parser_load_from_data (IdeSnippetParser  *parser,
                                                     const gchar       *defalut_language,
                                                     const gchar       *data,
                                                     gssize             data_len,
                                                     GError           **error);
IDE_AVAILABLE_IN_3_32
gboolean          ide_snippet_parser_load_from_file (IdeSnippetParser  *parser,
                                                     GFile             *file,
                                                     GError           **error);
IDE_AVAILABLE_IN_3_32
GList            *ide_snippet_parser_get_snippets   (IdeSnippetParser  *parser);
IDE_AVAILABLE_IN_3_36
IdeSnippet       *ide_snippet_parser_parse_one      (const char        *data,
                                                     gssize             data_len,
                                                     GError           **error);

G_END_DECLS
