/* ide-code-index-builder.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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
#include <string.h>

#include "ide-code-index-builder.h"

struct _IdeCodeIndexBuilder
{
  IdeObject            parent;
  IdeCodeIndexService *service;
  IdeCodeIndexIndex   *index;
};

#define ADD_ENTRIES_CHUNK_SIZE  5
#define BUILD_DATA_MAGIC        0x778124
#define IS_BUILD_DATA(d)        ((d)->magic == BUILD_DATA_MAGIC)
#define GET_CHANGES_MAGIC       0x912828
#define IS_GET_CHANGES(d)       ((d)->magic == GET_CHANGES_MAGIC)
#define FILE_INFO_MAGIC         0x112840
#define IS_FILE_INFO(d)         ((d)->magic == FILE_INFO_MAGIC)
#define INDEX_DIRECTORY_MAGIC   0x133801
#define IS_INDEX_DIRECTORY(d)   ((d)->magic == INDEX_DIRECTORY_MAGIC)

typedef struct
{
  guint           magic;
  GFile          *data_dir;
  GFile          *index_dir;
  GFile          *building_data_dir;
  GFile          *building_index_dir;
  IdeBuildSystem *build_system;
  GPtrArray      *changes;
} BuildData;

typedef struct
{
  GPtrArray *specs;
  GPtrArray *mime_types;
} IndexerInfo;

typedef struct
{
  guint        magic;
  GFile       *data_dir;
  GFile       *index_dir;
  IdeVcs      *vcs;
  IndexerInfo *indexers;
  GQueue       directories;
  guint        recursive : 1;
} GetChangesData;

typedef struct
{
  guint        magic;
  GFile       *directory;
  gchar       *name;
  const gchar *content_type;
  GTimeVal     mtime;
  GFileType    file_type;
} FileInfo;

typedef struct
{
  guint                    magic;
  IdePersistentMapBuilder *map;
  DzlFuzzyIndexBuilder    *fuzzy;
  GFile                   *index_dir;
  guint                    n_active;
  guint                    n_files;
} IndexDirectoryData;

typedef struct
{
  IdeCodeIndexEntries     *entries;
  IdePersistentMapBuilder *map_builder;
  DzlFuzzyIndexBuilder    *fuzzy_builder;
  guint32                  file_id;
} AddEntriesData;

enum {
  PROP_0,
  PROP_INDEX,
  PROP_SERVICE,
  N_PROPS
};

static void build_tick (IdeTask *task);

G_DEFINE_TYPE (IdeCodeIndexBuilder, ide_code_index_builder, IDE_TYPE_OBJECT)

static GParamSpec *properties [N_PROPS];

static void
build_data_free (BuildData *self)
{
  g_clear_object (&self->build_system);
  g_clear_object (&self->data_dir);
  g_clear_object (&self->index_dir);
  g_clear_object (&self->building_data_dir);
  g_clear_object (&self->building_index_dir);
  g_clear_pointer (&self->changes, g_ptr_array_unref);
  g_slice_free (BuildData, self);
}

static void
indexer_info_free (IndexerInfo *info)
{
  g_clear_pointer (&info->specs, g_ptr_array_unref);
  g_clear_pointer (&info->mime_types, g_ptr_array_unref);
  g_slice_free (IndexerInfo, info);
}

static void
get_changes_data_free (GetChangesData *self)
{
  g_clear_pointer (&self->indexers, indexer_info_free);
  g_clear_object (&self->data_dir);
  g_clear_object (&self->index_dir);
  g_clear_object (&self->vcs);
  g_queue_foreach (&self->directories, (GFunc)g_object_unref, NULL);
  g_queue_clear (&self->directories);
  g_slice_free (GetChangesData, self);
}

static void
add_entries_data_free (AddEntriesData *self)
{
  g_clear_object (&self->entries);
  g_clear_object (&self->map_builder);
  g_clear_object (&self->fuzzy_builder);
  g_slice_free (AddEntriesData, self);
}

static void
file_info_free (FileInfo *file_info)
{
  g_clear_pointer (&file_info->name, g_free);
  g_clear_object (&file_info->directory);
  g_slice_free (FileInfo, file_info);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (FileInfo, file_info_free)

static void
index_directory_data_free (IndexDirectoryData *self)
{
  g_assert (self != NULL);
  g_assert (self->n_active == 0);

  g_clear_object (&self->map);
  g_clear_object (&self->fuzzy);
  g_clear_object (&self->index_dir);
  g_slice_free (IndexDirectoryData, self);
}

static IndexerInfo *
collect_indexer_info (void)
{
  GtkSourceLanguageManager *manager;
  const GList *plugins;
  PeasEngine *engine;
  IndexerInfo *info;

  manager = gtk_source_language_manager_get_default ();
  engine = peas_engine_get_default ();
  plugins = peas_engine_get_plugin_list (engine);

  info = g_slice_new0 (IndexerInfo);
  info->specs = g_ptr_array_new_with_free_func ((GDestroyNotify)g_pattern_spec_free);
  info->mime_types = g_ptr_array_new ();

  for (; plugins != NULL; plugins = plugins->next)
    {
      const PeasPluginInfo *plugin_info = plugins->data;
      g_auto(GStrv) split = NULL;
      const gchar *str;

      if (!peas_plugin_info_is_loaded (plugin_info))
        continue;

      if (!(str = peas_plugin_info_get_external_data (plugin_info, "Code-Indexer-Languages")))
        continue;

      split = g_strsplit (str, ",", 0);

      for (guint i = 0; split[i] != NULL; i++)
        {
          const gchar *name = split[i];
          GtkSourceLanguage *lang;
          g_auto(GStrv) globs = NULL;
          g_auto(GStrv) mime_types = NULL;

          if (!(lang = gtk_source_language_manager_get_language (manager, name)))
            continue;

          globs = gtk_source_language_get_globs (lang);
          mime_types = gtk_source_language_get_mime_types (lang);

          for (guint j = 0; globs[j] != NULL; j++)
            {
              g_autoptr(GPatternSpec) spec = g_pattern_spec_new (globs[j]);
              g_ptr_array_add (info->specs, g_steal_pointer (&spec));
            }

          for (guint j = 0; mime_types[j]; j++)
            g_ptr_array_add (info->mime_types, (gchar *)g_intern_string (mime_types[j]));
        }
    }

  return info;
}

static gboolean
has_supported_indexer (const IndexerInfo *info,
                       const gchar       *filename,
                       const gchar       *mime_type)
{
  g_autofree gchar *reversed = NULL;
  guint len;

  g_assert (info != NULL);
  g_assert (info->specs != NULL);
  g_assert (info->mime_types != NULL);
  g_assert (filename != NULL);

  if (mime_type != NULL)
    {
      for (guint i = 0; i < info->mime_types->len; i++)
        {
          const gchar *mt = g_ptr_array_index (info->mime_types, i);

          /* interned strings use pointer comparison */
          if (mt == mime_type)
            return TRUE;
        }
    }

  len = strlen (filename);
  reversed = g_utf8_strreverse (filename, len);

  for (guint i = 0; i < info->specs->len; i++)
    {
      GPatternSpec *spec = g_ptr_array_index (info->specs, i);

      if (g_pattern_match (spec, len, filename, reversed))
        return TRUE;
    }

  return FALSE;
}

