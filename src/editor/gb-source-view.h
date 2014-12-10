/* gb-source-view.h
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

#ifndef GB_SOURCE_VIEW_H
#define GB_SOURCE_VIEW_H

#include <gtksourceview/gtksourceview.h>

#include "gb-source-auto-indenter.h"
#include "gb-source-snippet.h"
#include "gb-source-vim.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_VIEW            (gb_source_view_get_type())
#define GB_SOURCE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIEW, GbSourceView))
#define GB_SOURCE_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_VIEW, GbSourceView const))
#define GB_SOURCE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_VIEW, GbSourceViewClass))
#define GB_IS_SOURCE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_VIEW))
#define GB_IS_SOURCE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_VIEW))
#define GB_SOURCE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_VIEW, GbSourceViewClass))

typedef struct _GbSourceView        GbSourceView;
typedef struct _GbSourceViewClass   GbSourceViewClass;
typedef struct _GbSourceViewPrivate GbSourceViewPrivate;

struct _GbSourceView
{
  GtkSourceView parent;

  /*< private >*/
  GbSourceViewPrivate *priv;
};

struct _GbSourceViewClass
{
  GtkSourceViewClass parent_class;

  void (*push_snippet) (GbSourceView           *view,
                        GbSourceSnippet        *snippet,
                        GbSourceSnippetContext *context,
                        GtkTextIter            *location);
  void (*pop_snippet)  (GbSourceView           *view,
                        GbSourceSnippet        *snippet);
  void (*begin_search) (GbSourceView           *view,
                        GtkDirectionType        direction,
                        const gchar            *search_text);
  void (*draw_layer)   (GbSourceView           *view,
                        GtkTextViewLayer        layer,
                        cairo_t                *cr);
  void (*request_documentation) (GbSourceView           *view);
  void (*display_documentation) (GbSourceView           *view,
                                 const gchar            *search_text);
};

void                  gb_source_view_begin_search       (GbSourceView         *view,
                                                         GtkDirectionType      direction,
                                                         const gchar          *search_text);
void                  gb_source_view_clear_snippets     (GbSourceView         *view);
GbSourceAutoIndenter *gb_source_view_get_auto_indenter  (GbSourceView         *view);
gboolean              gb_source_view_get_show_shadow    (GbSourceView         *view);
GType                 gb_source_view_get_type           (void) G_GNUC_CONST;
void                  gb_source_view_indent_selection   (GbSourceView         *view);
void                  gb_source_view_push_snippet       (GbSourceView         *view,
                                                         GbSourceSnippet      *snippet);
void                  gb_source_view_set_font_name      (GbSourceView         *view,
                                                         const gchar          *font_name);
void                  gb_source_view_set_show_shadow    (GbSourceView         *view,
                                                         gboolean              show_shadow);
void                  gb_source_view_unindent_selection (GbSourceView         *view);
GbSourceVim          *gb_source_view_get_vim            (GbSourceView         *view);

G_END_DECLS

#endif /* GB_SOURCE_VIEW_H */
