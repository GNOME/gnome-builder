/* ide-code-index-builder.c
 *
 * Copyright © 2017 Anoop Chandu <anoopchandu96@gmail.com>
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

#define G_LOG_DOMAIN "ide-code-index-builder"

#include <dazzle.h>
#include <glib/gprintf.h>

#include "ide-code-index-builder.h"
#include "ide-persistent-map-builder.h"

struct _IdeCodeIndexBuilder
{
  IdeObject            parent;

  IdeCodeIndexIndex   *index;
  IdeCodeIndexService *service;

  GMutex               indexed;

  GHashTable          *build_flags;
};

typedef struct
{
  GFile     *directory;
  GPtrArray *changes;
  guint      recursive : 1;
} GetChangesTaskData;

typedef struct
{
  GPtrArray *files;
  GFile     *destination;
} IndexingData;

G_DEFINE_TYPE (IdeCodeIndexBuilder, ide_code_index_builder, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_INDEX,
  PROP_SERVICE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
get_changes_task_data_free (GetChangesTaskData *data)
{
  g_clear_object (&data->directory);
  g_clear_pointer (&data->changes, g_ptr_array_unref);
  g_slice_free (GetChangesTaskData, data);
}

static void
indexing_data_free (IndexingData *data)
{
  if (data == NULL)
    return;
  g_clear_pointer (&data->files, g_ptr_array_unref);
  g_clear_object (&data->destination);
  g_slice_free (IndexingData, data);
}

static gboolean
timeval_compare (GTimeVal a,
                 GTimeVal b)
{
  return ((a.tv_sec > b.tv_sec) || ((a.tv_sec == b.tv_sec) && (a.tv_usec > b.tv_usec)));
}

static const gchar * const *
ide_code_index_builder_get_build_flags (IdeCodeIndexBuilder *self,
                                        GFile               *file)
{
  g_autoptr(IdeFile) ifile = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));

  context = ide_object_get_context (IDE_OBJECT (self));
  ifile = ide_file_new (context, file);

  return g_hash_table_lookup (self->build_flags, ifile);
}

static GPtrArray *
ide_code_index_builder_get_all_files (IdeCodeIndexBuilder *self,
                                      GPtrArray           *changes)
{
  g_autoptr(GPtrArray) all_files = NULL;
  IdeContext *context;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (changes != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  all_files = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < changes->len; i++)
    {
      const IndexingData *change = g_ptr_array_index (changes, i);
      GPtrArray *dir_files = change->files;

      for (guint j = 0; j < dir_files->len; j++)
        {
          GFile *gfile = g_ptr_array_index (dir_files, j);
          g_autoptr(IdeFile) file = ide_file_new (context, gfile);

          if (!g_hash_table_contains (self->build_flags, file))
            g_ptr_array_add (all_files, g_steal_pointer (&file));
        }
    }

  return g_steal_pointer (&all_files);
}

/* Index directories: index all directores, store index and load index */

