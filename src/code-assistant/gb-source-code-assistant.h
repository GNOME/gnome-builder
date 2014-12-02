/* gb-source-code-assistant.h
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

#ifndef GB_SOURCE_CODE_ASSISTANT_H
#define GB_SOURCE_CODE_ASSISTANT_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GB_TYPE_SOURCE_CODE_ASSISTANT            (gb_source_code_assistant_get_type())
#define GB_SOURCE_CODE_ASSISTANT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT, GbSourceCodeAssistant))
#define GB_SOURCE_CODE_ASSISTANT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT, GbSourceCodeAssistant const))
#define GB_SOURCE_CODE_ASSISTANT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_SOURCE_CODE_ASSISTANT, GbSourceCodeAssistantClass))
#define GB_IS_SOURCE_CODE_ASSISTANT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_SOURCE_CODE_ASSISTANT))
#define GB_IS_SOURCE_CODE_ASSISTANT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_SOURCE_CODE_ASSISTANT))
#define GB_SOURCE_CODE_ASSISTANT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_SOURCE_CODE_ASSISTANT, GbSourceCodeAssistantClass))

typedef struct _GbSourceCodeAssistant        GbSourceCodeAssistant;
typedef struct _GbSourceCodeAssistantClass   GbSourceCodeAssistantClass;
typedef struct _GbSourceCodeAssistantPrivate GbSourceCodeAssistantPrivate;

struct _GbSourceCodeAssistant
{
  GObject parent;

  /*< private >*/
  GbSourceCodeAssistantPrivate *priv;
};

struct _GbSourceCodeAssistantClass
{
  GObjectClass parent_class;

  void (*changed) (GbSourceCodeAssistant *assistant);
};

GType                  gb_source_code_assistant_get_type        (void) G_GNUC_CONST;
GbSourceCodeAssistant *gb_source_code_assistant_new             (GtkTextBuffer         *buffer);
GArray                *gb_source_code_assistant_get_diagnostics (GbSourceCodeAssistant *assistant);

G_END_DECLS

#endif /* GB_SOURCE_CODE_ASSISTANT_H */
