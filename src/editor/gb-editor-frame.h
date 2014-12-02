/* gb-editor-frame.h
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

#ifndef GB_EDITOR_FRAME_H
#define GB_EDITOR_FRAME_H

#include <gtk/gtk.h>

#include "gb-editor-document.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_FRAME            (gb_editor_frame_get_type())
#define GB_EDITOR_FRAME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FRAME, GbEditorFrame))
#define GB_EDITOR_FRAME_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FRAME, GbEditorFrame const))
#define GB_EDITOR_FRAME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_FRAME, GbEditorFrameClass))
#define GB_IS_EDITOR_FRAME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_FRAME))
#define GB_IS_EDITOR_FRAME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_FRAME))
#define GB_EDITOR_FRAME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_FRAME, GbEditorFrameClass))

typedef struct _GbEditorFrame        GbEditorFrame;
typedef struct _GbEditorFrameClass   GbEditorFrameClass;
typedef struct _GbEditorFramePrivate GbEditorFramePrivate;

struct _GbEditorFrame
{
  GtkOverlay parent;

  /*< private >*/
  GbEditorFramePrivate *priv;
};

struct _GbEditorFrameClass
{
  GtkOverlayClass parent;
};

GType             gb_editor_frame_get_type     (void);
GtkWidget        *gb_editor_frame_new          (void);
void              gb_editor_frame_link         (GbEditorFrame    *src,
                                                GbEditorFrame    *dst);
GbEditorDocument *gb_editor_frame_get_document (GbEditorFrame    *frame);
void              gb_editor_frame_set_document (GbEditorFrame    *frame,
                                                GbEditorDocument *document);
void              gb_editor_frame_find         (GbEditorFrame    *frame,
                                                const gchar      *search_text);
void              gb_editor_frame_reformat     (GbEditorFrame    *frame);

G_END_DECLS

#endif /* GB_EDITOR_FRAME_H */