static void
ide_code_index_builder_index_file (IdeCodeIndexBuilder      *self,
                                   GFile                    *file,
                                   guint32                   file_id,
                                   IdePersistentMapBuilder  *map_builder,
                                   DzlFuzzyIndexBuilder     *fuzzy_builder,
                                   GTask                    *task)
{
  g_autofree gchar *file_name = NULL;
  g_autoptr(IdeCodeIndexEntries) entries = NULL;
  g_autoptr(GError) error = NULL;
  IdeCodeIndexer *indexer;
  GCancellable *cancellable;
  gpointer indexentryptr;
  gchar num[16];

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (map_builder));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (fuzzy_builder));
  g_assert (G_IS_TASK (task));

  file_name = g_file_get_path (file);

  indexer = ide_code_index_service_get_code_indexer (self->service, file_name);
  if (indexer == NULL)
    return;

  cancellable = g_task_get_cancellable (task);

  entries = ide_code_indexer_index_file (indexer,
                                         file,
                                         ide_code_index_builder_get_build_flags (self, file),
                                         cancellable,
                                         &error);

  if (entries == NULL)
    {
      if (error != NULL)
        g_warning ("Failed to index file: %s", error->message);
      return;
    }

  g_snprintf (num, sizeof (num), "%u", file_id);

  /*
   * Storing file_name:id and id:file_name into index, file_name:id will be
   * used to check whether a file is there in index or not.
   */
  dzl_fuzzy_index_builder_set_metadata_uint32 (fuzzy_builder, file_name, file_id);
  dzl_fuzzy_index_builder_set_metadata_string (fuzzy_builder, num, file_name);

  while (NULL != (indexentryptr = ide_code_index_entries_get_next_entry (entries)))
    {
      g_autoptr(IdeCodeIndexEntry) entry = indexentryptr;
      const gchar *key;
      const gchar *name;
      IdeSymbolKind kind;
      IdeSymbolFlags flags;
      guint begin_line;
      guint begin_line_offset;

      key = ide_code_index_entry_get_key (entry);
      name  = ide_code_index_entry_get_name (entry);
      kind  = ide_code_index_entry_get_kind (entry);
      flags  = ide_code_index_entry_get_flags (entry);

      ide_code_index_entry_get_range (entry,
                                      &begin_line,
                                      &begin_line_offset,
                                      NULL,
                                      NULL);

      /* In our index lines and offsets are 1-based */
      if (key != NULL)
        ide_persistent_map_builder_insert (map_builder,
                                           key,
                                           g_variant_new ("(uuuu)",
                                             file_id,
                                             begin_line, begin_line_offset,
                                             flags),
                                           flags & IDE_SYMBOL_FLAGS_IS_DEFINITION);
      if (name != NULL)
        dzl_fuzzy_index_builder_insert (fuzzy_builder,
                                        name,
                                        g_variant_new ("(uuuuu)",
                                          file_id,
                                          begin_line, begin_line_offset,
                                          flags, kind),
                                        0);
    }
}

