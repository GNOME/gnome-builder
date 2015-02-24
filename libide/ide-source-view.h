/* ide-source-view.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IDE_SOURCE_VIEW_H
#define IDE_SOURCE_VIEW_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_SOURCE_VIEW            (ide_source_view_get_type())
#define IDE_SOURCE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView))
#define IDE_SOURCE_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_SOURCE_VIEW, IdeSourceView const))
#define IDE_SOURCE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))
#define IDE_IS_SOURCE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_SOURCE_VIEW))
#define IDE_IS_SOURCE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_SOURCE_VIEW))
#define IDE_SOURCE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_SOURCE_VIEW, IdeSourceViewClass))

typedef struct _IdeSourceView      IdeSourceView;
typedef struct _IdeSourceViewClass IdeSourceViewClass;

struct _IdeSourceView
{
  GtkSourceView parent;
};

struct _IdeSourceViewClass
{
  GtkSourceViewClass parent_class;
};

GType ide_source_view_get_type (void);

void                        ide_source_view_set_font_name (IdeSourceView              *self,
                                                           const gchar                *font_name);
const PangoFontDescription *ide_source_view_get_font_desc (IdeSourceView              *self);
void                        ide_source_view_set_font_desc (IdeSourceView              *self,
                                                           const PangoFontDescription *font_desc);

G_END_DECLS

#endif /* IDE_SOURCE_VIEW_H */
