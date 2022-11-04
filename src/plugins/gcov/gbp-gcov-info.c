/*
 * gbp-gcov-info.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define G_LOG_DOMAIN "gbp-gcov-info"

#include "config.h"

#include <json-glib/json-glib.h>

#include "gbp-gcov-info.h"

struct _GbpGcovInfo
{
  GObject       parent_instance;
  GStringChunk *strings;
  GHashTable   *all_files;
  guint         loaded : 1;
};

G_DEFINE_FINAL_TYPE (GbpGcovInfo, gbp_gcov_info, G_TYPE_OBJECT)

GbpGcovInfo *
gbp_gcov_info_new (void)
{
  return g_object_new (GBP_TYPE_GCOV_INFO, NULL);
}

static void
gbp_gcov_info_dispose (GObject *object)
{
  GbpGcovInfo *self = (GbpGcovInfo *)object;

  g_clear_pointer (&self->all_files, g_hash_table_unref);
  g_clear_pointer (&self->strings, g_string_chunk_free);

  G_OBJECT_CLASS (gbp_gcov_info_parent_class)->dispose (object);
}

static void
gbp_gcov_info_class_init (GbpGcovInfoClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gbp_gcov_info_dispose;
}

static void
gbp_gcov_info_init (GbpGcovInfo *self)
{
  self->strings = g_string_chunk_new (4096 * 2);
}

static void
gbp_gcov_info_load_worker (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  JsonParser *parser = task_data;
  g_autoptr(GHashTable) all_files = NULL;
  const char *version = NULL;
  JsonNode *root;
  JsonNode *files;
  JsonObject *obj;
  JsonArray *ar;

  g_assert (G_IS_TASK (task));
  g_assert (GBP_IS_GCOV_INFO (source_object));
  g_assert (JSON_IS_PARSER (parser));

  if (!(root = json_parser_get_root (parser)) ||
      !JSON_NODE_HOLDS_OBJECT (root) ||
      !(obj = json_node_get_object (root)))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Incorrect or unsupported gcov JSON data");
      return;
    }

  if (!json_object_has_member (obj, "format_version") ||
      !(version = json_object_get_string_member (obj, "format_version")) ||
      !g_str_equal (version, "1"))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "Unsupported format_version for gcov output: %s",
                               version ? version : "no version provided");
      return;
    }

  all_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_array_unref);

  if (json_object_has_member (obj, "files") &&
      (files = json_object_get_member (obj, "files")) &&
      JSON_NODE_HOLDS_ARRAY (files) &&
      (ar = json_node_get_array (files)))
    {
      guint n_files = json_array_get_length (ar);

      for (guint i = 0; i < n_files; i++)
        {
          JsonNode *node = json_array_get_element (files, i);
          g_autoptr(GArray) lines_ar = NULL;
          JsonArray *lines;
          JsonObject *file;
          const char *filename;

          if (!JSON_NODE_HOLDS_OBJECT (node) ||
              !(file = json_node_get_object (node)))
            continue;

          /* Need a filename to be able to save anything */
          if (!(filename = json_object_get_string_member (file, "file")))
            continue;

          lines_ar = g_array_new (FALSE, FALSE, sizeof (GbpGcovLineInfo));

          /* Get the lines within the file */
          if ((lines = json_object_get_array_member (file, "lines")))
            {
              guint n_lines = json_array_get_length (lines);

              for (guint l = 0; l < n_lines; l++)
                {
                  JsonObject *line = json_array_get_object_element (lines, l);
                  gint64 lineno = json_object_get_int_member (line, "line_number");
                  gint64 count = json_object_get_int_member (line, "count");
                  gboolean unexecuted_block = json_object_get_boolean_member (line, "unexecuted_block");
                  const char *function_name = json_object_get_string_member (line, "function_name");
                  GbpGcovLineInfo info = {0};

                  /* TODO: what does branches:[] format look like? */

                  if (function_name != NULL)
                    function_name = g_string_chunk_insert_const (self->strings, function_name);

                  info.function_name = function_name;
                  info.line_number = lineno;
                  info.count = count;
                  info.unexecuted_block = !!unexecuted_block;

                  g_array_append_val (lines_ar, info);
                }
            }

          if (lines_ar->len > 0)
            g_hash_table_insert (all_files,
                                 g_strdup (filename),
                                 g_steal_pointer (&lines_ar));
        }
    }

  /* TODO: Parse "functions" data so we can provide tooltip info */

  self->all_files = g_steal_pointer (&all_files);

  g_task_return_boolean (task, TRUE);

  return;
}