static gint
timeval_compare (const GTimeVal *a,
                 const GTimeVal *b)
{
  if (memcmp (a, b, sizeof *a) == 0)
    return 0;
  else if (a->tv_sec < b->tv_sec || (a->tv_sec == b->tv_sec && a->tv_usec < b->tv_usec))
    return -1;
  else
    return 1;
}

static void
maybe_log_error (const GError *error)
{
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
    return;

  if (error != NULL)
    g_warning ("%s", error->message);
}

static void
ide_code_index_builder_dispose (GObject *object)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;

  g_clear_object (&self->index);
  g_clear_object (&self->service);

  G_OBJECT_CLASS (ide_code_index_builder_parent_class)->dispose (object);
}

static void
ide_code_index_builder_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  IdeCodeIndexBuilder *self = IDE_CODE_INDEX_BUILDER (object);

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
ide_code_index_builder_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  IdeCodeIndexBuilder *self = IDE_CODE_INDEX_BUILDER (object);

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
ide_code_index_builder_class_init (IdeCodeIndexBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_code_index_builder_dispose;
  object_class->get_property = ide_code_index_builder_get_property;
  object_class->set_property = ide_code_index_builder_set_property;

  properties [PROP_INDEX] =
    g_param_spec_object ("index",
                         "Index",
                         "The index to update after building sub-indexes",
                         IDE_TYPE_CODE_INDEX_INDEX,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties [PROP_SERVICE] =
    g_param_spec_object ("service",
                         "Service",
                         "The service to query for various build information",
                         IDE_TYPE_CODE_INDEX_SERVICE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_code_index_builder_init (IdeCodeIndexBuilder *self)
{
}

IdeCodeIndexBuilder *
ide_code_index_builder_new (IdeContext          *context,
                            IdeCodeIndexService *service,
                            IdeCodeIndexIndex   *index)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_SERVICE (service), NULL);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_INDEX (index), NULL);

  return g_object_new (IDE_TYPE_CODE_INDEX_BUILDER,
                       "context", context,
                       "service", service,
                       "index", index,
                       NULL);
}

