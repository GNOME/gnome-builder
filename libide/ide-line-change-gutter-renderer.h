/* ide-line-change-gutter-renderer.h
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

#ifndef IDE_LINE_CHANGE_GUTTER_RENDERER_H
#define IDE_LINE_CHANGE_GUTTER_RENDERER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER            (ide_line_change_gutter_renderer_get_type())
#define IDE_LINE_CHANGE_GUTTER_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER, IdeLineChangeGutterRenderer))
#define IDE_LINE_CHANGE_GUTTER_RENDERER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER, IdeLineChangeGutterRenderer const))
#define IDE_LINE_CHANGE_GUTTER_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER, IdeLineChangeGutterRendererClass))
#define IDE_IS_LINE_CHANGE_GUTTER_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER))
#define IDE_IS_LINE_CHANGE_GUTTER_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER))
#define IDE_LINE_CHANGE_GUTTER_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  IDE_TYPE_LINE_CHANGE_GUTTER_RENDERER, IdeLineChangeGutterRendererClass))

typedef struct _IdeLineChangeGutterRenderer      IdeLineChangeGutterRenderer;
typedef struct _IdeLineChangeGutterRendererClass IdeLineChangeGutterRendererClass;

GType ide_line_change_gutter_renderer_get_type (void);

G_END_DECLS

#endif /* IDE_LINE_CHANGE_GUTTER_RENDERER_H */
