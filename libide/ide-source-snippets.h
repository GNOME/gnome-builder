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

#ifndef IDE_SOURCE_SNIPPETS_H
#define IDE_SOURCE_SNIPPETS_H

#include <gio/gio.h>

#include "ide-source-snippet.h"

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_SNIPPETS            (ide_source_snippets_get_type())
#define IDE_SOURCE_SNIPPETS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPETS, IdeSourceSnippets))
#define IDE_SOURCE_SNIPPETS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_SNIPPETS, IdeSourceSnippets const))
#define IDE_SOURCE_SNIPPETS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_SNIPPETS, IdeSourceSnippetsClass))
#define IDE_IS_SOURCE_SNIPPETS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_SNIPPETS))
#define IDE_IS_SOURCE_SNIPPETS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_SNIPPETS))
#define IDE_SOURCE_SNIPPETS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_SNIPPETS, IdeSourceSnippetsClass))

typedef struct _IdeSourceSnippets        IdeSourceSnippets;
typedef struct _IdeSourceSnippetsClass   IdeSourceSnippetsClass;
typedef struct _IdeSourceSnippetsPrivate IdeSourceSnippetsPrivate;

struct _IdeSourceSnippets
{
  GObject parent;

  /*< private >*/
  IdeSourceSnippetsPrivate *priv;
};

struct _IdeSourceSnippetsClass
{
  GObjectClass parent_class;
};

void              ide_source_snippets_add            (IdeSourceSnippets *snippets,
                                                     IdeSourceSnippet  *snippet);
void              ide_source_snippets_clear          (IdeSourceSnippets *snippets);
void              ide_source_snippets_merge          (IdeSourceSnippets *snippets,
                                                     IdeSourceSnippets *other);
IdeSourceSnippets *ide_source_snippets_new            (void);
GType             ide_source_snippets_get_type       (void);
void              ide_source_snippets_foreach        (IdeSourceSnippets *snippets,
                                                     const gchar      *prefix,
                                                     GFunc             foreach_func,
                                                     gpointer          user_data);

G_END_DECLS

#endif /* IDE_SOURCE_SNIPPETS_H */
