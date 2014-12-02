/* gb-source-code-assistant-renderer.h
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

#ifndef GB_SOURCE_CODE_ASSISTANT_RENDERER_H
#define GB_SOURCE_CODE_ASSISTANT_RENDERER_H

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER            (gb_source_code_assistant_renderer_get_type())
#define GB_SOURCE_CODE_ASSISTANT_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER, GbSourceCodeAssistantRenderer))
#define GB_SOURCE_CODE_ASSISTANT_RENDERER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER, GbSourceCodeAssistantRenderer const))
#define GB_SOURCE_CODE_ASSISTANT_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER, GbSourceCodeAssistantRendererClass))
#define GB_IS_SOURCE_CODE_ASSISTANT_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER))
#define GB_IS_SOURCE_CODE_ASSISTANT_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER))
#define GB_SOURCE_CODE_ASSISTANT_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_CODE_ASSISTANT_RENDERER, GbSourceCodeAssistantRendererClass))

typedef struct _GbSourceCodeAssistantRenderer        GbSourceCodeAssistantRenderer;
typedef struct _GbSourceCodeAssistantRendererClass   GbSourceCodeAssistantRendererClass;
typedef struct _GbSourceCodeAssistantRendererPrivate GbSourceCodeAssistantRendererPrivate;

struct _GbSourceCodeAssistantRenderer
{
  GtkSourceGutterRendererPixbuf parent;

  /*< private >*/
  GbSourceCodeAssistantRendererPrivate *priv;
};

struct _GbSourceCodeAssistantRendererClass
{
  GtkSourceGutterRendererPixbufClass parent_class;
};

GType gb_source_code_assistant_renderer_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GB_SOURCE_CODE_ASSISTANT_RENDERER_H */
