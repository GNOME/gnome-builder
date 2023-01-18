/* ide-code-index-index.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-index"

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "ide-code-index-search-result.h"
#include "ide-code-index-index.h"

/*
 * This class will store index of all directories and will have a map of
 * directory and Indexes (IdeFuzzyIndex & IdePersistentMap)
 */

struct _IdeCodeIndexIndex
{
  IdeObject   parent_instance;

  GMutex      mutex;
  GHashTable *directories;
  GPtrArray  *indexes;
};

typedef struct
{
  GFile            *directory;
  GFile            *source_directory;
  IdeFuzzyIndex    *symbol_names;
  IdePersistentMap *symbol_keys;
  guint64           mtime;
} DirectoryIndex;

typedef struct
{
  gchar   *query;
  IdeHeap *fuzzy_matches;
  guint    curr_index;
  gsize    max_results;
} PopulateTaskData;

/*
 * Represents a match. It contains match, matches from which it came and
 * index from which matches came
 */
typedef struct
{
  IdeFuzzyIndex      *index;
  GListModel         *list;
  IdeFuzzyIndexMatch *match;
  guint               match_num;
} FuzzyMatch;

G_DEFINE_FINAL_TYPE (IdeCodeIndexIndex, ide_code_index_index, IDE_TYPE_OBJECT)

static void directory_index_free (DirectoryIndex *data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DirectoryIndex, directory_index_free)

