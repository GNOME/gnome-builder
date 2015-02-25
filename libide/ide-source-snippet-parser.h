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

#define IDE_TYPE_SOURCE_SNIPPET_PARSER            (ide_source_snippet_parser_get_type())
#define IDE_SOURCE_SNIPPET_PARSER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPET_PARSER, IdeSourceSnippetParser))
#define IDE_SOURCE_SNIPPET_PARSER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPET_PARSER, IdeSourceSnippetParser const))
#define IDE_SOURCE_SNIPPET_PARSER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_SNIPPET_PARSER, IdeSourceSnippetParserClass))
#define IDE_IS_SOURCE_SNIPPET_PARSER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_SNIPPET_PARSER))
#define IDE_IS_SOURCE_SNIPPET_PARSER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_SNIPPET_PARSER))
#define IDE_SOURCE_SNIPPET_PARSER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_SNIPPET_PARSER, IdeSourceSnippetParserClass))

typedef struct _IdeSourceSnippetParser        IdeSourceSnippetParser;
typedef struct _IdeSourceSnippetParserClass   IdeSourceSnippetParserClass;
typedef struct _IdeSourceSnippetParserPrivate IdeSourceSnippetParserPrivate;

struct _IdeSourceSnippetParser
{
  GObject parent;

  /*< private >*/
  IdeSourceSnippetParserPrivate *priv;
};

struct _IdeSourceSnippetParserClass
{
  GObjectClass parent_class;
};

GType                  ide_source_snippet_parser_get_type       (void);
IdeSourceSnippetParser *ide_source_snippet_parser_new            (void);
gboolean               ide_source_snippet_parser_load_from_file (IdeSourceSnippetParser  *parser,
                                                                GFile                  *file,
                                                                GError                **error);
GList                 *ide_source_snippet_parser_get_snippets   (IdeSourceSnippetParser  *parser);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPET_PARSER_H */
