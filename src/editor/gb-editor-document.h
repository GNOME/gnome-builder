/* gb-editor-document.h
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

#ifndef GB_EDITOR_DOCUMENT_H
#define GB_EDITOR_DOCUMENT_H

#include <gtksourceview/gtksourcebuffer.h>

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_DOCUMENT            (gb_editor_document_get_type())
#define GB_EDITOR_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_DOCUMENT, GbEditorDocument))
#define GB_EDITOR_DOCUMENT_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_DOCUMENT, GbEditorDocument const))
#define GB_EDITOR_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_DOCUMENT, GbEditorDocumentClass))
#define GB_IS_EDITOR_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_DOCUMENT))
#define GB_IS_EDITOR_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_DOCUMENT))
#define GB_EDITOR_DOCUMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_DOCUMENT, GbEditorDocumentClass))

typedef struct _GbEditorDocument        GbEditorDocument;
typedef struct _GbEditorDocumentClass   GbEditorDocumentClass;
typedef struct _GbEditorDocumentPrivate GbEditorDocumentPrivate;

struct _GbEditorDocument
{
  GtkSourceBuffer parent;

  /*< private >*/
  GbEditorDocumentPrivate *priv;
};

struct _GbEditorDocumentClass
{
  GtkSourceBufferClass parent_class;

  void (*cursor_moved) (GbEditorDocument *document);
};

GbEditorDocument *gb_editor_document_new            (void);
GType             gb_editor_document_get_type       (void) G_GNUC_CONST;
GFile            *gb_editor_document_get_file       (GbEditorDocument      *document);
void              gb_editor_document_set_file       (GbEditorDocument      *document,
                                                     GFile                 *file);
void              gb_editor_document_load_async     (GbEditorDocument      *document,
                                                     GFile                 *file,
                                                     GCancellable          *cancellable,
                                                     GFileProgressCallback  progress_callback,
                                                     gpointer               progress_callback_data,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data);
gboolean          gb_editor_document_load_finish    (GbEditorDocument      *document,
                                                     GAsyncResult          *result,
                                                     GError               **error);
void              gb_editor_document_save_async     (GbEditorDocument      *document,
                                                     GCancellable          *cancellable,
                                                     GFileProgressCallback  progress_callback,
                                                     gpointer               progress_callback_data,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data);
gboolean          gb_editor_document_save_finish    (GbEditorDocument      *document,
                                                     GAsyncResult          *result,
                                                     GError               **error);
void              gb_editor_document_save_as_async  (GbEditorDocument      *document,
                                                     GFile                 *file,
                                                     GCancellable          *cancellable,
                                                     GFileProgressCallback  progress_callback,
                                                     gpointer               progress_callback_data,
                                                     GAsyncReadyCallback    callback,
                                                     gpointer               user_data);
gboolean          gb_editor_document_save_as_finish (GbEditorDocument      *document,
                                                     GAsyncResult          *result,
                                                     GError               **error);

G_END_DECLS

#endif /* GB_EDITOR_DOCUMENT_H */