static guint64
newest_mtime (GFile        *a,
              GFile        *b,
              GCancellable *cancellable)
{
  g_autoptr(GFileInfo) ainfo = NULL;
  g_autoptr(GFileInfo) binfo = NULL;
  guint64 aval = 0;
  guint64 bval = 0;

  ainfo = g_file_query_info (a, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, cancellable, NULL);
  binfo = g_file_query_info (b, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, cancellable, NULL);

  if (ainfo)
    aval = g_file_info_get_attribute_uint64 (ainfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  if (binfo)
    bval = g_file_info_get_attribute_uint64 (binfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);

  return aval > bval ? aval : bval;
}

static void
directory_index_free (DirectoryIndex *data)
{
  g_clear_object (&data->symbol_names);
  g_clear_object (&data->symbol_keys);
  g_clear_object (&data->directory);
  g_clear_object (&data->source_directory);
  g_slice_free (DirectoryIndex, data);
}

static void
populate_task_data_free (PopulateTaskData *data)
{
  g_clear_pointer (&data->query, g_free);

  for (guint i = 0; i < data->fuzzy_matches->len; i++)
    {
      g_clear_object (&(ide_heap_index(data->fuzzy_matches, FuzzyMatch, i).list));
      g_clear_object (&(ide_heap_index(data->fuzzy_matches, FuzzyMatch, i).match));
    }

  g_clear_pointer (&data->fuzzy_matches, ide_heap_unref);

  g_slice_free (PopulateTaskData, data);
}

static int
fuzzy_match_compare (const FuzzyMatch *a,
                     const FuzzyMatch *b)
{
  float diff;

  diff = ide_fuzzy_index_match_get_score (a->match) -
          ide_fuzzy_index_match_get_score (b->match);

  if (diff < 0)
    return -1;
  else if (diff > 0)
    return 1;
  else
    return 0;
}

/* This function will load indexes and returns them */
static DirectoryIndex *
directory_index_new (GFile         *directory,
                     GFile         *source_directory,
                     GCancellable  *cancellable,
                     GError       **error)
{
  g_autoptr(GFile) keys_file = NULL;
  g_autoptr(GFile) names_file = NULL;
  g_autoptr(IdeFuzzyIndex) symbol_names = NULL;
  g_autoptr(IdePersistentMap) symbol_keys = NULL;
  g_autoptr(DirectoryIndex) dir_index = NULL;

  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  symbol_keys = ide_persistent_map_new ();
  keys_file = g_file_get_child (directory, "SymbolKeys");

  if (!ide_persistent_map_load_file (symbol_keys, keys_file, cancellable, error))
    return NULL;

  symbol_names = ide_fuzzy_index_new ();
  names_file = g_file_get_child (directory, "SymbolNames");

  if (!ide_fuzzy_index_load_file (symbol_names, names_file, cancellable, error))
    return NULL;

  dir_index = g_slice_new0 (DirectoryIndex);
  dir_index->symbol_keys = g_steal_pointer (&symbol_keys);
  dir_index->symbol_names = g_steal_pointer (&symbol_names);
  dir_index->directory = g_file_dup (directory);
  dir_index->source_directory = g_file_dup (source_directory);
  dir_index->mtime = newest_mtime (keys_file, names_file, cancellable);

  return g_steal_pointer (&dir_index);
}

static gboolean
can_ignore_reload (IdeCodeIndexIndex *self,
                   GFile             *directory,
                   GCancellable      *cancellable)
{
  g_autofree gchar *dir_name = NULL;
  gboolean ret = FALSE;
  gpointer value;

  g_assert (IDE_IS_CODE_INDEX_INDEX (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  dir_name = g_file_get_path (directory);

  g_mutex_lock (&self->mutex);

  if (g_hash_table_lookup_extended (self->directories, dir_name, NULL, &value))
    {
      g_autoptr(GFile) keys_file = g_file_get_child (directory, "SymbolKeys");
      g_autoptr(GFile) names_file = g_file_get_child (directory, "SymbolNames");
      guint i = GPOINTER_TO_UINT (value);
      DirectoryIndex *info = g_ptr_array_index (self->indexes, i);
      guint64 mtime = newest_mtime (keys_file, names_file, cancellable);

      g_assert (i < self->indexes->len);
      g_assert (self->indexes->len > 0);

      ret = mtime <= info->mtime;
    }

  g_mutex_unlock (&self->mutex);

  return ret;
}

/**
 * ide_code_index_index_load:
 * @self: a #IdeCodeIndexIndex
 * @directory: a #GFile of the directory to load
 * @source_directory: a #GFile of the directory containing the sources
 * @cancellable: a #GCancellable or %NULL
 * @error: a #GError or %NULL
 *
 * This function will load the index of a directory and update old index
 * pointer if it exists.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Thread safety: you may call this function from a thread so long as the
 *   thread has a reference to @self.
 */
gboolean
ide_code_index_index_load (IdeCodeIndexIndex   *self,
                           GFile               *directory,
                           GFile               *source_directory,
                           GCancellable        *cancellable,
                           GError             **error)
{
  g_autoptr(DirectoryIndex) dir_index = NULL;
  g_autofree gchar *dir_name = NULL;
  gpointer value;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (directory), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (can_ignore_reload (self, directory, cancellable))
    return TRUE;

  dir_name = g_file_get_path (directory);
  g_debug ("Loading code index from %s", dir_name);

  if (!(dir_index = directory_index_new (directory, source_directory, cancellable, error)))
    return FALSE;

  g_mutex_lock (&self->mutex);

  if (g_hash_table_lookup_extended (self->directories, dir_name, NULL, &value))
    {
      guint i = GPOINTER_TO_UINT (value);

      g_assert (i < self->indexes->len);
      g_assert (self->indexes->len > 0);

      /* update current directory index by clearing old one */
      directory_index_free (g_ptr_array_index (self->indexes, i));
      g_ptr_array_index (self->indexes, i) = g_steal_pointer (&dir_index);
    }
  else
    {
      g_hash_table_insert (self->directories,
                           g_steal_pointer (&dir_name),
                           GUINT_TO_POINTER (self->indexes->len));

      g_ptr_array_add (self->indexes, g_steal_pointer (&dir_index));
    }

  g_mutex_unlock (&self->mutex);

  return TRUE;
}

/* Create a new IdeCodeIndexSearchResult based on match from fuzzy index */
static IdeCodeIndexSearchResult *
ide_code_index_index_create_search_result (IdeContext       *context,
                                           const FuzzyMatch *fuzzy_match)
{
  g_autoptr(IdeLocation) location = NULL;
  g_autoptr(GString) subtitle = NULL;
  g_autoptr(GFile) file = NULL;
  const char *key;
  const char *shortname;
  const char *path;
  GVariant *value;
  GIcon *gicon;
  float score;
  guint file_id;
  guint line;
  guint line_offset;
  guint kind;
  guint flags;
  char num [20];

  g_assert (IDE_IS_CONTEXT (context));
  g_assert (fuzzy_match != NULL);

  value = ide_fuzzy_index_match_get_document (fuzzy_match->match);

  g_variant_get (value, "(uuuuu)", &file_id, &line, &line_offset, &flags, &kind);

  /* Ignore variables in global search */
  if (kind == IDE_SYMBOL_KIND_VARIABLE)
    return NULL;

  key = ide_fuzzy_index_match_get_key (fuzzy_match->match);

  g_snprintf (num, sizeof num, "%u", file_id);

  path = ide_fuzzy_index_get_metadata_string (fuzzy_match->index, num);

  file = g_file_new_for_path (path);
  location = ide_location_new (file, line - 1, line_offset - 1);

  gicon = ide_symbol_kind_get_gicon (kind);
  score = ide_fuzzy_index_match_get_score (fuzzy_match->match);

  subtitle = g_string_new (NULL);

  if (NULL != (shortname = strrchr (path, G_DIR_SEPARATOR)))
    g_string_append (subtitle, shortname + 1);

  g_string_append_printf (subtitle, ":%u", line);

  if ((kind == IDE_SYMBOL_KIND_FUNCTION) && !(flags & IDE_SYMBOL_FLAGS_IS_DEFINITION))
    {
      /* translators: "Declaration" is describing a function that is defined in a header
       *              file (.h) rather than a source file (.c).
       */
      g_string_append_printf (subtitle, " (%s)", _("Declaration"));
    }

  return ide_code_index_search_result_new (key + 2, subtitle->str, gicon, location, score);
}

static void
ide_code_index_index_query_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeFuzzyIndex *index = (IdeFuzzyIndex *)object;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GListModel) list = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(GError) error = NULL;
  IdeCodeIndexIndex *self;
  PopulateTaskData *data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FUZZY_INDEX (index));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CODE_INDEX_INDEX (self));

  locker = g_mutex_locker_new (&self->mutex);

  data = ide_task_get_task_data (task);
  g_assert (data != NULL);

  list = ide_fuzzy_index_query_finish (index, result, &error);
  g_assert (!list || G_IS_LIST_MODEL (list));

  if (list != NULL)
    {
      if (g_list_model_get_n_items (list))
        {
          FuzzyMatch fuzzy_match = {0};

          fuzzy_match.index = index;
          fuzzy_match.match = g_list_model_get_item (list, 0);
          fuzzy_match.list = g_steal_pointer (&list);
          fuzzy_match.match_num = 0;

          ide_heap_insert_val (data->fuzzy_matches, fuzzy_match);
        }
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_message ("%s", error->message);
    }

  data->curr_index++;

  if (data->curr_index < self->indexes->len)
    {
      DirectoryIndex *dir_index;
      GCancellable *cancellable;

      dir_index = g_ptr_array_index (self->indexes, data->curr_index);
      cancellable = ide_task_get_cancellable (task);

      ide_fuzzy_index_query_async (dir_index->symbol_names,
                                   data->query,
                                   data->max_results,
                                   cancellable,
                                   ide_code_index_index_query_cb,
                                   g_steal_pointer (&task));
    }
  else
    {
      g_autoptr(GPtrArray) results = g_ptr_array_new_with_free_func (g_object_unref);
      g_autoptr(IdeContext) context = ide_object_ref_context (IDE_OBJECT (self));

      /*
       * Extract match from heap with max score, get next item from the list from which
       * the max score match came from and insert that into heap.
       */
      while (context != NULL && data->max_results > 0 && data->fuzzy_matches->len > 0)
        {
          IdeCodeIndexSearchResult *item;
          FuzzyMatch fuzzy_match;

          ide_heap_extract (data->fuzzy_matches, &fuzzy_match);

          item = ide_code_index_index_create_search_result (context, &fuzzy_match);
          if (item != NULL)
            g_ptr_array_add (results, item);

          data->max_results--;

          g_clear_object (&fuzzy_match.match);

          fuzzy_match.match_num++;

          if (fuzzy_match.match_num < g_list_model_get_n_items (fuzzy_match.list))
            {
              fuzzy_match.match = g_list_model_get_item (fuzzy_match.list, fuzzy_match.match_num);
              ide_heap_insert_val (data->fuzzy_matches, fuzzy_match);
            }
          else
            {
              g_clear_object (&fuzzy_match.list);
            }
        }

      if (data->max_results == 0 && data->fuzzy_matches->len > 0)
        g_object_set_data (G_OBJECT (task), "TRUNCATED", GINT_TO_POINTER (TRUE));

      ide_task_return_pointer (task,
                               g_steal_pointer (&results),
                               g_ptr_array_unref);
    }
}

