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

#include "gb-source-change-monitor.h"
#include "gb-source-code-assistant.h"

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

GbEditorDocument      *gb_editor_document_new                (void);
GType                  gb_editor_document_get_type           (void) G_GNUC_CONST;
GtkSourceFile         *gb_editor_document_get_file           (GbEditorDocument       *document);
void                   gb_editor_document_set_file           (GbEditorDocument       *document,
                                                              GtkSourceFile          *file);
GbSourceChangeMonitor *gb_editor_document_get_change_monitor (GbEditorDocument       *document);
GbSourceCodeAssistant *gb_editor_document_get_code_assistant (GbEditorDocument       *document);
gboolean               gb_editor_document_get_trim_trailing_whitespace (GbEditorDocument *document);
void                   gb_editor_document_set_trim_trailing_whitespace (GbEditorDocument *document,
                                                                        gboolean          trim_trailing_whitespace);
void                   gb_editor_document_load_async         (GbEditorDocument       *document,
                                                              GFile                  *file,
                                                              GCancellable           *cancellable,
                                                              GFileProgressCallback   progress_callback,
                                                              gpointer                progress_data,
                                                              GDestroyNotify          progress_data_notify,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
gboolean               gb_editor_document_load_finish        (GbEditorDocument       *document,
                                                              GAsyncResult           *result,
                                                              GError                **error);
void                   gb_editor_document_save_async         (GbEditorDocument       *document,
                                                              GCancellable           *cancellable,
                                                              GFileProgressCallback   progress_callback,
                                                              gpointer                progress_data,
                                                              GDestroyNotify          progress_data_notify,
                                                              GAsyncReadyCallback     callback,
                                                              gpointer                user_data);
gboolean               gb_editor_document_save_finish        (GbEditorDocument       *document,
                                                              GAsyncResult           *result,
                                                              GError                **error);
void                   gb_editor_document_reformat           (GbEditorDocument       *document);

G_END_DECLS

#endif /* GB_EDITOR_DOCUMENT_H */
