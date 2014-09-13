/* gb-source-change-gutter-renderer.h
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GB_SOURCE_CHANGE_GUTTER_RENDERER_H
#define GB_SOURCE_CHANGE_GUTTER_RENDERER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER            (gb_source_change_gutter_renderer_get_type())
#define GB_SOURCE_CHANGE_GUTTER_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER, GbSourceChangeGutterRenderer))
#define GB_SOURCE_CHANGE_GUTTER_RENDERER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER, GbSourceChangeGutterRenderer const))
#define GB_SOURCE_CHANGE_GUTTER_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER, GbSourceChangeGutterRendererClass))
#define GB_IS_SOURCE_CHANGE_GUTTER_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER))
#define GB_IS_SOURCE_CHANGE_GUTTER_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER))
#define GB_SOURCE_CHANGE_GUTTER_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_CHANGE_GUTTER_RENDERER, GbSourceChangeGutterRendererClass))

typedef struct _GbSourceChangeGutterRenderer        GbSourceChangeGutterRenderer;
typedef struct _GbSourceChangeGutterRendererClass   GbSourceChangeGutterRendererClass;
typedef struct _GbSourceChangeGutterRendererPrivate GbSourceChangeGutterRendererPrivate;

struct _GbSourceChangeGutterRenderer
{
  GtkSourceGutterRenderer parent;

  /*< private >*/
  GbSourceChangeGutterRendererPrivate *priv;
};

struct _GbSourceChangeGutterRendererClass
{
  GtkSourceGutterRendererClass parent_class;
};

GType gb_source_change_gutter_renderer_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_SOURCE_CHANGE_GUTTER_RENDERER_H */
