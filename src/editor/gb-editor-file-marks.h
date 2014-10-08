/* gb-editor-file-marks.h
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

#ifndef GB_EDITOR_FILE_MARKS_H
#define GB_EDITOR_FILE_MARKS_H

#include "gb-editor-file-mark.h"

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_FILE_MARKS            (gb_editor_file_marks_get_type())
#define GB_EDITOR_FILE_MARKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FILE_MARKS, GbEditorFileMarks))
#define GB_EDITOR_FILE_MARKS_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FILE_MARKS, GbEditorFileMarks const))
#define GB_EDITOR_FILE_MARKS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_FILE_MARKS, GbEditorFileMarksClass))
#define GB_IS_EDITOR_FILE_MARKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_FILE_MARKS))
#define GB_IS_EDITOR_FILE_MARKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_FILE_MARKS))
#define GB_EDITOR_FILE_MARKS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_FILE_MARKS, GbEditorFileMarksClass))

typedef struct _GbEditorFileMarks        GbEditorFileMarks;
typedef struct _GbEditorFileMarksClass   GbEditorFileMarksClass;
typedef struct _GbEditorFileMarksPrivate GbEditorFileMarksPrivate;

struct _GbEditorFileMarks
{
  GObject parent;

  /*< private >*/
  GbEditorFileMarksPrivate *priv;
};

struct _GbEditorFileMarksClass
{
  GObjectClass parent;
};

GType              gb_editor_file_marks_get_type     (void) G_GNUC_CONST;
GbEditorFileMarks *gb_editor_file_marks_new          (void);
GbEditorFileMarks *gb_editor_file_marks_get_default  (void);
GbEditorFileMark  *gb_editor_file_marks_get_for_file (GbEditorFileMarks    *marks,
                                                      GFile                *file);
gboolean           gb_editor_file_marks_load         (GbEditorFileMarks    *marks,
                                                      GError              **error);
void               gb_editor_file_marks_save_async   (GbEditorFileMarks    *marks,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean           gb_editor_file_marks_save_finish  (GbEditorFileMarks    *marks,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* GB_EDITOR_FILE_MARKS_H */
