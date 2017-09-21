/* ide-code-index-index.c
 *
 * Copyright (C) 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-index"

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "ide-code-index-search-result.h"
#include "ide-code-index-index.h"
#include "ide-persistent-map.h"

/*
 * This class will store index of all directories and will have a map of
 * directory and Indexes (DzlFuzzyIndex & IdePersistentMap)
 */

struct _IdeCodeIndexIndex
{
  IdeObject   parent;

  GHashTable *directories;
  GPtrArray  *indexes;

  GMutex      update_entries;
};

typedef struct
{
  DzlFuzzyIndex    *symbol_names;
  IdePersistentMap *symbol_keys;
} DirectoryIndex;

typedef struct
{
  gchar   *query;
  DzlHeap *fuzzy_matches;
  guint    curr_index;
  gsize    max_results;
} PopulateTaskData;

/*
 * Represents a match. It contains match, matches from which it came and
 * index from which matches came
 */
typedef struct
{
  DzlFuzzyIndex      *index;
  GListModel         *list;
  DzlFuzzyIndexMatch *match;
  guint               match_num;
} FuzzyMatch;

G_DEFINE_TYPE (IdeCodeIndexIndex, ide_code_index_index, IDE_TYPE_OBJECT)

static void directory_index_free (DirectoryIndex *data);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DirectoryIndex, directory_index_free)

static void
directory_index_free (DirectoryIndex *data)
{
  g_clear_object (&data->symbol_names);
  g_clear_object (&data->symbol_keys);
  g_slice_free (DirectoryIndex, data);
}

static void
populate_task_data_free (PopulateTaskData *data)
{
  g_clear_pointer (&data->query, g_free);

  for (guint i = 0; i < data->fuzzy_matches->len; i++)
    {
      g_clear_object (&(dzl_heap_index(data->fuzzy_matches, FuzzyMatch, i).list));
      g_clear_object (&(dzl_heap_index(data->fuzzy_matches, FuzzyMatch, i).match));
    }

  g_clear_pointer (&data->fuzzy_matches, dzl_heap_unref);
  g_slice_free (PopulateTaskData, data);
}

static int
fuzzy_match_compare (const FuzzyMatch *a,
                     const FuzzyMatch *b)
{
  float diff;

  diff = dzl_fuzzy_index_match_get_score (a->match) -
          dzl_fuzzy_index_match_get_score (b->match);

  if (diff < 0)
    return -1;
  else if (diff > 0)
    return 1;
  else
    return 0;
}

