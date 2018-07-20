/* ide-gi-repository.c
 *
 * Copyright (C) 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-gi-repository"

#include "config.h"

#include <dazzle.h>
#include <inttypes.h>
#include <stdlib.h>

#include "ide-gi-index.h"
#include "ide-gi-index-private.h"
#include "ide-gi-utils.h"

#include "ide-gi-repository.h"

struct _IdeGiRepository
{
  IdeObject         parent_instance;

  GFile            *builddir;
  gchar            *cache_path;
  gchar            *current_runtime_id;

  GHashTable       *indexer_table;
  GPtrArray        *project_girs;
  GPtrArray        *gir_paths;
  GMutex            project_dirs_mutex;
  IdeGiIndex       *current_indexer;
  IdeBuildPipeline *current_pipeline;
  /* The runtimes we wait for an #IdeGiIndex creation to finish */
  GHashTable       *pending_runtimes;

  guint             update_on_build : 1;
  guint             is_constructed  : 1;
};

G_DEFINE_TYPE (IdeGiRepository, ide_gi_repository, IDE_TYPE_OBJECT)

#define GIR_EXTENSION_LEN 4

enum {
  PROP_0,
  PROP_UPDATE_ON_BUILD,
  PROP_RUNTIME_ID,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  CURRENT_VERSION_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

static void configuration_changed_cb (IdeGiRepository *self);

/**
 * ide_gi_repository_get_project_girs:
 * @self: a #IdeGiRepository
 *
 * Get a #GPtrArray of the project .gir files
 * (the array contains elements only after the build phase of the pipeline)
 *
 * Returns: (transfer full) (element-type Gio.File): A #GPtrArray of #GFile.
 */
GPtrArray *
ide_gi_repository_get_project_girs (IdeGiRepository *self)
{
  GPtrArray *ar;

  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);
  g_mutex_lock (&self->project_dirs_mutex);

  if (self->project_girs != NULL)
    for (guint i = 0; i < self->project_girs->len; i++)
      g_ptr_array_add (ar, g_object_ref (g_ptr_array_index (self->project_girs, i)));

  g_mutex_unlock (&self->project_dirs_mutex);

  return ar;
}

/**
 * ide_gi_repository_get_cache_path:
 * @self: a #IdeGiRepository
 *
 * Get the path of the cache used by the #IdeGiRepository.
 *
 * Returns: (transfer none): The cache path.
 */
const gchar *
ide_gi_repository_get_cache_path (IdeGiRepository *self)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);

  return self->cache_path;
}

/**
 * ide_gi_repository_get_current_runtime_id:
 *
 * Get the current runtime id set on the #IdeGiRepository.
 * The runtime id may not have a matching #IdeGiIndex yet.
 * (creation and update in process)
 *
 * Returns: (transfer none): The current runtime id.
 */
const gchar *
ide_gi_repository_get_current_runtime_id (IdeGiRepository *self)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);

  return self->current_runtime_id;
}

/**
 * ide_gi_repository_get_current_version:
 * @self: #IdeGiRepository
 *
 * Get a ref on the current #IdeGiVersion.
 * If the @IdeGiIndex matching the selected runtime is not ready yet,
 * (mostly because of creation and first update is not finished yet)
 * then %NULL is returnned.
 *
 * Returns: (transfer full) (nullable): A #IdeGiVersion.
 */
IdeGiVersion *
ide_gi_repository_get_current_version (IdeGiRepository *self)
{
  IdeGiVersion *version = NULL;

  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);

  /* TODO: lock early to keep the indexer alive */
  if (self->current_indexer != NULL)
    version = ide_gi_index_get_current_version (self->current_indexer);

  return version;
}

/**
 * ide_gi_repository_get_builddir:
 * @self: a #IdeGiRepository
 *
 * Return the build dir used by the repository.
 *
 * Returns: (transfer full) (nullable): A #GFile.
 */
GFile *
_ide_gi_repository_get_builddir (IdeGiRepository *self)
{
  g_assert (IDE_IS_GI_REPOSITORY (self));

  if (self->builddir != NULL)
    return g_object_ref (self->builddir);
  else
    return NULL;
}

/**
 * ide_gi_repository_add_gir_search_path:
 * @self: a #IdeGiRepository
 * @path: a path to add
 *
 * Add an additioonal path to search for .gir files.
 *
 * Returns: %FALSE if the path already exist in the search paths, %TRUE otherwise.
 */
