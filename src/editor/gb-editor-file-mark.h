/* gb-editor-file-mark.h
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

#ifndef GB_EDITOR_FILE_MARK_H
#define GB_EDITOR_FILE_MARK_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GB_TYPE_EDITOR_FILE_MARK            (gb_editor_file_mark_get_type())
#define GB_EDITOR_FILE_MARK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FILE_MARK, GbEditorFileMark))
#define GB_EDITOR_FILE_MARK_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GB_TYPE_EDITOR_FILE_MARK, GbEditorFileMark const))
#define GB_EDITOR_FILE_MARK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GB_TYPE_EDITOR_FILE_MARK, GbEditorFileMarkClass))
#define GB_IS_EDITOR_FILE_MARK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GB_TYPE_EDITOR_FILE_MARK))
#define GB_IS_EDITOR_FILE_MARK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GB_TYPE_EDITOR_FILE_MARK))
#define GB_EDITOR_FILE_MARK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GB_TYPE_EDITOR_FILE_MARK, GbEditorFileMarkClass))

typedef struct _GbEditorFileMark        GbEditorFileMark;
typedef struct _GbEditorFileMarkClass   GbEditorFileMarkClass;
typedef struct _GbEditorFileMarkPrivate GbEditorFileMarkPrivate;

struct _GbEditorFileMark
{
  GObject parent;

  /*< private >*/
  GbEditorFileMarkPrivate *priv;
};

struct _GbEditorFileMarkClass
{
  GObjectClass parent;
};

GType             gb_editor_file_mark_get_type   (void);
GbEditorFileMark *gb_editor_file_mark_new        (GFile            *file,
                                                  guint             line,
                                                  guint             column);
guint             gb_editor_file_mark_get_column (GbEditorFileMark *mark);
guint             gb_editor_file_mark_get_line   (GbEditorFileMark *mark);
GFile            *gb_editor_file_mark_get_file   (GbEditorFileMark *mark);
void              gb_editor_file_mark_set_column (GbEditorFileMark *mark,
                                                  guint             column);
void              gb_editor_file_mark_set_line   (GbEditorFileMark *mark,
                                                  guint             line);
void              gb_editor_file_mark_set_file   (GbEditorFileMark *mark,
                                                  GFile            *file);

G_END_DECLS

#endif /* GB_EDITOR_FILE_MARK_H */