static void
ide_code_index_builder_index_directory (GTask        *task,
                                        gpointer      source_object,
                                        gpointer      task_data_ptr,
                                        GCancellable *cancellable)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)source_object;
  IndexingData *data = task_data_ptr;
  g_autoptr(GFile) keys_file = NULL;
  g_autoptr(GFile) names_file = NULL;
  g_autoptr(IdePersistentMapBuilder) map_builder = NULL;
  g_autoptr(DzlFuzzyIndexBuilder) fuzzy_builder = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (data != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  map_builder = ide_persistent_map_builder_new ();
  fuzzy_builder = dzl_fuzzy_index_builder_new ();

  g_file_make_directory_with_parents (data->destination, NULL, NULL);

  g_debug ("Indexing directory");

  for (guint i = 0; i < data->files->len; i++)
    {
      if (g_task_return_error_if_cancelled (task))
        return;

      ide_code_index_builder_index_file (self,
                                         g_ptr_array_index (data->files, i),
                                         i + 1,
                                         map_builder,
                                         fuzzy_builder,
                                         task);
    }

  g_debug ("Writing directory index");

  keys_file = g_file_get_child (data->destination, "SymbolKeys");

  if (!ide_persistent_map_builder_write (map_builder,
                                         keys_file,
                                         G_PRIORITY_LOW,
                                         cancellable,
                                         &error))
    {
      g_message ("Unable to write keys map, %s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  dzl_fuzzy_index_builder_set_metadata_uint32 (fuzzy_builder,
                                               "n_files",
                                               data->files->len);

  names_file = g_file_get_child (data->destination, "SymbolNames");

  if (!dzl_fuzzy_index_builder_write (fuzzy_builder,
                                      names_file,
                                      G_PRIORITY_LOW,
                                      cancellable,
                                      &error))
    {
      g_message ("Unable to write fuzzy index, %s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (!ide_code_index_index_load (self->index, data->destination, cancellable, &error))
    {
      g_message ("Unable to load indexes, %s", error->message);
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
ide_code_index_builder_index_directory_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(GTask) task = user_data;
  guint n_threads;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_TASK (task));

  g_mutex_lock (&self->indexed);

  n_threads = GPOINTER_TO_UINT (g_task_get_task_data (task)) - 1;

  if (n_threads)
    g_task_set_task_data (task, GUINT_TO_POINTER (n_threads), NULL);
  else
    g_task_return_boolean (task, TRUE);

  g_mutex_unlock (&self->indexed);
}

static void
ide_code_index_builder_index_directories_async (IdeCodeIndexBuilder *self,
                                                GPtrArray           *changes,
                                                GCancellable        *cancellable,
                                                GAsyncReadyCallback  callback,
                                                gpointer             user_data)
{
  g_autoptr(GTask) dirs_task = NULL;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (changes != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  dirs_task = g_task_new (self, cancellable, callback, user_data);

  g_task_set_priority (dirs_task, G_PRIORITY_LOW);
  g_task_set_task_data (dirs_task, GUINT_TO_POINTER (changes->len), NULL);

  for (guint i = 0; i < changes->len; i++)
    {
      g_autoptr(GTask) dir_task = NULL;
      IndexingData *idata;

      dir_task = g_task_new (self,
                             cancellable,
                             ide_code_index_builder_index_directory_cb,
                             g_object_ref (dirs_task));

      idata = g_ptr_array_index (changes, i);
      g_ptr_array_index (changes, i) = NULL;

      g_task_set_priority (dir_task, G_PRIORITY_LOW);
      g_task_set_source_tag (dir_task, ide_code_index_builder_index_directories_async);
      g_task_set_task_data (dir_task, idata, (GDestroyNotify)indexing_data_free);

      ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER,
                                 dir_task,
                                 ide_code_index_builder_index_directory);
    }
}

static gboolean
ide_code_index_builder_index_directories_finish (IdeCodeIndexBuilder *self,
                                                 GAsyncResult        *result,
                                                 GError             **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (task));

  return g_task_propagate_boolean (task, error);
}

/* Get Changes: get all directories which are newer than index */

static void
ide_code_index_builder_get_changes (IdeCodeIndexBuilder *self,
                                    GFile               *directory,
                                    GFile               *destination,
                                    gboolean             recursive,
                                    GPtrArray           *changes,
                                    GCancellable        *cancellable)
{
  g_autoptr(GPtrArray) files = NULL;
  g_autoptr(GPtrArray) directories = NULL;
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  gpointer infoptr;
  GTimeVal max_mod_time = { 0 };
  IdeVcs *vcs;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (G_IS_FILE (destination));

  vcs = ide_context_get_vcs (ide_object_get_context (IDE_OBJECT (self)));

  if (ide_vcs_is_ignored (vcs, directory, NULL))
    return;

  if (NULL == (enumerator = g_file_enumerate_children (directory,
                                                       G_FILE_ATTRIBUTE_STANDARD_NAME","
                                                       G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                                       G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                       NULL,
                                                       &error)))
    {
      g_message ("Failed to get children, %s", error->message);
      return;
    }

  files = g_ptr_array_new_with_free_func (g_object_unref);
  directories = g_ptr_array_new_with_free_func (g_free);

  while (NULL != (infoptr = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
    {
      g_autoptr(GFileInfo) info = infoptr;
      const gchar *file_name;
      GFileType type;

      file_name = g_file_info_get_name (info);
      type = g_file_info_get_file_type (info);

      if (type == G_FILE_TYPE_DIRECTORY && recursive)
        {
          g_ptr_array_add (directories, g_strdup (file_name));
        }
      else if (type == G_FILE_TYPE_REGULAR)
        {
          if (NULL != ide_code_index_service_get_code_indexer (self->service, file_name))
            {
              GTimeVal mod_time;
              g_autoptr(GFile) file = NULL;

              g_file_info_get_modification_time (info, &mod_time);

              if (timeval_compare (mod_time, max_mod_time))
                  max_mod_time = mod_time;

              file = g_file_get_child (directory, file_name);

              if (!ide_vcs_is_ignored (vcs, file, NULL))
                g_ptr_array_add (files, g_steal_pointer (&file));
            }
        }
    }

  g_file_enumerator_close (enumerator, cancellable, NULL);

  if ((files->len != 0) &&
      !ide_code_index_index_load_if_nmod (self->index,
                                          destination,
                                          files,
                                          max_mod_time,
                                          cancellable,
                                          NULL))
    {
      IndexingData *idata;

      idata = g_slice_new0 (IndexingData);
      idata->files = g_ptr_array_ref (files);
      idata->destination = g_object_ref (destination);
      g_ptr_array_add (changes, idata);
    }

  for (guint i = 0; i < directories->len; i++)
    {
      const gchar *file_name;
      g_autoptr(GFile) sub_dir = NULL;
      g_autoptr(GFile) sub_dest = NULL;

      file_name = g_ptr_array_index (directories, i);
      sub_dir = g_file_get_child (directory, file_name);
      sub_dest = g_file_get_child (destination, file_name);

      ide_code_index_builder_get_changes (self,
                                          sub_dir,
                                          sub_dest,
                                          recursive,
                                          changes,
                                          cancellable);
    }
}

static void
ide_code_index_builder_get_changes_worker (GTask        *task,
                                           gpointer      source_object,
                                           gpointer      task_data_ptr,
                                           GCancellable *cancellable)
{
  IdeCodeIndexBuilder *self = source_object;
  IdeContext *context;
  GFile *workdir;
  g_autoptr(GFile) destination = NULL;
  g_autofree gchar *relative_path = NULL;
  GetChangesTaskData *data = task_data_ptr;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_TASK (task));
  g_assert (data != NULL);

  context = ide_object_get_context (IDE_OBJECT (self));
  workdir = ide_vcs_get_working_directory (ide_context_get_vcs (context));
  relative_path = g_file_get_relative_path (workdir, data->directory);
  destination = ide_context_cache_file (context, "code-index", relative_path, NULL);

  data->changes = g_ptr_array_new_with_free_func ((GDestroyNotify)indexing_data_free);

  if (g_task_return_error_if_cancelled (task))
    return;

  ide_code_index_builder_get_changes (self,
                                      data->directory,
                                      destination,
                                      data->recursive,
                                      data->changes,
                                      cancellable);

  g_task_return_pointer (task, g_ptr_array_ref (data->changes), (GDestroyNotify)g_ptr_array_unref);
}

static void
ide_code_index_builder_get_changes_async (IdeCodeIndexBuilder *self,
                                          GFile               *directory,
                                          gboolean             recursive,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GetChangesTaskData *data;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_debug ("Getting file changes");

  data = g_slice_new0 (GetChangesTaskData);

  data->directory = g_object_ref (directory);
  data->recursive = recursive;

  task = g_task_new (self, cancellable, callback, user_data);

  g_task_set_task_data (task, data, (GDestroyNotify)get_changes_task_data_free);
  g_task_set_priority (task, G_PRIORITY_LOW);
  g_task_set_source_tag (task, ide_code_index_builder_get_changes_async);

  ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER,
                             task,
                             ide_code_index_builder_get_changes_worker);
}

static GPtrArray *
ide_code_index_builder_get_changes_finish (IdeCodeIndexBuilder *self,
                                           GAsyncResult        *result,
                                           GError             **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (task));

  return g_task_propagate_pointer (task, error);
}

 /* Main task: get changes, retrive build flags and index directories */

static void
ide_code_index_builder_build_cb3 (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(GTask) main_task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_TASK (main_task));

  if (ide_code_index_builder_index_directories_finish (self, result, &error))
    g_task_return_boolean (main_task, TRUE);
  else
    g_task_return_error (main_task, g_steal_pointer (&error));
}

