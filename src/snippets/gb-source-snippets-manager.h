/* gb-source-snippets-manager.h
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

#ifndef GB_SOURCE_SNIPPETS_MANAGER_H
#define GB_SOURCE_SNIPPETS_MANAGER_H

#include <gtksourceview/gtksourcelanguage.h>

#include "gb-source-snippets.h"
#include "gb-source-snippet.h"
#include "gb-source-snippet-parser.h"

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_SNIPPETS_MANAGER            (gb_source_snippets_manager_get_type())
#define GB_SOURCE_SNIPPETS_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPETS_MANAGER, GbSourceSnippetsManager))
#define GB_SOURCE_SNIPPETS_MANAGER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_SNIPPETS_MANAGER, GbSourceSnippetsManager const))
#define GB_SOURCE_SNIPPETS_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_SNIPPETS_MANAGER, GbSourceSnippetsManagerClass))
#define GB_IS_SOURCE_SNIPPETS_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_SNIPPETS_MANAGER))
#define GB_IS_SOURCE_SNIPPETS_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_SNIPPETS_MANAGER))
#define GB_SOURCE_SNIPPETS_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_SNIPPETS_MANAGER, GbSourceSnippetsManagerClass))

typedef struct _GbSourceSnippetsManager        GbSourceSnippetsManager;
typedef struct _GbSourceSnippetsManagerClass   GbSourceSnippetsManagerClass;
typedef struct _GbSourceSnippetsManagerPrivate GbSourceSnippetsManagerPrivate;

struct _GbSourceSnippetsManager
{
  GObject parent;

  /*< private >*/
  GbSourceSnippetsManagerPrivate *priv;
};

struct _GbSourceSnippetsManagerClass
{
  GObjectClass parent_class;
};

GType                    gb_source_snippets_manager_get_type         (void);
GbSourceSnippetsManager *gb_source_snippets_manager_get_default      (void);
GbSourceSnippets        *gb_source_snippets_manager_get_for_language (GbSourceSnippetsManager *manager,
                                                                      GtkSourceLanguage       *language);

G_END_DECLS

#endif /* GB_SOURCE_SNIPPETS_MANAGER_H */