static GFile *
get_index_dir (GFile *index_root,
               GFile *data_root,
               GFile *directory)
{
  g_autofree gchar *relative = NULL;

  g_assert (G_IS_FILE (index_root));
  g_assert (G_IS_FILE (data_root));
  g_assert (G_IS_FILE (directory));
  g_assert (g_file_equal (data_root, directory) ||
            g_file_has_prefix (directory, data_root));

  relative = g_file_get_relative_path (data_root, directory);

  if (relative != NULL)
    return g_file_get_child (index_root, relative);
  else
    return g_object_ref (index_root);
}

static void
remove_indexes_in_dir (GFile        *index_dir,
                       GCancellable *cancellable)
{
  g_autoptr(GFile) keys = NULL;
  g_autoptr(GFile) names = NULL;

  g_assert (G_IS_FILE (index_dir));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  keys = g_file_get_child (index_dir, "SymbolKeys");
  names = g_file_get_child (index_dir, "SymbolNames");

  g_file_delete (keys, cancellable, NULL);
  g_file_delete (names, cancellable, NULL);
}

static gboolean
directory_needs_update (GFile        *index_dir,
                        GFile        *directory,
                        GQueue       *file_infos,
                        GCancellable *cancellable)
{
  g_autoptr(GFile) names_file = NULL;
  g_autoptr(GFileInfo) names_info = NULL;
  g_autoptr(DzlFuzzyIndex) index = NULL;
  GTimeVal mtime;
  guint n_files;

  g_assert (G_IS_FILE (index_dir));
  g_assert (G_IS_FILE (directory));
  g_assert (file_infos != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  names_file = g_file_get_child (index_dir, "SymbolNames");
  names_info = g_file_query_info (names_file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  cancellable,
                                  NULL);
  if (names_info == NULL)
    return TRUE;

  g_file_info_get_modification_time (names_info, &mtime);

  /* If the mtime of a file is newer than the index, it needs indexing */
  for (const GList *iter = file_infos->head; iter != NULL; iter = iter->next)
    {
      const FileInfo *info = iter->data;

      if (timeval_compare (&info->mtime, &mtime) > 0)
        return TRUE;
    }

  /* Load the SymbolNames index for this directory */
  index = dzl_fuzzy_index_new ();
  if (!dzl_fuzzy_index_load_file (index, names_file, cancellable, NULL))
    return TRUE;

  /* If the file number count is off, it needs indexing */
  n_files = dzl_fuzzy_index_get_metadata_uint32 (index, "n_files");
  if (n_files != file_infos->length)
    return TRUE;

  /* If any file names are missing from metadata, it needs indexing */
  for (const GList *iter = file_infos->head; iter != NULL; iter = iter->next)
    {
      const FileInfo *info = iter->data;
      g_autoptr(GFile) file = g_file_get_child (info->directory, info->name);
      g_autofree gchar *path = g_file_get_path (file);

      if (!dzl_fuzzy_index_get_metadata_uint32 (index, path))
        return TRUE;
    }

  return FALSE;
}

static void
filter_ignored (IdeVcs            *vcs,
                GQueue            *file_infos,
                const IndexerInfo *indexers)
{
  g_assert (IDE_IS_VCS (vcs));
  g_assert (file_infos != NULL);

  for (GList *iter = file_infos->head; iter; iter = iter->next)
    {
      FileInfo *info;
      GFile *file;
      gboolean ignore;

    again:
      info = iter->data;

      g_assert (IS_FILE_INFO (info));

      file = g_file_get_child (info->directory, info->name);
      ignore = ide_vcs_is_ignored (vcs, file, NULL);
      g_clear_object (&file);

      if (!ignore)
        ignore = !has_supported_indexer (indexers, info->name, info->content_type);

      if (ignore)
        {
          GList *tmp = iter->next;
          g_queue_delete_link (file_infos, iter);
          if (tmp == NULL)
            break;
          iter = tmp;
          file_info_free (info);
          goto again;
        }
    }
}

static void
find_all_files_typed (GFile        *root,
                      GFileType     req_file_type,
                      gboolean      recursive,
                      GCancellable *cancellable,
                      GFunc         func,
                      gpointer      user_data)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;

  g_assert (G_IS_FILE (root));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (func != NULL);

  enumerator = g_file_enumerate_children (root,
                                          G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK","
                                          G_FILE_ATTRIBUTE_STANDARD_NAME","
                                          G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                                          G_FILE_ATTRIBUTE_STANDARD_TYPE","
                                          G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                          cancellable,
                                          NULL);
  if (enumerator == NULL)
    return;

  for (;;)
    {
      g_autoptr(GFileInfo) info = NULL;
      const gchar *name;
      GFileType file_type;

      if (g_cancellable_is_cancelled (cancellable))
        break;

      if (!(info = g_file_enumerator_next_file (enumerator, cancellable, NULL)))
        break;

      name = g_file_info_get_name (info);
      if (ide_vcs_path_is_ignored (NULL, name, NULL))
        continue;

      file_type = g_file_info_get_file_type (info);

      if (file_type == req_file_type)
        {
          FileInfo *fi;

          fi = g_slice_new0 (FileInfo);
          fi->magic = FILE_INFO_MAGIC;
          fi->directory = g_object_ref (root);
          fi->name = g_strdup (g_file_info_get_name (info));
          fi->file_type = g_file_info_get_file_type (info);
          fi->content_type = g_intern_string (g_file_info_get_content_type (info));
          g_file_info_get_modification_time (info, &fi->mtime);

          func (g_steal_pointer (&fi), user_data);
        }

      if (recursive &&
          !g_file_info_get_is_symlink (info) &&
          file_type == G_FILE_TYPE_DIRECTORY)
        {
          g_autoptr(GFile) child = g_file_enumerator_get_child (enumerator, info);

          find_all_files_typed (child,
                                req_file_type,
                                TRUE,
                                cancellable,
                                func,
                                user_data);
        }
    }

  g_file_enumerator_close (enumerator, NULL, NULL);
}