/* This function will load indexes and returns them */
static DirectoryIndex *
ide_code_index_index_real_load_index (IdeCodeIndexIndex *self,
                                      GFile             *directory,
                                      GCancellable      *cancellable,
                                      GError           **error)
{
  g_autoptr(GFile) keys_file = NULL;
  g_autoptr(GFile) names_file = NULL;
  g_autoptr(DzlFuzzyIndex) symbol_names = NULL;
  g_autoptr(IdePersistentMap) symbol_keys = NULL;
  g_autoptr(DirectoryIndex) dir_index = NULL;

  g_assert (IDE_IS_CODE_INDEX_INDEX (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  symbol_keys = ide_persistent_map_new ();
  keys_file = g_file_get_child (directory, "SymbolKeys");

  if (!ide_persistent_map_load_file (symbol_keys, keys_file, cancellable, error))
    return NULL;

  symbol_names = dzl_fuzzy_index_new ();
  names_file = g_file_get_child (directory, "SymbolNames");

  if (!dzl_fuzzy_index_load_file (symbol_names, names_file, cancellable, error))
    return NULL;

  dir_index = g_slice_new0 (DirectoryIndex);
  dir_index->symbol_keys = g_steal_pointer (&symbol_keys);
  dir_index->symbol_names = g_steal_pointer (&symbol_names);

  return g_steal_pointer (&dir_index);
}

/* This function will load index of a directory and update old index pointer (if exists) */
gboolean
ide_code_index_index_load (IdeCodeIndexIndex   *self,
                           GFile               *directory,
                           GCancellable        *cancellable,
                           GError             **error)
{
  g_autoptr(DirectoryIndex) dir_index = NULL;
  g_autofree gchar *dir_name = NULL;
  gpointer value;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (directory), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  if (NULL == (dir_index = ide_code_index_index_real_load_index (self,
                                                                 directory,
                                                                 cancellable,
                                                                 error)))
    {
      return FALSE;
    }

  dir_name = g_file_get_path (directory);

  g_mutex_lock (&self->update_entries);

  if (g_hash_table_lookup_extended (self->directories,
                                    dir_name,
                                    NULL,
                                    &value))
    {
      guint i = GPOINTER_TO_UINT (value);

      /* update current directory index by clearing old one */
      g_clear_pointer (&g_ptr_array_index (self->indexes, i), (GDestroyNotify)directory_index_free);
      g_ptr_array_index (self->indexes, i) = g_steal_pointer (&dir_index);
    }
  else
    {
      g_hash_table_insert (self->directories,
                           g_steal_pointer (&dir_name),
                           GUINT_TO_POINTER (self->indexes->len));

      g_ptr_array_add (self->indexes, g_steal_pointer (&dir_index));
    }

  g_mutex_unlock (&self->update_entries);

  return TRUE;
}

/*
 * This function will load index from directory if it is not modified. This
 * fuction will only load if all "files"(GPtrArray) and only those "files"
 * are there in index.
 */
gboolean
ide_code_index_index_load_if_nmod (IdeCodeIndexIndex     *self,
                                   GFile                 *directory,
                                   GPtrArray             *files,
                                   GTimeVal               mod_time,
                                   GCancellable          *cancellable,
                                   GError               **error)
{
  g_autoptr(DirectoryIndex) dir_index = NULL;
  gpointer value = NULL;
  DzlFuzzyIndex *symbol_names;
  GTimeVal index_mod_time;
  g_autoptr(GFile) names_file = NULL;
  g_autoptr(GFileInfo) file_info = NULL;
  g_autofree gchar *dir_name = NULL;
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (directory), FALSE);
  g_return_val_if_fail (files != NULL, FALSE);

  if (NULL == (dir_index = ide_code_index_index_real_load_index (self,
                                                                 directory,
                                                                 cancellable,
                                                                 error)))
    {
      return FALSE;
    }

  symbol_names = dir_index->symbol_names;

  /*
   * Return false if current number of files in directory != number of files that are
   * indexed previously in the same directory.
   */
  if (dzl_fuzzy_index_get_metadata_uint32 (symbol_names, "n_files") != files->len)
    return FALSE;

  /* Return false if files are modified after they are indexed. */
  names_file = g_file_get_child (directory, "SymbolNames");

  if (NULL == (file_info = g_file_query_info (names_file,
                                              G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                              G_FILE_QUERY_INFO_NONE,
                                              cancellable,
                                              error)))
    {
      return FALSE;
    }

  g_file_info_get_modification_time (file_info, &index_mod_time);

  if ((mod_time.tv_sec > index_mod_time.tv_sec) ||
     ((mod_time.tv_sec == index_mod_time.tv_sec) &&
      (mod_time.tv_usec > index_mod_time.tv_usec)))
    return FALSE;

  /* Return false if all current files in directory are not there in index. */
  for (guint i = 0; i < files->len; i++)
    {
      g_autofree gchar *file_name = NULL;

      file_name = g_file_get_path (g_ptr_array_index (files, i));

      if (!dzl_fuzzy_index_get_metadata_uint32 (symbol_names, file_name))
        return FALSE;
    }

  dir_name = g_file_get_path (directory);

  g_mutex_lock (&self->update_entries);

  if (g_hash_table_lookup_extended (self->directories,
                                    dir_name,
                                    NULL,
                                    &value))
    {
      guint i = GPOINTER_TO_UINT (value);

      g_clear_pointer (&g_ptr_array_index (self->indexes, i), (GDestroyNotify)directory_index_free);
      g_ptr_array_index (self->indexes, i) = g_steal_pointer (&dir_index);
    }
  else
    {
      g_hash_table_insert (self->directories,
                           g_steal_pointer (&dir_name),
                           GUINT_TO_POINTER (self->indexes->len));

      g_ptr_array_add (self->indexes, g_steal_pointer (&dir_index));
    }

  g_mutex_unlock (&self->update_entries);

  return TRUE;
}

/* Create a new IdeCodeIndexSearchResult based on match from fuzzy index */
static IdeCodeIndexSearchResult *
ide_code_index_index_new_search_result (IdeCodeIndexIndex *self,
                                        const FuzzyMatch  *fuzzy_match)
{
  g_autoptr(IdeFile) file = NULL;
  g_autoptr(IdeSourceLocation) location = NULL;
  g_autoptr(GString) subtitle = NULL;
  IdeContext *context;
  const gchar *key;
  const gchar *icon_name;
  const gchar *shortname;
  const gchar *path;
  GVariant *value;
  gfloat score;
  guint file_id;
  guint line;
  guint line_offset;
  guint kind;
  guint flags;
  gchar num [20];

  g_assert (IDE_IS_CODE_INDEX_INDEX (self));
  g_assert (fuzzy_match != NULL);

  value = dzl_fuzzy_index_match_get_document (fuzzy_match->match);

  g_variant_get (value, "(uuuuu)", &file_id, &line, &line_offset, &flags, &kind);

  /* Ignore variables in global search */
  if (kind == IDE_SYMBOL_VARIABLE)
    return NULL;

  context = ide_object_get_context (IDE_OBJECT (self));
  key = dzl_fuzzy_index_match_get_key (fuzzy_match->match);

  g_snprintf (num, sizeof num, "%u", file_id);
  path = dzl_fuzzy_index_get_metadata_string (fuzzy_match->index, num);
  file = ide_file_new_for_path (context, path);
  location = ide_source_location_new (file, line - 1, line_offset - 1, 0);

  icon_name = ide_symbol_kind_get_icon_name (kind);
  score = dzl_fuzzy_index_match_get_score (fuzzy_match->match);

  subtitle = g_string_new (NULL);
  if (NULL != (shortname = strrchr (path, G_DIR_SEPARATOR)))
    g_string_append (subtitle, shortname + 1);
  if ((kind == IDE_SYMBOL_FUNCTION) && !(flags & IDE_SYMBOL_FLAGS_IS_DEFINITION))
    /* translators: "Declaration" is the forward-declaration (usually a header file), not the implementation */
    g_string_append_printf (subtitle, " (%s)", _("Declaration"));

  return ide_code_index_search_result_new (context,
                                           key + 2,
                                           subtitle->str,
                                           icon_name,
                                           location,
                                           score);
}

static void
ide_code_index_index_query_cb (GObject       *object,
                               GAsyncResult  *result,
                               gpointer       user_data)
{
  IdeCodeIndexIndex *self;
  DzlFuzzyIndex *index = (DzlFuzzyIndex *)object;
  g_autoptr(GTask) task = (GTask *)user_data;
  PopulateTaskData *data;
  g_autoptr(GListModel) list = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (DZL_IS_FUZZY_INDEX (index));
  g_assert (G_IS_TASK (task));

  self = g_task_get_source_object (task);
  data = g_task_get_task_data (task);

  if (NULL != (list = dzl_fuzzy_index_query_finish (index, result, &error)))
    {
      if (g_list_model_get_n_items (list))
        {
          FuzzyMatch fuzzy_match;

          fuzzy_match.index = index;
          fuzzy_match.match = g_list_model_get_item (list, 0);
          fuzzy_match.list = g_steal_pointer (&list);
          fuzzy_match.match_num = 0;

          dzl_heap_insert_val (data->fuzzy_matches, fuzzy_match);
        }
    }
  else
    {
      g_message ("%s", error->message);
    }

  data->curr_index++;

  if (data->curr_index < self->indexes->len)
    {
      DirectoryIndex *dir_index;
      GCancellable *cancellable;

      dir_index = g_ptr_array_index (self->indexes, data->curr_index);
      cancellable = g_task_get_cancellable (task);

      dzl_fuzzy_index_query_async (dir_index->symbol_names,
                                   data->query,
                                   data->max_results,
                                   cancellable,
                                   ide_code_index_index_query_cb,
                                   g_steal_pointer (&task));
    }
  else
    {
      g_autoptr(GPtrArray) results = NULL;

      results = g_ptr_array_new_with_free_func (g_object_unref);

      /*
       * Extract match from heap with max score, get next item from the list from which
       * the max score match came from and insert that into heap.
       */
      while (data->max_results && data->fuzzy_matches->len)
        {
          IdeCodeIndexSearchResult *item;
          FuzzyMatch fuzzy_match;

          dzl_heap_extract (data->fuzzy_matches, &fuzzy_match);
          item = ide_code_index_index_new_search_result (self, &fuzzy_match);
          if (item != NULL)
            g_ptr_array_add (results, item);
          data->max_results--;

          g_clear_object (&fuzzy_match.match);

          fuzzy_match.match_num++;

          if (fuzzy_match.match_num < g_list_model_get_n_items (fuzzy_match.list))
            {
              fuzzy_match.match = g_list_model_get_item (fuzzy_match.list, fuzzy_match.match_num);
              dzl_heap_insert_val (data->fuzzy_matches, fuzzy_match);
            }
          else
            {
              g_clear_object (&fuzzy_match.list);
            }
        }

      g_task_return_pointer (task, g_steal_pointer (&results), (GDestroyNotify)g_ptr_array_unref);
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
  g_autoptr(GTask) task = NULL;
  PopulateTaskData *data;
  g_auto(GStrv) str = NULL;

  g_return_if_fail (IDE_IS_CODE_INDEX_INDEX (self));
  g_return_if_fail (query != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);

  data = g_slice_new0 (PopulateTaskData);
  data->max_results = max_results;
  data->curr_index = 0;
  data->fuzzy_matches = dzl_heap_new (sizeof(FuzzyMatch), (GCompareFunc)fuzzy_match_compare);

  /* Replace "<symbol type prefix><space>" with <symbol code>INFORMATION SEPARATOR ONE  */

  str = g_strsplit (query, " ", 2);

  if (str[1] == NULL)
    {
      data->query = g_strconcat ("\x1F", query, NULL);
    }
  else if (str[1] != NULL)
    {
      gchar *prefix = "\0";

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

  g_task_set_task_data (task, data, (GDestroyNotify)populate_task_data_free);

  if (data->curr_index < self->indexes->len)
    {
      DirectoryIndex *dir_index;

      dir_index = g_ptr_array_index (self->indexes, data->curr_index);

      dzl_fuzzy_index_query_async (dir_index->symbol_names,
                                   data->query,
                                   data->max_results,
                                   cancellable,
                                   ide_code_index_index_query_cb,
                                   g_steal_pointer (&task));
    }
  else
    {
      g_task_return_pointer (task, g_ptr_array_new (), (GDestroyNotify)g_ptr_array_unref);
    }
}

GPtrArray *
ide_code_index_index_populate_finish (IdeCodeIndexIndex *self,
                                      GAsyncResult      *result,
                                      GError           **error)
{
  GTask *task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return g_task_propagate_pointer (task, error);
}

IdeSymbol *
ide_code_index_index_lookup_symbol (IdeCodeIndexIndex     *self,
                                    const gchar           *key)
{
  g_autofree gchar *name = NULL;
  IdeSymbolKind kind = IDE_SYMBOL_NONE;
  IdeSymbolFlags flags = IDE_SYMBOL_FLAGS_NONE;
  DirectoryIndex *dir_index;
  DzlFuzzyIndex *symbol_names = NULL;
  guint32 file_id =0;
  guint32 line = 0;
  guint32 line_offset = 0;
  gchar num[20];
  const gchar *path;
  g_autoptr(IdeSourceLocation) declaration = NULL;
  g_autoptr(IdeSourceLocation) definition = NULL;
  g_autoptr(IdeFile) file = NULL;

  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (self), NULL);

  if (key == NULL || !key[0])
    return NULL;

  g_message ("Searching declaration with key:%s\n", key);

  for (guint i = 0; i < self->indexes->len; i++)
    {
      g_autoptr(GVariant) variant = NULL;

      dir_index = g_ptr_array_index (self->indexes, i);

      variant = ide_persistent_map_lookup_value (dir_index->symbol_keys, key);

      if (variant == NULL)
        continue;

      symbol_names = dir_index->symbol_names;

      g_variant_get (variant, "(uuuu)", &file_id, &line, &line_offset, &flags);

      if (flags & IDE_SYMBOL_FLAGS_IS_DEFINITION)
        break;
    }

  if (!file_id)
    {
      g_debug ("symbol location not found");
      return NULL;
    }

  g_snprintf (num, sizeof(num), "%u", file_id);

  path = dzl_fuzzy_index_get_metadata_string (symbol_names, num);
  file = ide_file_new_for_path (ide_object_get_context (IDE_OBJECT (self)), path);

  g_debug ("symbol location found, %s %d:%d\n", path, line, line_offset);

  if (flags & IDE_SYMBOL_FLAGS_IS_DEFINITION)
    definition = ide_source_location_new (file, line - 1, line_offset - 1, 0);
  else
    declaration = ide_source_location_new (file, line - 1, line_offset - 1, 0);

  return ide_symbol_new (name, kind, flags, declaration, definition, NULL);
}

static void
ide_code_index_index_finalize (GObject *object)
{
  IdeCodeIndexIndex *self = (IdeCodeIndexIndex *)object;

  g_clear_pointer (&self->directories, g_hash_table_unref);
  g_clear_pointer (&self->indexes, g_ptr_array_unref);

  g_mutex_clear (&self->update_entries);

  G_OBJECT_CLASS (ide_code_index_index_parent_class)->finalize (object);
}

static void
ide_code_index_index_init (IdeCodeIndexIndex *self)
{
  self->directories = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->indexes = g_ptr_array_new_with_free_func ((GDestroyNotify)directory_index_free);

  g_mutex_init (&self->update_entries);
}

static void
ide_code_index_index_class_init (IdeCodeIndexIndexClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_code_index_index_finalize;
}

IdeCodeIndexIndex *
ide_code_index_index_new (IdeContext *context)
{
  return g_object_new (IDE_TYPE_CODE_INDEX_INDEX,
                       "context", context,
                       NULL);
}
