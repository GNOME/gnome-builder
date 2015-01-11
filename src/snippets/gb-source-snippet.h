/* gb-source-snippet.h
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

#ifndef GB_SOURCE_SNIPPET_H
#define GB_SOURCE_SNIPPET_H

#include <gtk/gtk.h>

#include "gb-source-snippet-context.h"
#include "gb-source-snippet-chunk.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SNIPPET            (gb_source_snippet_get_type())
#define GB_SOURCE_SNIPPET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET, GbSourceSnippet))
#define GB_SOURCE_SNIPPET_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET, GbSourceSnippet const))
#define GB_SOURCE_SNIPPET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SNIPPET, GbSourceSnippetClass))
#define GB_IS_SOURCE_SNIPPET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SNIPPET))
#define GB_IS_SOURCE_SNIPPET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SNIPPET))
#define GB_SOURCE_SNIPPET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SNIPPET, GbSourceSnippetClass))

typedef struct _GbSourceSnippet        GbSourceSnippet;
typedef struct _GbSourceSnippetClass   GbSourceSnippetClass;
typedef struct _GbSourceSnippetPrivate GbSourceSnippetPrivate;

struct _GbSourceSnippet
{
  GObject parent;

  /*< private >*/
  GbSourceSnippetPrivate *priv;
};

struct _GbSourceSnippetClass
{
  GObjectClass parent_class;
};

GbSourceSnippet        *gb_source_snippet_new             (const gchar          *trigger,
                                                           const gchar          *language);
GbSourceSnippet        *gb_source_snippet_copy            (GbSourceSnippet      *snippet);
GType                   gb_source_snippet_get_type        (void);
const gchar            *gb_source_snippet_get_trigger     (GbSourceSnippet      *snippet);
void                    gb_source_snippet_set_trigger     (GbSourceSnippet      *snippet,
                                                           const gchar          *trigger);
const gchar            *gb_source_snippet_get_language    (GbSourceSnippet      *snippet);
void                    gb_source_snippet_set_language    (GbSourceSnippet      *snippet,
                                                           const gchar          *language);
const gchar            *gb_source_snippet_get_description (GbSourceSnippet      *snippet);
void                    gb_source_snippet_set_description (GbSourceSnippet      *snippet,
                                                           const gchar          *description);
void                    gb_source_snippet_add_chunk       (GbSourceSnippet      *snippet,
                                                           GbSourceSnippetChunk *chunk);
guint                   gb_source_snippet_get_n_chunks    (GbSourceSnippet      *snippet);
gint                    gb_source_snippet_get_tab_stop    (GbSourceSnippet      *snippet);
GbSourceSnippetChunk   *gb_source_snippet_get_nth_chunk   (GbSourceSnippet      *snippet,
                                                           guint                 n);
void                    gb_source_snippet_get_chunk_range (GbSourceSnippet      *snippet,
                                                           GbSourceSnippetChunk *chunk,
                                                           GtkTextIter          *begin,
                                                           GtkTextIter          *end);
GtkTextMark            *gb_source_snippet_get_mark_begin  (GbSourceSnippet      *snippet);
GtkTextMark            *gb_source_snippet_get_mark_end    (GbSourceSnippet      *snippet);
GbSourceSnippetContext *gb_source_snippet_get_context     (GbSourceSnippet      *snippet);

G_END_DECLS

#endif /* GB_SOURCE_SNIPPET_H */