static void
get_changes_collect_dirs_cb (gpointer data,
                             gpointer user_data)
{
  g_autoptr(FileInfo) fi = data;
  GQueue *queue = user_data;

  g_assert (fi != NULL);
  g_assert (G_IS_FILE (fi->directory));
  g_assert (fi->name != NULL);
  g_assert (queue != NULL);

  g_queue_push_tail (queue, g_file_get_child (fi->directory, fi->name));
}

static void
get_changes_collect_files_cb (gpointer data,
                              gpointer user_data)
{
  g_autoptr(FileInfo) fi = data;
  GQueue *queue = user_data;

  g_assert (fi != NULL);
  g_assert (G_IS_FILE (fi->directory));
  g_assert (fi->name != NULL);
  g_assert (queue != NULL);

  g_queue_push_tail (queue, g_steal_pointer (&fi));
}

static void
get_changes_worker (IdeTask      *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  g_autoptr(GPtrArray) to_update = NULL;
  GetChangesData *gcd = task_data;

  g_assert (!IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (source_object));
  g_assert (gcd != NULL);
  g_assert (IS_GET_CHANGES (gcd));
  g_assert (G_IS_FILE (gcd->data_dir));
  g_assert (G_IS_FILE (gcd->index_dir));
  g_assert (IDE_IS_VCS (gcd->vcs));

  if (ide_task_return_error_if_cancelled (task))
    return;

  /*
   * If we are recursive, collect all the directories we need to look
   * at to locate changes.
   */
  if (gcd->recursive)
    find_all_files_typed (gcd->data_dir,
                          G_FILE_TYPE_DIRECTORY,
                          TRUE,
                          cancellable,
                          get_changes_collect_dirs_cb,
                          &gcd->directories);

  /*
   * We'll keep track of all the directories which contain data
   * that is invalid and needs updating.
   */
  to_update = g_ptr_array_new_with_free_func (g_object_unref);

  if (ide_task_return_error_if_cancelled (task))
    return;

  /*
   * Process directories to check for changes, while ensuring we have not
   * been asynchronously cancelled.
   */
  while (!g_queue_is_empty (&gcd->directories))
    {
      g_autofree gchar *relative = NULL;
      g_autoptr(GFile) dir = g_queue_pop_head (&gcd->directories);
      g_autoptr(GFile) index_dir = NULL;
      g_auto(GQueue) files = G_QUEUE_INIT;

      g_assert (G_IS_FILE (dir));

      find_all_files_typed (dir,
                            G_FILE_TYPE_REGULAR,
                            FALSE,
                            cancellable,
                            get_changes_collect_files_cb,
                            &files);

      filter_ignored (gcd->vcs, &files, gcd->indexers);

      if (!(relative = g_file_get_relative_path (gcd->index_dir, dir)))
        index_dir = g_object_ref (gcd->index_dir);
      else
        index_dir = g_file_get_child (gcd->index_dir, relative);

      if (files.length == 0)
        remove_indexes_in_dir (index_dir, cancellable);
      else if (directory_needs_update (index_dir, dir, &files, cancellable))
        g_ptr_array_add (to_update, g_steal_pointer (&dir));

      g_queue_foreach (&files, (GFunc)file_info_free, NULL);

      if (ide_task_return_error_if_cancelled (task))
        return;
    }

  /* In case we were cancelled */
  if (gcd->directories.length > 0)
    {
      g_queue_foreach (&gcd->directories, (GFunc)g_object_unref, NULL);
      g_queue_clear (&gcd->directories);
    }

  g_assert (gcd->directories.length == 0);
  g_assert (IDE_IS_VCS (gcd->vcs));
  g_assert (G_IS_FILE (gcd->data_dir));
  g_assert (G_IS_FILE (gcd->index_dir));

  ide_task_return_pointer (task,
                           g_steal_pointer (&to_update),
                           (GDestroyNotify) g_ptr_array_unref);
}