void
ide_code_index_index_populate_async (IdeCodeIndexIndex   *self,
                                     const gchar         *query,
                                     gsize                max_results,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(GMutexLocker) locker = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_auto(GStrv) str = NULL;
  PopulateTaskData *data;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CODE_INDEX_INDEX (self));
  g_return_if_fail (query != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_code_index_index_populate_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  data = g_slice_new0 (PopulateTaskData);
  data->max_results = max_results;
  data->curr_index = 0;
  data->fuzzy_matches = ide_heap_new (sizeof (FuzzyMatch),
                                      (GCompareFunc)fuzzy_match_compare);

  /* Replace "<symbol type prefix><space>" with <symbol code>INFORMATION SEPARATOR ONE  */

  str = g_strsplit (query, " ", 2);

  if (str[1] == NULL)
    {
      data->query = g_strconcat ("\x1F", query, NULL);
    }
  else if (str[1] != NULL)
    {
      const gchar *prefix = "\0";

      if (g_str_has_prefix ("function", str[0]))
        prefix = "f";
      else if (g_str_has_prefix ("variable", str[0]))
        prefix = "v";
      else if (g_str_has_prefix ("struct", str[0]))
        prefix = "s";
      else if (g_str_has_prefix ("union", str[0]))
        prefix = "u";
      else if (g_str_has_prefix ("enum", str[0]))
        prefix = "e";
      else if (g_str_has_prefix ("class", str[0]))
        prefix = "c";
      else if (g_str_has_prefix ("constant", str[0]))
        prefix = "a";
      else if (g_str_has_prefix ("macro", str[0]))
        prefix = "m";

      data->query = g_strconcat (prefix, "\x1F", str[1], NULL);
    }

  ide_task_set_task_data (task, data, populate_task_data_free);

  locker = g_mutex_locker_new (&self->mutex);

  if (data->curr_index < self->indexes->len)
    {
      DirectoryIndex *dir_index = g_ptr_array_index (self->indexes, data->curr_index);

      ide_fuzzy_index_query_async (dir_index->symbol_names,
                                   data->query,
                                   data->max_results,
                                   cancellable,
                                   ide_code_index_index_query_cb,
                                   g_steal_pointer (&task));
    }
  else
    {
      ide_task_return_pointer (task,
                               g_ptr_array_new_with_free_func (g_object_unref),
                               g_ptr_array_unref);
    }
}

