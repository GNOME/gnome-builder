/* gb-editor-view.h
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

#ifndef GB_EDITOR_VIEW_H
#define GB_EDITOR_VIEW_H

#include <gtk/gtk.h>

#include "gb-document-view.h"
#include "gb-editor-document.h"
#include "gb-editor-frame.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_VIEW            (gb_editor_view_get_type())
#define GB_EDITOR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_VIEW, GbEditorView))
#define GB_EDITOR_VIEW_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_VIEW, GbEditorView const))
#define GB_EDITOR_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_VIEW, GbEditorViewClass))
#define GB_IS_EDITOR_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_VIEW))
#define GB_IS_EDITOR_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_VIEW))
#define GB_EDITOR_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_VIEW, GbEditorViewClass))

typedef struct _GbEditorView        GbEditorView;
typedef struct _GbEditorViewClass   GbEditorViewClass;
typedef struct _GbEditorViewPrivate GbEditorViewPrivate;

struct _GbEditorView
{
  GbDocumentView parent;

  /*< private >*/
  GbEditorViewPrivate *priv;
};

struct _GbEditorViewClass
{
  GbDocumentViewClass parent;
};

GType          gb_editor_view_get_type          (void);
GtkWidget     *gb_editor_view_new               (GbEditorDocument *document);
GbEditorFrame *gb_editor_view_get_frame1        (GbEditorView     *view);
GbEditorFrame *gb_editor_view_get_frame2        (GbEditorView     *view);
gboolean       gb_editor_view_get_split_enabled (GbEditorView     *view);
void           gb_editor_view_set_split_enabled (GbEditorView     *view,
                                                 gboolean          split_enabled);
gboolean       gb_editor_view_get_use_spaces    (GbEditorView     *view);
void           gb_editor_view_set_use_spaces    (GbEditorView     *view,
                                                 gboolean          use_spaces);

G_END_DECLS

#endif /* GB_EDITOR_VIEW_H */