/*
 * get_changes_async:
 * @self: an #IdeCodeIndexBuilder
 * @directory: a #GFile
 * @recursive: if the directories should be recursively scanned
 * @cancellable: a cancellable or %NULL
 * @callback: callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * This asynchronously looks for all of the directories starting from
 * @directoriy (if recursive is set) that contain changes to their
 * existing index.
 */
static void
get_changes_async (IdeCodeIndexBuilder *self,
                   GFile               *data_dir,
                   GFile               *index_dir,
                   gboolean             recursive,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  GetChangesData *gcd;
  IdeContext *context;
  IdeVcs *vcs;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (data_dir));
  g_assert (G_IS_FILE (index_dir));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, get_changes_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  gcd = g_slice_new0 (GetChangesData);
  gcd->magic = GET_CHANGES_MAGIC;
  gcd->indexers = collect_indexer_info ();
  gcd->data_dir = g_object_ref (data_dir);
  gcd->index_dir = g_object_ref (index_dir);
  gcd->recursive = !!recursive;
  gcd->vcs = g_object_ref (vcs);
  ide_task_set_task_data (task, gcd, (GDestroyNotify)get_changes_data_free);

  g_queue_push_head (&gcd->directories, g_object_ref (data_dir));

  ide_task_run_in_thread (task, get_changes_worker);
}

static GPtrArray *
get_changes_finish (IdeCodeIndexBuilder  *self,
                    GAsyncResult         *result,
                    GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/**
 * add_entries_to_index
 * @entries: (element-type Ide.CodeIndexEntry): the entries to add
 * @file_id: the id within the index
 * @map_builder: the persistent map builder to append
 * @fuzzy_builder: the fuzzy index builder to append
 *
 * This will incrementally add entries to the builder.
 */
static void
add_entries_to_index (GPtrArray               *entries,
                      GFile                   *file,
                      guint32                  file_id,
                      IdePersistentMapBuilder *map_builder,
                      DzlFuzzyIndexBuilder    *fuzzy_builder)
{
  g_autofree gchar *filename = NULL;
  gchar num[16];

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (entries != NULL);
  g_assert (G_IS_FILE (file));
  g_assert (file_id > 0);
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (map_builder));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (fuzzy_builder));

  /*
   * Storing file_name:id and id:file_name into index, file_name:id will be
   * used to check whether a file is there in index or not.
   *
   * This can get called multiple times, but it's fine because we're just
   * updating a GVariantDict until the file has been processed.
   */
  g_snprintf (num, sizeof (num), "%u", file_id);
  filename = g_file_get_path (file);
  dzl_fuzzy_index_builder_set_metadata_uint32 (fuzzy_builder, filename, file_id);
  dzl_fuzzy_index_builder_set_metadata_string (fuzzy_builder, num, filename);

  IDE_TRACE_MSG ("Adding %u entries for %s", entries->len, filename);

  for (guint i = 0; i < entries->len; i++)
    {
      IdeCodeIndexEntry *entry = g_ptr_array_index (entries, i);
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
                                                          begin_line,
                                                          begin_line_offset,
                                                          flags),
                                           !!(flags & IDE_SYMBOL_FLAGS_IS_DEFINITION));

      if (name != NULL)
        dzl_fuzzy_index_builder_insert (fuzzy_builder,
                                        name,
                                        g_variant_new ("(uuuuu)",
                                                       file_id,
                                                       begin_line,
                                                       begin_line_offset,
                                                       flags,
                                                       kind),
                                        0);
    }
}