GPtrArray *
ide_code_index_index_populate_finish (IdeCodeIndexIndex *self,
                                      GAsyncResult      *result,
                                      gboolean          *truncated,
                                      GError           **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  if (truncated)
    *truncated = !!g_object_get_data (G_OBJECT (result), "TRUNCATED");

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

IdeSymbol *
ide_code_index_index_lookup_symbol (IdeCodeIndexIndex *self,
                                    const gchar       *key)
{
  g_autoptr(IdeLocation) declaration = NULL;
  g_autoptr(IdeLocation) definition = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  g_autofree gchar *name = NULL;
  IdeSymbolKind kind = IDE_SYMBOL_KIND_NONE;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;
  IdeFuzzyIndex *symbol_names = NULL;
  const DirectoryIndex *dir_index = NULL;
  const gchar *filename;
  guint32 file_id = 0;
  guint32 line = 0;
  guint32 line_offset = 0;
  gchar num[20];

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  g_debug ("Searching declaration with key: %s", key);

  locker = g_mutex_locker_new (&self->mutex);

  for (guint i = 0; i < self->indexes->len; i++)
    {
      g_autoptr(GVariant) variant = NULL;

      dir_index = g_ptr_array_index (self->indexes, i);

      if (!(variant = ide_persistent_map_lookup_value (dir_index->symbol_keys, key)))
        continue;

      symbol_names = dir_index->symbol_names;

      g_variant_get (variant, "(uuuu)", &file_id, &line, &line_offset, &flags);

      if (flags & IDE_SYMBOL_FLAGS_IS_DEFINITION)
        break;
    }

  if (file_id == 0)
    {
      g_debug ("symbol location not found");
      return NULL;
    }

  g_snprintf (num, sizeof(num), "%u", file_id);

  filename = ide_fuzzy_index_get_metadata_string (symbol_names, num);
  file = g_file_new_for_path (filename);

  if (flags & IDE_SYMBOL_FLAGS_IS_DEFINITION)
    definition = ide_location_new (file, line - 1, line_offset - 1);
  else
    declaration = ide_location_new (file, line - 1, line_offset - 1);

  return ide_symbol_new (name, kind, flags, definition, declaration);
}

static void
ide_code_index_index_finalize (GObject *object)
{
  IdeCodeIndexIndex *self = (IdeCodeIndexIndex *)object;

  g_clear_pointer (&self->directories, g_hash_table_unref);
  g_clear_pointer (&self->indexes, g_ptr_array_unref);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (ide_code_index_index_parent_class)->finalize (object);
}

static void
ide_code_index_index_init (IdeCodeIndexIndex *self)
{
  self->directories = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->indexes = g_ptr_array_new_with_free_func ((GDestroyNotify)directory_index_free);

  g_mutex_init (&self->mutex);
}

static void
ide_code_index_index_class_init (IdeCodeIndexIndexClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_code_index_index_finalize;
}

IdeCodeIndexIndex *
ide_code_index_index_new (IdeObject *parent)
{
  /* Without parent, no queries are supported */
  g_return_val_if_fail (!parent || IDE_IS_OBJECT (parent), NULL);

  return g_object_new (IDE_TYPE_CODE_INDEX_INDEX,
                       "parent", parent,
                       NULL);
}