gboolean
ide_gi_repository_add_gir_search_path (IdeGiRepository *self,
                                       const gchar     *path)
{
  GFile *file;

  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (path), FALSE);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);

  for (guint i = 0; i < self->gir_paths->len; i++)
    {
      GFile *existing_file = g_ptr_array_index (self->gir_paths, i);
      const gchar *file_path = g_file_get_path (existing_file);

      if (dzl_str_equal0 (path, file_path))
        {
          g_warning ("You can't add an already existing gir path:%s", path);
          return FALSE;
        }
    }

  file = g_file_new_for_path (path);
  g_ptr_array_add (self->gir_paths, file);

  return TRUE;
}

/**
 * ide_gi_repository_remove_gir_search_path:
 * @self: a #IdeGiRepository
 * @path: a path to remove
 *
 * Add an additioonal path to search for .gir files.
 *
 * Returns: %TRUE if the path was in the search paths, %FALSE otherwise.
 */
gboolean
ide_gi_repository_remove_gir_search_path (IdeGiRepository *self,
                                          const gchar     *path)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), FALSE);
  g_return_val_if_fail (!dzl_str_empty0 (path), FALSE);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);

  for (guint i = 0; i < self->gir_paths->len; i++)
    {
      GFile *existing_file = g_ptr_array_index (self->gir_paths, i);
      const gchar *file_path = g_file_get_path (existing_file);

      if (dzl_str_equal0 (path, file_path))
        {
          g_ptr_array_remove_index_fast (self->gir_paths, i);
          return TRUE;
        }
    }

  return  FALSE;
}

/**
 * ide_gi_repository_get_gir_search_paths:
 * @self: a #IdeGiRepository
 *
 * Get a #GPtrArray of all the paths added by using
 * ide_gi_repository_add_gir_search_path.
 *
 * Returns: (transfer full): A #GPtrArray of additional #GFile gir paths.
 */
GPtrArray  *
ide_gi_repository_get_gir_search_paths (IdeGiRepository *self)
{
  GPtrArray *ar;

  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  ar = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < self->gir_paths->len; i++)
    g_ptr_array_add (ar, g_object_ref (g_ptr_array_index (self->gir_paths, i)));

  return ar;
}

/**
 * ide_gi_repository_get_update_on_build:
 * @self: #IdeGiRepository
 *
 * Get the update-on-build state.
 *
 * Returns: %TRUE if set, %FALSE otherwise.
 */
gboolean
ide_gi_repository_get_update_on_build (IdeGiRepository *self)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), FALSE);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);

  return self->update_on_build;
}

/**
 * ide_gi_repository_set_update_on_build:
 * @self: #IdeGiRepository
 *
 * Set the update-on-build state.
 * It does not stop the in-fight updates and only trigger
 * an update by itself if the index has not been initialized yet.
 *
 * Set to %FALSE, it clear the scheduled update queue and prevent from
 * further on build/rebuild updates.
 *
 * Set to %TRUE, it allow new on build/rebuild updates to happen.
 *
 * Returns:
 */
void
ide_gi_repository_set_update_on_build (IdeGiRepository *self,
                                       gboolean         state)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (IDE_IS_GI_REPOSITORY (self));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  if (state == self->update_on_build)
    return;

  self->update_on_build = state;

  if (g_hash_table_size (self->indexer_table) == 0)
    {
      /* No index means:
       * - we use update_on_build == FALSE from the start,
       *   we need one, but be sure *_constructed is already done.
       *
       * - the index is in-construction.
       */
      if (!state || g_hash_table_size (self->pending_runtimes) != 0)
        return;

      if (self->is_constructed)
        configuration_changed_cb (self);
    }
  else
    {
      g_hash_table_iter_init (&iter, self->indexer_table);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          IdeGiIndex *index = IDE_GI_INDEX (value);
          _ide_gi_index_set_update_on_build (index, state);
        }
    }
}

void
ide_gi_repository_queue_update (IdeGiRepository *self,
                                GCancellable    *cancellable)
{
  g_return_if_fail (IDE_IS_GI_REPOSITORY (self));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  if (self->current_indexer)
    ide_gi_index_queue_update (self->current_indexer, cancellable);
}

void
ide_gi_repository_update_async (IdeGiRepository     *self,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_return_if_fail (IDE_IS_GI_REPOSITORY (self));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  if (self->current_indexer)
    ide_gi_index_update_async (self->current_indexer,
                               cancellable,
                               callback,
                               user_data);
}

gboolean
ide_gi_repository_update_finish (IdeGiRepository  *self,
                                 GAsyncResult     *result,
                                 GError          **error)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), FALSE);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);

  /* TODO: what if the current indexer has changed since ? */
  if (self->current_indexer)
    return ide_gi_index_update_finish (self->current_indexer, result, error);
  else
    return FALSE;
}