static void
index_directory_worker (IdeTask      *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  IndexDirectoryData *idd = task_data;
  g_autoptr(GFile) names = NULL;
  g_autoptr(GFile) keys = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CODE_INDEX_BUILDER (source_object));
  g_assert (idd != NULL);
  g_assert (IS_INDEX_DIRECTORY (idd));
  g_assert (G_IS_FILE (idd->index_dir));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (idd->fuzzy));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (idd->map));

  keys = g_file_get_child (idd->index_dir, "SymbolKeys");
  names = g_file_get_child (idd->index_dir, "SymbolNames");

  /* Ignore result, as it will set @error if index_dir exists */
  g_file_make_directory_with_parents (idd->index_dir, cancellable, NULL);

  if (!ide_persistent_map_builder_write (idd->map, keys, 0, cancellable, &error) ||
      !dzl_fuzzy_index_builder_write (idd->fuzzy, names, 0, cancellable, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
add_entries_to_index_next_entries_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  IdeCodeIndexEntries *entries = (IdeCodeIndexEntries *)object;
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  AddEntriesData *task_data;
  GCancellable *cancellable;

  g_assert (IDE_IS_CODE_INDEX_ENTRIES (entries));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  ret = ide_code_index_entries_next_entries_finish (entries, result, &error);
  IDE_PTR_ARRAY_SET_FREE_FUNC (ret, ide_code_index_entry_free);

  if (error != NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if (ret == NULL || ret->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  cancellable = ide_task_get_cancellable (task);
  task_data = ide_task_get_task_data (task);

  g_assert (task_data != NULL);
  g_assert (IDE_IS_CODE_INDEX_ENTRIES (task_data->entries));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (task_data->map_builder));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (task_data->fuzzy_builder));
  g_assert (task_data->file_id > 0);

  file = ide_code_index_entries_get_file (entries);

  add_entries_to_index (ret,
                        file,
                        task_data->file_id,
                        task_data->map_builder,
                        task_data->fuzzy_builder);

  ide_code_index_entries_next_entries_async (entries,
                                             cancellable,
                                             add_entries_to_index_next_entries_cb,
                                             g_steal_pointer (&task));
}

static void
add_entries_to_index_async (IdeCodeIndexBuilder     *self,
                            IdeCodeIndexEntries     *entries,
                            guint32                  file_id,
                            IdePersistentMapBuilder *map_builder,
                            DzlFuzzyIndexBuilder    *fuzzy_builder,
                            GCancellable            *cancellable,
                            GAsyncReadyCallback      callback,
                            gpointer                 user_data)
{
  g_autoptr(IdeTask) task = NULL;
  AddEntriesData *task_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_CODE_INDEX_ENTRIES (entries));
  g_assert (file_id > 0);
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (map_builder));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (fuzzy_builder));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, add_entries_to_index_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (ide_task_return_error_if_cancelled (task))
    return;

  task_data = g_slice_new0 (AddEntriesData);
  task_data->entries = g_object_ref (entries);
  task_data->map_builder = g_object_ref (map_builder);
  task_data->fuzzy_builder = g_object_ref (fuzzy_builder);
  task_data->file_id = file_id;
  ide_task_set_task_data (task, task_data, (GDestroyNotify)add_entries_data_free);

  ide_code_index_entries_next_entries_async (entries,
                                             cancellable,
                                             add_entries_to_index_next_entries_cb,
                                             g_steal_pointer (&task));
}

static gboolean
add_entries_to_index_finish (IdeCodeIndexBuilder  *self,
                             GAsyncResult         *result,
                             GError              **error)
{
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
dec_active_and_maybe_complete (IdeTask *task)
{
  IndexDirectoryData *idd;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));

  idd = ide_task_get_task_data (task);
  g_assert (idd != NULL);
  g_assert (IS_INDEX_DIRECTORY (idd));
  g_assert (G_IS_FILE (idd->index_dir));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (idd->fuzzy));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (idd->map));

  idd->n_active--;

  if (idd->n_active == 0)
    {
      dzl_fuzzy_index_builder_set_metadata_uint32 (idd->fuzzy, "n_files", idd->n_files);
      ide_task_run_in_thread (task, index_directory_worker);
    }
}

