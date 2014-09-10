/* gb-source-snippet-context.h
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

#ifndef GB_SOURCE_SNIPPET_CONTEXT_H
#define GB_SOURCE_SNIPPET_CONTEXT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SNIPPET_CONTEXT            (gb_source_snippet_context_get_type())
#define GB_SOURCE_SNIPPET_CONTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET_CONTEXT, GbSourceSnippetContext))
#define GB_SOURCE_SNIPPET_CONTEXT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPET_CONTEXT, GbSourceSnippetContext const))
#define GB_SOURCE_SNIPPET_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SNIPPET_CONTEXT, GbSourceSnippetContextClass))
#define GB_IS_SOURCE_SNIPPET_CONTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SNIPPET_CONTEXT))
#define GB_IS_SOURCE_SNIPPET_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SNIPPET_CONTEXT))
#define GB_SOURCE_SNIPPET_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SNIPPET_CONTEXT, GbSourceSnippetContextClass))

typedef struct _GbSourceSnippetContext        GbSourceSnippetContext;
typedef struct _GbSourceSnippetContextClass   GbSourceSnippetContextClass;
typedef struct _GbSourceSnippetContextPrivate GbSourceSnippetContextPrivate;

struct _GbSourceSnippetContext
{
  GObject parent;

  /*< private >*/
  GbSourceSnippetContextPrivate *priv;
};

struct _GbSourceSnippetContextClass
{
  GObjectClass parent_class;
};

GType                   gb_source_snippet_context_get_type        (void) G_GNUC_CONST;
GbSourceSnippetContext *gb_source_snippet_context_new             (void);
void                    gb_source_snippet_context_emit_changed    (GbSourceSnippetContext *context);
void                    gb_source_snippet_context_clear_variables (GbSourceSnippetContext *context);
void                    gb_source_snippet_context_add_variable    (GbSourceSnippetContext *context,
                                                                   const gchar            *key,
                                                                   const gchar            *value);
const gchar            *gb_source_snippet_context_get_variable    (GbSourceSnippetContext *context,
                                                                   const gchar            *key);
gchar                  *gb_source_snippet_context_expand          (GbSourceSnippetContext *context,
                                                                   const gchar            *input);
void                    gb_source_snippet_context_set_tab_width   (GbSourceSnippetContext *context,
                                                                   gint                    tab_size);
void                    gb_source_snippet_context_set_use_spaces  (GbSourceSnippetContext *context,
                                                                   gboolean                use_spaces);
void                    gb_source_snippet_context_set_line_prefix (GbSourceSnippetContext *context,
                                                                   const gchar            *line_prefix);
void                    gb_source_snippet_context_dump            (GbSourceSnippetContext *context);

G_END_DECLS

#endif /* GB_SOURCE_SNIPPET_CONTEXT_H */