static void
ide_code_index_builder_build_cb2 (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(GHashTable) build_flags = NULL;
  g_autoptr(GTask) main_task = user_data;
  g_autoptr(GError) error = NULL;
  IdeCodeIndexBuilder *self;
  GCancellable *cancellable;
  GPtrArray *changes;
  IdeFile *key;
  gchar **value;
  GHashTableIter iter;

  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (G_IS_TASK (main_task));

  build_flags = ide_build_system_get_build_flags_for_files_finish (build_system, result, &error);

  if (build_flags == NULL)
    {
      g_message ("Failed to fetch build flags %s", error->message);
      g_task_return_error (main_task, g_steal_pointer (&error));
      return;
    }

  if (g_task_return_error_if_cancelled (main_task))
    return;

  self = g_task_get_source_object (main_task);
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));

  cancellable = g_task_get_cancellable (main_task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  changes = g_task_get_task_data (main_task);
  g_assert (changes != NULL);

  /* Update self->build_flags hash table with new flags */
  g_hash_table_iter_init (&iter, build_flags);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      g_hash_table_iter_steal (&iter);
      g_hash_table_insert (self->build_flags, key, value);
    }

  ide_code_index_builder_index_directories_async (self,
                                                  changes,
                                                  cancellable,
                                                  ide_code_index_builder_build_cb3,
                                                  g_steal_pointer (&main_task));
}