static void
index_directory_add_entries_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!add_entries_to_index_finish (self, result, &error))
    maybe_log_error (error);

  dec_active_and_maybe_complete (task);
}

static void
index_directory_index_file_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeCodeIndexer *indexer = (IdeCodeIndexer *)object;
  g_autoptr(IdeCodeIndexEntries) entries = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  IdeCodeIndexBuilder *self;
  IndexDirectoryData *idd;
  GCancellable *cancellable;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEXER (indexer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  idd = ide_task_get_task_data (task);
  g_assert (idd != NULL);
  g_assert (IS_INDEX_DIRECTORY (idd));
  g_assert (G_IS_FILE (idd->index_dir));
  g_assert (DZL_IS_FUZZY_INDEX_BUILDER (idd->fuzzy));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (idd->map));

  entries = ide_code_indexer_index_file_finish (indexer, result, &error);

  if (entries == NULL)
    {
      maybe_log_error (error);
      dec_active_and_maybe_complete (task);
      return;
    }

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  add_entries_to_index_async (self,
                              entries,
                              ++idd->n_files,
                              idd->map,
                              idd->fuzzy,
                              cancellable,
                              index_directory_add_entries_cb,
                              g_steal_pointer (&task));
}

static void
index_directory_async (IdeCodeIndexBuilder *self,
                       GFile               *data_dir,
                       GFile               *index_dir,
                       GHashTable          *build_flags,
                       GCancellable        *cancellable,
                       GAsyncReadyCallback  callback,
                       gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IndexDirectoryData *idd;
  GHashTableIter iter;
  gpointer k, v;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (data_dir));
  g_assert (G_IS_FILE (index_dir));
  g_assert (build_flags != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, index_directory_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (ide_task_return_error_if_cancelled (task))
    return;

  idd = g_slice_new0 (IndexDirectoryData);
  idd->magic = INDEX_DIRECTORY_MAGIC;
  idd->index_dir = g_object_ref (index_dir);
  idd->fuzzy = dzl_fuzzy_index_builder_new ();
  idd->map = ide_persistent_map_builder_new ();
  ide_task_set_task_data (task, idd, (GDestroyNotify)index_directory_data_free);

  g_hash_table_iter_init (&iter, build_flags);

  idd->n_active++;

  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      IdeFile *file = k;
      const gchar * const *file_flags = v;
      const gchar *path = ide_file_get_path (file);
      GFile *gfile = ide_file_get_file (file);
      IdeCodeIndexer *indexer;

      g_assert (IDE_IS_FILE (file));
      g_assert (G_IS_FILE (gfile));
      g_assert (path != NULL);
      g_assert (IDE_IS_CODE_INDEX_SERVICE (self->service));

      if ((indexer = ide_code_index_service_get_code_indexer (self->service, path)))
        {
          idd->n_active++;
          ide_code_indexer_index_file_async (indexer,
                                             gfile,
                                             file_flags,
                                             cancellable,
                                             index_directory_index_file_cb,
                                             g_object_ref (task));
        }
    }

  idd->n_active--;

  if (idd->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static gboolean
index_directory_finish (IdeCodeIndexBuilder  *self,
                        GAsyncResult         *result,
                        GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (IDE_TASK (result), self));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
build_index_directory_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  BuildData *bd;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (ide_task_return_error_if_cancelled (task))
    return;

  bd = ide_task_get_task_data (task);
  g_assert (bd != NULL);
  g_assert (IS_BUILD_DATA (bd));
  g_assert (G_IS_FILE (bd->building_data_dir));
  g_assert (G_IS_FILE (bd->building_index_dir));

  if (!index_directory_finish (self, result, &error))
    maybe_log_error (error);
  else if (self->index != NULL)
    ide_code_index_index_load (self->index,
                               bd->building_index_dir,
                               bd->building_data_dir,
                               NULL, NULL);

  build_tick (task);
}

