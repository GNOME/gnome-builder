/* gb-source-snippets.h
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

#ifndef GB_SOURCE_SNIPPETS_H
#define GB_SOURCE_SNIPPETS_H

#include <gio/gio.h>

#include "gb-source-snippet.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SNIPPETS            (gb_source_snippets_get_type())
#define GB_SOURCE_SNIPPETS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPETS, GbSourceSnippets))
#define GB_SOURCE_SNIPPETS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPETS, GbSourceSnippets const))
#define GB_SOURCE_SNIPPETS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SNIPPETS, GbSourceSnippetsClass))
#define GB_IS_SOURCE_SNIPPETS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SNIPPETS))
#define GB_IS_SOURCE_SNIPPETS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SNIPPETS))
#define GB_SOURCE_SNIPPETS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SNIPPETS, GbSourceSnippetsClass))

typedef struct _GbSourceSnippets        GbSourceSnippets;
typedef struct _GbSourceSnippetsClass   GbSourceSnippetsClass;
typedef struct _GbSourceSnippetsPrivate GbSourceSnippetsPrivate;

struct _GbSourceSnippets
{
  GObject parent;

  /*< private >*/
  GbSourceSnippetsPrivate *priv;
};

struct _GbSourceSnippetsClass
{
  GObjectClass parent_class;
};

void              gb_source_snippets_add            (GbSourceSnippets *snippets,
                                                     GbSourceSnippet  *snippet);
void              gb_source_snippets_clear          (GbSourceSnippets *snippets);
void              gb_source_snippets_merge          (GbSourceSnippets *snippets,
                                                     GbSourceSnippets *other);
GbSourceSnippets *gb_source_snippets_new            (void);
GType             gb_source_snippets_get_type       (void);
void              gb_source_snippets_foreach        (GbSourceSnippets *snippets,
                                                     const gchar      *prefix,
                                                     GFunc             foreach_func,
                                                     gpointer          user_data);

G_END_DECLS

#endif /* GB_SOURCE_SNIPPETS_H */
