/* gb-editor-file-marks.c
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

#include <errno.h>

#include "gb-editor-file-marks.h"
#include "gb-string.h"

struct _GbEditorFileMarksPrivate
{
  GHashTable *marks;
};

G_DEFINE_TYPE_WITH_PRIVATE (GbEditorFileMarks, gb_editor_file_marks, G_TYPE_OBJECT)

GbEditorFileMarks *
gb_editor_file_marks_new (void)
{
  return g_object_new (GB_TYPE_EDITOR_FILE_MARKS, NULL);
}

GbEditorFileMarks *
gb_editor_file_marks_get_default (void)
{
  static GbEditorFileMarks *instance;

  if (!instance)
    instance = gb_editor_file_marks_new ();

  return instance;
}

static GFile *
gb_editor_file_marks_get_file (GbEditorFileMarks *marks)
{
  gchar *path;
  GFile *file;
  
  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARKS (marks), NULL);

  path = g_build_filename (g_get_user_data_dir (),
                           "gnome-builder",
                           "file-marks",
                           NULL);
  file = g_file_new_for_path (path);
  g_free (path);

  return file;
}

GbEditorFileMark *
gb_editor_file_marks_get_for_file (GbEditorFileMarks *marks,
                                   GFile             *file)
{
  GbEditorFileMark *ret;
  gchar *uri;

  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARKS (marks), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  uri = g_file_get_uri (file);
  ret = g_hash_table_lookup (marks->priv->marks, uri);
  g_free (uri);

  return ret;
}

static GBytes *
gb_editor_file_marks_serialize (GbEditorFileMarks *marks)
{
  GbEditorFileMark *mark;
  GHashTableIter iter;
  GString *str;
  gsize len;
  gchar *data;

  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARKS (marks), NULL);

  str = g_string_new (NULL);

  g_hash_table_iter_init (&iter, marks->priv->marks);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&mark))
    {
      guint line;
      guint column;
      GFile *file;
      gchar *uri = NULL;

      g_object_get (mark,
                    "file", &file,
                    "line", &line,
                    "column", &column,
                    NULL);

      if (!file)
        continue;

      uri = g_file_get_uri (file);

      g_string_append_printf (str, "%u:%u %s\n", line, column, uri);

      g_free (uri);
      g_object_unref (file);
    }

  len = str->len;
  data = g_string_free (str, FALSE);

  return g_bytes_new_take (data, len);
}

static void
gb_editor_file_marks_save_cb (GObject      *source,
                              GAsyncResult *result,
                              gpointer      user_data)
{
  GFile *file = (GFile *)source;
  GSimpleAsyncResult *simple = user_data;
  GError *error = NULL;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple));

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_simple_async_result_take_error (simple, error);
  else
    g_simple_async_result_set_op_res_gboolean (simple, TRUE);

  g_simple_async_result_complete_in_idle (simple);
}

void
gb_editor_file_marks_save_async (GbEditorFileMarks   *marks,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GSimpleAsyncResult *simple;
  GFile *file = NULL;
  GBytes *bytes = NULL;

  g_return_if_fail (GB_IS_EDITOR_FILE_MARKS (marks));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  file = gb_editor_file_marks_get_file (marks);
  bytes = gb_editor_file_marks_serialize (marks);

  simple = g_simple_async_result_new (G_OBJECT (marks), callback, user_data,
                                      gb_editor_file_marks_save_async);

  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       cancellable,
                                       gb_editor_file_marks_save_cb,
                                       simple);

  g_clear_object (&file);
  g_clear_object (&simple);
  g_clear_pointer (&bytes, g_bytes_unref);
}

gboolean
gb_editor_file_marks_save_finish (GbEditorFileMarks  *marks,
                                  GAsyncResult       *result,
                                  GError            **error)
{
  GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;

  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARKS (marks), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (simple), FALSE);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;

  return TRUE;
}

gboolean
gb_editor_file_marks_load (GbEditorFileMarks  *marks,
                           GError            **error)
{
  gchar **parts = NULL;
  gchar *contents = NULL;
  GFile *file = NULL;
  gsize len = 0;
  gboolean ret;
  guint i;

  g_return_val_if_fail (GB_IS_EDITOR_FILE_MARKS (marks), FALSE);

  file = gb_editor_file_marks_get_file (marks);

  ret = g_file_load_contents (file, NULL, &contents, &len, NULL, error);

  if (ret)
    {
      parts = g_strsplit (contents, "\n", -1);

      for (i = 0; parts [i]; i++)
        {
          const gchar *str = g_strstrip (parts [i]);
          GbEditorFileMark *mark;
          gchar *endptr = NULL;
          GFile *mark_file;
          gint64 val;
          guint line;
          guint column;

          val = g_ascii_strtoll (str, &endptr, 10);
          if (((val == G_MAXINT64) || (val == G_MININT64)) && (errno == ERANGE))
            continue;
          line = (guint)val;

          if (*endptr != ':')
            continue;

          str = ++endptr;

          val = g_ascii_strtoll (str, &endptr, 10);
          if (((val == G_MAXINT64) || (val == G_MININT64)) && (errno == ERANGE))
            continue;
          column = (guint)val;

          if (*endptr != ' ')
            continue;

          str = ++endptr;

          if (gb_str_empty0 (str))
            continue;

          mark_file = g_file_new_for_uri (str);
          if (!mark_file)
            continue;

          mark = gb_editor_file_mark_new (mark_file, line, column);
          g_hash_table_replace (marks->priv->marks, g_strdup (str), mark);
          g_object_unref (mark_file);
        }
    }

  g_clear_object (&file);
  g_clear_pointer (&parts, g_strfreev);
  g_clear_pointer (&contents, g_free);

  return ret;
}

static void
gb_editor_file_marks_finalize (GObject *object)
{
  GbEditorFileMarksPrivate *priv = GB_EDITOR_FILE_MARKS (object)->priv;

  g_clear_pointer (&priv->marks, g_hash_table_unref);

  G_OBJECT_CLASS (gb_editor_file_marks_parent_class)->finalize (object);
}

static void
gb_editor_file_marks_class_init (GbEditorFileMarksClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gb_editor_file_marks_finalize;
}

static void
gb_editor_file_marks_init (GbEditorFileMarks *self)
{
  self->priv = gb_editor_file_marks_get_instance_private (self);
  self->priv->marks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, g_object_unref);
}