static void
build_get_build_flags_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GHashTable) flags = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) data_dir = NULL;
  g_autoptr(GFile) index_dir = NULL;
  IdeCodeIndexBuilder *self;
  GCancellable *cancellable;
  BuildData *bd;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  bd = ide_task_get_task_data (task);
  g_assert (bd != NULL);
  g_assert (G_IS_FILE (bd->data_dir));
  g_assert (G_IS_FILE (bd->index_dir));
  g_assert (bd->changes != NULL);
  g_assert (bd->changes->len > 0);
  g_assert (IDE_IS_BUILD_SYSTEM (bd->build_system));

  data_dir = g_object_ref (g_ptr_array_index (bd->changes, bd->changes->len - 1));
  g_ptr_array_remove_index (bd->changes, bd->changes->len - 1);
  g_assert (G_IS_FILE (data_dir));

  if (!(flags = ide_build_system_get_build_flags_for_dir_finish (build_system, result, &error)))
    {
      maybe_log_error (error);
      build_tick (task);
      return;
    }

  self = ide_task_get_source_object (task);
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  index_dir = get_index_dir (bd->index_dir, bd->data_dir, data_dir);
  g_assert (G_IS_FILE (index_dir));

  g_set_object (&bd->building_index_dir, index_dir);
  g_set_object (&bd->building_data_dir, data_dir);

  g_assert (G_IS_FILE (bd->building_index_dir));
  g_assert (G_IS_FILE (bd->building_data_dir));

  {
    g_autofree gchar *path = g_file_get_path (data_dir);
    g_debug ("Indexing code in directory %s", path);
  }

  index_directory_async (self,
                         data_dir,
                         index_dir,
                         flags,
                         cancellable,
                         build_index_directory_cb,
                         g_steal_pointer (&task));
}

static void
build_tick (IdeTask *task)
{
  GCancellable *cancellable;
  BuildData *bd;
  GFile *data_dir;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_TASK (task));

  if (ide_task_return_error_if_cancelled (task))
    return;

  bd = ide_task_get_task_data (task);
  g_assert (bd != NULL);
  g_assert (IS_BUILD_DATA (bd));
  g_assert (G_IS_FILE (bd->data_dir));
  g_assert (G_IS_FILE (bd->index_dir));
  g_assert (IDE_IS_BUILD_SYSTEM (bd->build_system));

  if (bd->changes->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      return;
    }

  cancellable = ide_task_get_cancellable (task);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  data_dir = g_ptr_array_index (bd->changes, bd->changes->len - 1);
  g_assert (G_IS_FILE (data_dir));

  ide_build_system_get_build_flags_for_dir_async (bd->build_system,
                                                  data_dir,
                                                  cancellable,
                                                  build_get_build_flags_cb,
                                                  g_object_ref (task));
}

static void
build_get_changes_cb (GObject      *object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  IdeCodeIndexBuilder *self = (IdeCodeIndexBuilder *)object;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeTask) task = user_data;
  BuildData *bd;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  bd = ide_task_get_task_data (task);
  g_assert (bd != NULL);
  g_assert (bd->magic == BUILD_DATA_MAGIC);
  g_assert (G_IS_FILE (bd->data_dir));
  g_assert (G_IS_FILE (bd->index_dir));
  g_assert (bd->changes == NULL);
  g_assert (IDE_IS_BUILD_SYSTEM (bd->build_system));

  bd->changes = get_changes_finish (self, result, &error);

  if (bd->changes == NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    build_tick (task);
}

void
ide_code_index_builder_build_async (IdeCodeIndexBuilder *self,
                                    GFile               *directory,
                                    gboolean             recursive,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autofree gchar *relative = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) index_dir = NULL;
  IdeBuildSystem *build_system;
  IdeContext *context;
  BuildData *bd;
  IdeVcs *vcs;
  GFile *workdir;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (IDE_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (G_IS_FILE (directory));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  context = ide_object_get_context (IDE_OBJECT (self));
  build_system = ide_context_get_build_system (context);
  vcs = ide_context_get_vcs (context);
  workdir = ide_vcs_get_working_directory (vcs);
  relative = g_file_get_relative_path (workdir, directory);
  index_dir = ide_context_cache_file (context, "code-index", relative, NULL);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_code_index_builder_build_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  bd = g_slice_new0 (BuildData);
  bd->magic = BUILD_DATA_MAGIC;
  bd->data_dir = g_object_ref (directory);
  bd->index_dir = g_steal_pointer (&index_dir);
  bd->build_system = g_object_ref (build_system);
  ide_task_set_task_data (task, bd, (GDestroyNotify)build_data_free);

  get_changes_async (self,
                     bd->data_dir,
                     bd->index_dir,
                     recursive,
                     cancellable,
                     build_get_changes_cb,
                     g_steal_pointer (&task));
}

gboolean
ide_code_index_builder_build_finish (IdeCodeIndexBuilder  *self,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (IDE_IS_CODE_INDEX_BUILDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}