static void
ide_gi_repository_scan_builddir_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  g_autoptr(IdeGiRepository) self = (IdeGiRepository *)user_data;
  g_autoptr(GError) error = NULL;
  GPtrArray *files;

  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  if (NULL == (files = ide_gi_utils_get_files_from_directory_finish (result, &error)))
    g_warning ("%s", error->message);
  else
    {
      /* We unconditionally trigger an update, defering the
       * checking of new project gir files to the update system.
       */
      g_mutex_lock (&self->project_dirs_mutex);
      dzl_clear_pointer (&self->project_girs, g_ptr_array_unref);
      self->project_girs = files;
      g_mutex_unlock (&self->project_dirs_mutex);

      if (self->update_on_build && self->current_indexer != NULL)
        ide_gi_index_queue_update (self->current_indexer, NULL);
    }
}

static void
phase_finished_cb (IdeGiRepository *self,
                   IdeBuildPhase    phase)
{
  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (IDE_IS_MAIN_THREAD ());

  if (phase == IDE_BUILD_PHASE_CONFIGURE && IDE_IS_BUILD_PIPELINE (self->current_pipeline))
    {
      const gchar *path = ide_build_pipeline_get_builddir (self->current_pipeline);
      g_autoptr (GFile) file = g_file_new_for_path (path);

      g_set_object (&self->builddir, file);
    }
  else if (phase == IDE_BUILD_PHASE_BUILD && G_IS_FILE (self->builddir))
    {
      ide_gi_utils_get_files_from_directory_async (self->builddir,
                                                   ".gir",
                                                   TRUE,
                                                   NULL,
                                                   ide_gi_repository_scan_builddir_cb,
                                                   g_object_ref (self));
    }
}

static void
index_version_changed_cb (IdeGiRepository *self,
                          IdeGiVersion    *version,
                          IdeGiIndex      *index)
{
  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (IDE_IS_GI_VERSION (version));
  g_assert (IDE_IS_GI_INDEX (index));

  if (index == self->current_indexer)
    g_signal_emit (self, signals [CURRENT_VERSION_CHANGED], 0, version);
}

IdeGiIndex *
_ide_gi_repository_get_current_index (IdeGiRepository *self)
{
  g_return_val_if_fail (IDE_IS_GI_REPOSITORY (self), NULL);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), NULL);

  if (self->current_indexer != NULL)
    return g_object_ref (self->current_indexer);
  else
    return NULL;
}