static void
ide_code_index_builder_build_cb (GObject      *object,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(GTask) main_task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) changes = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  g_autoptr(GPtrArray) files = NULL;
  GCancellable *cancellable;

  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_TASK (main_task));

  if (NULL == (changes = ide_code_index_builder_get_changes_finish (self, result, &error)))
    {
      g_message ("Failed to get file changes, %s", error->message);
      g_task_return_error (main_task, g_steal_pointer (&error));
      return;
    }
  else if (changes->len == 0)
    {
      g_debug ("No changes are there, completing task");
      g_task_return_boolean (main_task, TRUE);
      return;
    }
  else if (g_task_return_error_if_cancelled (main_task))
    {
      return;
    }

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);
  files = ide_code_index_builder_get_all_files (self, changes);
  cancellable = g_task_get_cancellable (main_task);

  g_message ("Getting build flags for %d directories", changes->len);

  g_task_set_task_data (main_task,
                        g_steal_pointer (&changes),
                        (GDestroyNotify)g_ptr_array_unref);

  /* TODO: add time out to finish task. This will help of build system fails to get flags */

  ide_build_system_get_build_flags_for_files_async (build_system,
                                                    files,
                                                    cancellable,
                                                    ide_code_index_builder_build_cb2,
                                                    g_steal_pointer (&main_task));
}

/* This function will index a directory (recursively) and load that index */
void
ide_code_index_builder_build_async (IdeCodeIndexBuilder *self,
                                    GFile               *directory,
                                    gboolean             recursive,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) main_task = NULL;

  g_return_if_fail (IDE_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_debug ("Started building index");

  main_task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (main_task, G_PRIORITY_LOW);
  g_task_set_source_tag (main_task, ide_code_index_builder_build_async);

  if (g_task_return_error_if_cancelled (main_task))
    return;

  ide_code_index_builder_get_changes_async (self,
                                            directory,
                                            recursive,
                                            cancellable,
                                            ide_code_index_builder_build_cb,
                                            g_steal_pointer (&main_task));
}

gboolean
ide_code_index_builder_build_finish (IdeCodeIndexBuilder  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  GTask *main_task = (GTask *)result;

  g_return_val_if_fail (G_IS_TASK (main_task), FALSE);

  return g_task_propagate_boolean (main_task, error);
}

static void
ide_code_index_builder_finalize (GObject *object)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;

  g_clear_object (&self->index);
  g_clear_object (&self->service);
  g_clear_pointer (&self->build_flags, g_hash_table_unref);
  g_mutex_clear (&self->indexed);

  G_OBJECT_CLASS(ide_code_index_builder_parent_class)->finalize (object);
}

static void
ide_code_index_builder_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      g_value_set_object (value, self->index);
      break;

    case PROP_SERVICE:
      g_value_set_object (value, self->service);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_builder_set_property (GObject       *object,
                                    guint          prop_id,
                                    const GValue  *value,
                                    GParamSpec    *pspec)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;

  switch (prop_id)
    {
    case PROP_INDEX:
      self->index = g_value_dup_object (value);
      break;

    case PROP_SERVICE:
      self->service = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_code_index_builder_init (IdeCodeIndexBuilder *self)
{
  self->build_flags = g_hash_table_new_full ((GHashFunc)ide_file_hash,
                                             (GEqualFunc)ide_file_equal,
                                             g_object_unref,
                                             (GDestroyNotify)g_strfreev);
  g_mutex_init (&self->indexed);
}

static void
ide_code_index_builder_class_init (IdeCodeIndexBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_code_index_builder_finalize;
  object_class->get_property = ide_code_index_builder_get_property;
  object_class->set_property = ide_code_index_builder_set_property;

  properties [PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "Index in which all symbols are stored.",
                         IDE_TYPE_CODE_INDEX_INDEX,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_SERVICE] =
    g_param_spec_object ("service",
                         "Service",
                         "IdeCodeIndexService.",
                         IDE_TYPE_CODE_INDEX_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

IdeCodeIndexBuilder *
ide_code_index_builder_new (IdeContext          *context,
                            IdeCodeIndexIndex   *index,
                            IdeCodeIndexService *service)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (index), NULL);

  return g_object_new (IDE_TYPE_CODE_INDEX_BUILDER,
                       "context", context,
                       "index", index,
                       "service", service,
                       NULL);
}

void
ide_code_index_builder_drop_caches (IdeCodeIndexBuilder *self)
{
  g_return_if_fail (IDE_IS_CODE_INDEX_BUILDER (self));

  /*
   * Drop our caches so that we force the data to be regenereted
   * upon the next request. Also helps keep IdeFile from lingering
   * around forever.
   */

  g_hash_table_remove_all (self->build_flags);
}