static void
gbp_gcov_info_load_file_parse_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  JsonParser *parser = (JsonParser *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;

  g_assert (JSON_IS_PARSER (parser));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!json_parser_load_from_stream_finish (parser, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_set_task_data (task, g_object_ref (parser), g_object_unref);
  g_task_run_in_thread (task, gbp_gcov_info_load_worker);
}

static void
gbp_gcov_info_load_file_open_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  GFile *file = (GFile *)object;
  g_autoptr(GFileInputStream) file_stream = NULL;
  g_autoptr(GInputStream) converter_stream = NULL;
  g_autoptr(JsonParser) parser = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GTask) task = user_data;
  GCancellable *cancellable;
  g_autofree char *name = NULL;
  GInputStream *stream;

  g_assert (G_IS_FILE (file));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (task));

  if (!(file_stream = g_file_read_finish (file, result, &error)))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  stream = G_INPUT_STREAM (file_stream);
  name = g_file_get_basename (file);

  if (g_str_has_suffix (name, ".gz"))
    {
      g_autoptr(GZlibDecompressor) converter = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);
      converter_stream = g_converter_input_stream_new (stream, G_CONVERTER (converter));
      stream = converter_stream;
    }

  parser = json_parser_new ();
  cancellable = g_task_get_cancellable (task);

  json_parser_load_from_stream_async (parser,
                                      stream,
                                      cancellable,
                                      gbp_gcov_info_load_file_parse_cb,
                                      g_steal_pointer (&task));
}

void
gbp_gcov_info_load_file_async (GbpGcovInfo         *self,
                               GFile               *file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (GBP_IS_GCOV_INFO (self));
  g_return_if_fail (G_IS_FILE (file));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, gbp_gcov_info_load_file_async);

  if (self->loaded)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Cannot load file twice");
      return;
    }

  self->loaded = TRUE;

  g_file_read_async (file,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     gbp_gcov_info_load_file_open_cb,
                     g_steal_pointer (&task));
}

gboolean
gbp_gcov_info_load_file_finish (GbpGcovInfo   *self,
                                GAsyncResult  *result,
                                GError       **error)
{
  g_return_val_if_fail (GBP_IS_GCOV_INFO (self), NULL);
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static int
compare_by_line (gconstpointer aptr,
                 gconstpointer bptr)
{
  const GbpGcovLineInfo *a = aptr;
  const GbpGcovLineInfo *b = bptr;

  if (a->line_number < b->line_number)
    return -1;
  else if (a->line_number > b->line_number)
    return 1;
  else
    return 0;
}

const GbpGcovLineInfo *
gbp_gcov_info_get_line (GbpGcovInfo *self,
                        const char  *filename,
                        guint        line)
{
  GbpGcovLineInfo key = { .line_number = line };
  GArray *lines;
  guint match_index = 0;

  g_return_val_if_fail (GBP_IS_GCOV_INFO (self), NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  if (!(lines = g_hash_table_lookup (self->all_files, filename)))
    return NULL;

  if (!g_array_binary_search (lines, &key, compare_by_line, &match_index))
    return NULL;

  return &g_array_index (lines, GbpGcovLineInfo, match_index);
}

void
gbp_gcov_info_foreach_in_range (GbpGcovInfo          *self,
                                const char           *filename,
                                guint                 begin_line,
                                guint                 end_line,
                                GFunc                 foreach_func,
                                gpointer              user_data)
{
  GArray *lines;

  g_return_if_fail (GBP_IS_GCOV_INFO (self));
  g_return_if_fail (filename != NULL);

  if (!(lines = g_hash_table_lookup (self->all_files, filename)))
    return NULL;

  /* TODO: We could use a fuzzy bsearch here like we do elsewhere in Builder
   *       to avoid walking the array.
   */

  for (guint i = 0; i < lines->len; i++)
    {
      const GbpGcovLineInfo *info = &g_array_index (lines, GbpGcovLineInfo, i);

      if (info->line_number > end_line)
        break;

      if (info->line_number >= begin_line && info->line_number <= end_line)
        foreach_func (info, user_data);
    }
}
