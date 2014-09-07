/* gb-source-search-highlighter.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
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

#ifndef GB_SOURCE_SEARCH_HIGHLIGHTER_H
#define GB_SOURCE_SEARCH_HIGHLIGHTER_H

#include <gtksourceview/gtksource.h>

#include "gb-source-view.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER            (gb_source_search_highlighter_get_type())
#define GB_SOURCE_SEARCH_HIGHLIGHTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER, GbSourceSearchHighlighter))
#define GB_SOURCE_SEARCH_HIGHLIGHTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER, GbSourceSearchHighlighter const))
#define GB_SOURCE_SEARCH_HIGHLIGHTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER, GbSourceSearchHighlighterClass))
#define GB_IS_SOURCE_SEARCH_HIGHLIGHTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER))
#define GB_IS_SOURCE_SEARCH_HIGHLIGHTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER))
#define GB_SOURCE_SEARCH_HIGHLIGHTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SEARCH_HIGHLIGHTER, GbSourceSearchHighlighterClass))

typedef struct _GbSourceSearchHighlighter        GbSourceSearchHighlighter;
typedef struct _GbSourceSearchHighlighterClass   GbSourceSearchHighlighterClass;
typedef struct _GbSourceSearchHighlighterPrivate GbSourceSearchHighlighterPrivate;

struct _GbSourceSearchHighlighter
{
  GObject parent;

  /*< private >*/
  GbSourceSearchHighlighterPrivate *priv;
};

struct _GbSourceSearchHighlighterClass
{
  GObjectClass parent_class;
};

GType                      gb_source_search_highlighter_get_type            (void) G_GNUC_CONST;
GbSourceSearchHighlighter *gb_source_search_highlighter_new                 (GtkSourceView             *source_view);
void                       gb_source_search_highlighter_set_search_context  (GbSourceSearchHighlighter *highlighter,
                                                                             GtkSourceSearchContext    *search_context);
void                       gb_source_search_highlighter_set_search_settings (GbSourceSearchHighlighter *highlighter,
                                                                             GtkSourceSearchSettings   *search_settings);
void                       gb_source_search_highlighter_draw                (GbSourceSearchHighlighter *highlighter,
                                                                             cairo_t                   *cr);

G_END_DECLS

#endif /* GB_SOURCE_SEARCH_HIGHLIGHTER_H */