static void
index_new_cb (GObject      *source_object,
              GAsyncResult *result,
              gpointer      user_data)
{
  g_autoptr(IdeGiRepository) self = (IdeGiRepository *)user_data;
  GAsyncInitable *initable = (GAsyncInitable *)source_object;
  IdeGiIndex *index;
  const gchar *runtime_id;
  g_autoptr(IdeGiVersion) version  = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (G_IS_ASYNC_INITABLE (initable));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  if ((index = ide_gi_index_new_finish (initable, result, &error)))
    {
      runtime_id = ide_gi_index_get_runtime_id (index);
      g_hash_table_insert (self->indexer_table, g_strdup (runtime_id), index);
      g_hash_table_remove (self->pending_runtimes, runtime_id);

      /* Maybe the update_on_build has already changed */
      _ide_gi_index_set_update_on_build (index, self->update_on_build);

      /* Maybe the selected runtime has already changed */
      if (dzl_str_equal0 (runtime_id, self->current_runtime_id))
        g_set_object (&self->current_indexer, index);

      /* At this point, the index has already created a version if update_on_build == TRUE */
      if ((version = ide_gi_index_get_current_version (index)))
        index_version_changed_cb (self, version, index);

      g_signal_connect_object (index,
                               "current-version-changed",
                               G_CALLBACK (index_version_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }
  else
    g_warning ("%s", error->message);
}

static void
configuration_changed_cb (IdeGiRepository *self)
{
  IdeContext *context;
  IdeBuildManager *build_manager;
  IdeConfigurationManager *conf_manager;
  IdeConfiguration *config;
  IdeGiIndex *indexer;
  const gchar *runtime_id;

  g_assert (IDE_IS_GI_REPOSITORY (self));
  g_assert (IDE_IS_MAIN_THREAD ());

  context = ide_object_get_context (IDE_OBJECT (self));
  build_manager = ide_context_get_build_manager (context);
  dzl_set_weak_pointer (&self->current_pipeline, ide_build_manager_get_pipeline (build_manager));

  /* TODO: use a dzl signal group */
  g_signal_connect_object (self->current_pipeline,
                           "phase-finished",
                           G_CALLBACK (phase_finished_cb),
                           self,
                           G_CONNECT_SWAPPED);

  conf_manager = ide_context_get_configuration_manager (context);
  config = ide_configuration_manager_get_current (conf_manager);
  runtime_id = ide_configuration_get_runtime_id (config);
  if (dzl_str_equal0 (runtime_id, self->current_runtime_id))
    return;

  g_free (self->current_runtime_id);
  self->current_runtime_id = g_strdup (runtime_id);

  if (NULL == (indexer = g_hash_table_lookup (self->indexer_table, runtime_id)))
    {
      if (!g_hash_table_contains (self->pending_runtimes, runtime_id))
        {
          g_autoptr(GFile) cache_dir = g_file_new_build_filename (self->cache_path, runtime_id, NULL);

          g_set_object (&self->current_indexer, NULL);
          g_hash_table_add (self->pending_runtimes, g_strdup (runtime_id));
          ide_gi_index_new_async (self,
                                  context,
                                  cache_dir,
                                  runtime_id,
                                  self->update_on_build,
                                  NULL,
                                  index_new_cb,
                                  g_object_ref (self));
        }
    }
  else
    g_set_object (&self->current_indexer, indexer);
}

/**
 * ide_gi_repository_new:
 * @context: a #IdeContext
 * @update_on_build: The initial update_on_build value.
 *
 * Returns: (transfer full): A #IdeGiRepository
 */
IdeGiRepository *
ide_gi_repository_new (IdeContext *context,
                       gboolean    update_on_build)
{
  g_return_val_if_fail (IDE_IS_CONTEXT (context), NULL);

  return g_object_new (IDE_TYPE_GI_REPOSITORY,
                       "context", context,
                       "update-oon-build", update_on_build,
                       NULL);
}

static void
ide_gi_repository_constructed (GObject *object)
{
  IdeGiRepository *self = IDE_GI_REPOSITORY (object);
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeConfigurationManager *config_manager = ide_context_get_configuration_manager (context);

  G_OBJECT_CLASS (ide_gi_repository_parent_class)->constructed (object);

  self->cache_path = ide_context_cache_filename (context, "gi", NULL);
  g_signal_connect_object (config_manager,
                           "invalidate",
                           G_CALLBACK (configuration_changed_cb),
                           self,
                           G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  self->is_constructed = TRUE;

  if (self->update_on_build)
    configuration_changed_cb (self);
}

static void
ide_gi_repository_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdeGiRepository *self = IDE_GI_REPOSITORY (object);

  switch (prop_id)
    {
    case PROP_UPDATE_ON_BUILD:
      g_value_set_boolean (value, self->update_on_build);
      break;

    case PROP_RUNTIME_ID:
      g_value_set_string (value, self->current_runtime_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_repository_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdeGiRepository *self = IDE_GI_REPOSITORY (object);

  switch (prop_id)
    {
    case PROP_UPDATE_ON_BUILD:
      ide_gi_repository_set_update_on_build (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_repository_finalize (GObject *object)
{
  IdeGiRepository *self = (IdeGiRepository *)object;

  g_clear_object (&self->builddir);
  dzl_clear_pointer (&self->cache_path, g_free);
  dzl_clear_pointer (&self->current_runtime_id, g_free);

  dzl_clear_pointer (&self->indexer_table, g_hash_table_unref);
  dzl_clear_pointer (&self->project_girs, g_ptr_array_unref);
  dzl_clear_pointer (&self->gir_paths, g_ptr_array_unref);
  g_clear_object (&self->current_indexer);
  g_mutex_clear (&self->project_dirs_mutex);

  if (self->current_pipeline != NULL)
    dzl_clear_weak_pointer (&self->current_pipeline);

  G_OBJECT_CLASS (ide_gi_repository_parent_class)->finalize (object);
}

static void
ide_gi_repository_class_init (IdeGiRepositoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_repository_finalize;
  object_class->get_property = ide_gi_repository_get_property;
  object_class->set_property = ide_gi_repository_set_property;
  object_class->constructed = ide_gi_repository_constructed;

properties [PROP_UPDATE_ON_BUILD] =
    g_param_spec_boolean ("update-on-build",
                          "update-on-build",
                          "set the update-on-build feature",
                          TRUE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME_ID] =
    g_param_spec_string ("current-runtime-id",
                         "Current runtime id",
                         "The current runtime identifier.",
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CURRENT_VERSION_CHANGED] = g_signal_new ("current-version-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL, NULL, NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    IDE_TYPE_GI_VERSION);
}

static void
ide_gi_repository_init (IdeGiRepository *self)
{
  self->indexer_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->pending_runtimes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->gir_paths = g_ptr_array_new_with_free_func (g_object_unref);

  g_mutex_init (&self->project_dirs_mutex);
}
