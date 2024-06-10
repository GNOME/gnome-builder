/* gbp-code-index-plan.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "gbp-code-index-plan"

#include "config.h"

#include <gtksourceview/gtksource.h>
#include <libpeas.h>

#include <libide-core.h>
#include <libide-foundry.h>
#include <libide-search.h>
#include <libide-vcs.h>

#include "gbp-code-index-plan.h"
#include "ide-code-index-index.h"
#include "indexer-info.h"

struct _GbpCodeIndexPlan
{
  GObject     parent_instance;

  GMutex      mutex;
  GHashTable *directories;
};

typedef struct
{
  GFile              *directory;
  GPtrArray          *plan_items;
  GbpCodeIndexReason  reason;
} DirectoryInfo;

typedef struct
{
  GPtrArray      *indexers;
  GFile          *workdir;
  IdeVcs         *vcs;
  IdeBuildSystem *build_system;
} PopulateData;

typedef struct
{
  GFile *cachedir;
  GFile *workdir;
} CullIndexed;

G_DEFINE_FINAL_TYPE (GbpCodeIndexPlan, gbp_code_index_plan, G_TYPE_OBJECT)

static void
directory_info_free (DirectoryInfo *info)
{
  g_clear_object (&info->directory);
  g_clear_pointer (&info->plan_items, g_ptr_array_unref);
  g_slice_free (DirectoryInfo, info);
}

static void
populate_data_free (PopulateData *data)
{
  g_clear_object (&data->workdir);
  g_clear_object (&data->vcs);
  g_clear_object (&data->build_system);
  g_clear_pointer (&data->indexers, g_ptr_array_unref);
  g_slice_free (PopulateData, data);
}

static void
cull_indexed_free (CullIndexed *cull)
{
  g_clear_object (&cull->cachedir);
  g_clear_object (&cull->workdir);
  g_slice_free (CullIndexed, cull);
}

static void
gbp_code_index_plan_finalize (GObject *object)
{
  GbpCodeIndexPlan *self = (GbpCodeIndexPlan *)object;

  g_clear_pointer (&self->directories, g_hash_table_unref);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gbp_code_index_plan_parent_class)->finalize (object);
}

static void
gbp_code_index_plan_class_init (GbpCodeIndexPlanClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_code_index_plan_finalize;
}

static void
gbp_code_index_plan_init (GbpCodeIndexPlan *self)
{
  g_mutex_init (&self->mutex);
  self->directories = g_hash_table_new_full (g_file_hash,
                                             (GEqualFunc)g_file_equal,
                                             NULL,
                                             (GDestroyNotify)directory_info_free);
}

GbpCodeIndexPlan *
gbp_code_index_plan_new (void)
{
  return g_object_new (GBP_TYPE_CODE_INDEX_PLAN, NULL);
}

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
gbp_code_index_plan_cull_indexed_worker (IdeTask      *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  GbpCodeIndexPlan *self = source_object;
  CullIndexed *cull = task_data;
  GHashTableIter iter;
  gpointer key, value;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_CODE_INDEX_PLAN (self));
  g_assert (G_IS_FILE (cull->cachedir));
  g_assert (G_IS_FILE (cull->workdir));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  g_mutex_lock (&self->mutex);

  g_hash_table_iter_init (&iter, self->directories);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_autofree gchar *relative = NULL;
      g_autoptr(GFile) indexdir = NULL;
      g_autoptr(GFile) symbol_keys = NULL;
      g_autoptr(GFile) symbol_names = NULL;
      g_autoptr(IdeFuzzyIndex) fuzzy = NULL;
      DirectoryInfo *info = value;
      GFile *directory = key;
      gboolean expired = FALSE;
      guint64 mtime;

      if (ide_task_return_error_if_cancelled (task))
        break;

      relative = g_file_get_relative_path (cull->workdir, directory);

      if (relative == NULL)
        indexdir = g_object_ref (cull->cachedir);
      else
        indexdir = g_file_get_child (cull->cachedir, relative);

      symbol_keys = g_file_get_child (indexdir, "SymbolKeys");
      symbol_names = g_file_get_child (indexdir, "SymbolNames");
      mtime = newest_mtime (symbol_keys, symbol_names, cancellable);

      /* Indexes don't yet exist, create them unless no files are available */
      if (!g_file_query_exists (symbol_keys, cancellable) ||
          !g_file_query_exists (symbol_names, cancellable))
        {
          if (ide_task_return_error_if_cancelled (task))
            break;

          if (info->plan_items->len == 0)
            {
              /* Nothing really to index, and no symol files, drop request */
              g_hash_table_iter_remove (&iter);
              continue;
            }
          else
            {
              info->reason = GBP_CODE_INDEX_REASON_INITIAL;
              continue;
            }
        }

      /* Indexes exist, but files no longer do. So remove them */
      if (info->plan_items->len == 0)
        {
          info->reason = GBP_CODE_INDEX_REASON_REMOVE_INDEX;
          continue;
        }

      for (guint i = 0; i < info->plan_items->len; i++)
        {
          GbpCodeIndexPlanItem *item = g_ptr_array_index (info->plan_items, i);
          guint64 item_mtime = g_file_info_get_attribute_uint64 (item->file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

          if (item_mtime > mtime)
            {
              expired = TRUE;
              break;
            }
        }

      if (expired)
        {
          info->reason = GBP_CODE_INDEX_REASON_EXPIRED;
          continue;
        }

      fuzzy = ide_fuzzy_index_new ();

      if (ide_fuzzy_index_load_file (fuzzy, symbol_names, cancellable, NULL))
        {
          guint32 n_files;

          if (ide_task_return_error_if_cancelled (task))
            break;

          n_files = ide_fuzzy_index_get_metadata_uint32 (fuzzy, "n_files");

          if (n_files != info->plan_items->len)
            {
              info->reason = GBP_CODE_INDEX_REASON_CHANGES;
              continue;
            }
        }
      else
        {
          /* Index is bad, we need to recreate it */
          info->reason = GBP_CODE_INDEX_REASON_INITIAL;
          continue;
        }

      /* Everything looks fine with this entry, nothing to update */
      g_hash_table_iter_remove (&iter);
    }

  g_mutex_unlock (&self->mutex);

  if (!ide_task_had_error (task))
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
gbp_code_index_plan_cull_indexed_async (GbpCodeIndexPlan    *self,
                                        IdeContext          *context,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  CullIndexed *state;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_CODE_INDEX_PLAN (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  state = g_slice_new0 (CullIndexed);
  state->cachedir = ide_context_cache_file (context, "code-index", NULL);
  state->workdir = ide_context_ref_workdir (context);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_plan_cull_indexed_async);
  ide_task_set_task_data (task, state, cull_indexed_free);
  ide_task_run_in_thread (task, gbp_code_index_plan_cull_indexed_worker);

  IDE_EXIT;
}

gboolean
gbp_code_index_plan_cull_indexed_finish (GbpCodeIndexPlan  *self,
                                         GAsyncResult      *result,
                                         GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_PLAN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

void
gbp_code_index_plan_foreach (GbpCodeIndexPlan        *self,
                             GbpCodeIndexPlanForeach  foreach_func,
                             gpointer                 foreach_data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (GBP_IS_CODE_INDEX_PLAN (self));
  g_return_if_fail (foreach_func != NULL);

  g_mutex_lock (&self->mutex);

  g_hash_table_iter_init (&iter, self->directories);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GFile *directory = key;
      DirectoryInfo *info = value;

      if (foreach_func (directory, info->plan_items, info->reason, foreach_data))
        g_hash_table_iter_remove (&iter);
    }

  g_mutex_unlock (&self->mutex);
}

static void
gbp_code_index_plan_populate_cb (GFile     *directory,
                                 GPtrArray *file_infos,
                                 gpointer   user_data)
{
  g_autoptr(GPtrArray) items = NULL;
  IdeTask *task = user_data;
  GbpCodeIndexPlan *self;
  DirectoryInfo *info;
  PopulateData *state;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_FILE (directory));
  g_assert (file_infos != NULL);

  self = ide_task_get_source_object (task);
  state = ide_task_get_task_data (task);

  g_assert (GBP_IS_CODE_INDEX_PLAN (self));
  g_assert (state != NULL);
  g_assert (G_IS_FILE (state->workdir));
  g_assert (state->indexers != NULL);

  items = g_ptr_array_new_with_free_func ((GDestroyNotify)gbp_code_index_plan_item_unref);

  for (guint i = 0; i < file_infos->len; i++)
    {
      GFileInfo *file_info = g_ptr_array_index (file_infos, i);
      g_autoptr(GFile) file = NULL;
      g_autofree gchar *reversed = NULL;
      const gchar *indexer_module_name = NULL;
      const gchar *mime_type;
      const gchar *name;
      GbpCodeIndexPlanItem *item;

      if (!(name = g_file_info_get_name (file_info)))
        continue;

      file = g_file_get_child (directory, name);

      if (ide_vcs_is_ignored (state->vcs, file, NULL))
        continue;

      /* Ignore .in files since those may bet miss-reported */
      if (g_str_has_suffix (name, ".in"))
        continue;

      reversed = g_utf8_strreverse (name, -1);
      mime_type = g_file_info_get_attribute_string (file_info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

      for (guint j = 0; j < state->indexers->len; j++)
        {
          const IndexerInfo *indexer = g_ptr_array_index (state->indexers, j);

          if (indexer_info_matches (indexer, name, reversed, mime_type))
            {
              indexer_module_name = indexer->module_name;
              break;
            }
        }

      if (indexer_module_name == NULL)
        continue;

      item = g_atomic_rc_box_new0 (GbpCodeIndexPlanItem);
      item->file_info = g_object_ref (file_info);
      item->build_flags = NULL;
      item->indexer_module_name = indexer_module_name;

      g_ptr_array_add (items, g_steal_pointer (&item));
    }

  info = g_slice_new0 (DirectoryInfo);
  info->directory = g_file_dup (directory);
  info->plan_items = g_steal_pointer (&items);
  info->reason = GBP_CODE_INDEX_REASON_INITIAL;

  g_mutex_lock (&self->mutex);
  g_hash_table_insert (self->directories, info->directory, info);
  g_mutex_unlock (&self->mutex);
}

static void
gbp_code_index_plan_populate_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  PopulateData *state = task_data;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (GBP_IS_CODE_INDEX_PLAN (source_object));
  g_assert (state != NULL);
  g_assert (IDE_IS_BUILD_SYSTEM (state->build_system));
  g_assert (IDE_IS_VCS (state->vcs));
  g_assert (G_IS_FILE (state->workdir));

  ide_g_file_walk_with_ignore (state->workdir,
                               G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE","
                               G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME","
                               G_FILE_ATTRIBUTE_STANDARD_NAME","
                               G_FILE_ATTRIBUTE_STANDARD_SIZE","
                               G_FILE_ATTRIBUTE_STANDARD_TYPE","
                               G_FILE_ATTRIBUTE_TIME_MODIFIED,
                               ".noindex",
                               cancellable,
                               gbp_code_index_plan_populate_cb,
                               task);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
gbp_code_index_plan_populate_async (GbpCodeIndexPlan    *self,
                                    IdeContext          *context,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) workdir = NULL;
  IdeBuildSystem *build_system = NULL;
  IdeVcs *vcs = NULL;
  PopulateData *state;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_PLAN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  workdir = ide_context_ref_workdir (context);
  build_system = ide_build_system_from_context (context);
  vcs = ide_vcs_from_context (context);

  g_return_if_fail (IDE_IS_BUILD_SYSTEM (build_system));
  g_return_if_fail (IDE_IS_VCS (vcs));

  state = g_slice_new0 (PopulateData);
  state->vcs = g_object_ref (vcs);
  state->build_system = g_object_ref (build_system);
  state->workdir = g_file_dup (workdir);
  state->indexers = collect_indexer_info ();

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_plan_populate_async);
  ide_task_set_task_data (task, state, populate_data_free);
  ide_task_run_in_thread (task, gbp_code_index_plan_populate_worker);

  IDE_EXIT;
}

gboolean
gbp_code_index_plan_populate_finish (GbpCodeIndexPlan  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_PLAN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

static gboolean
gbp_code_index_plan_collect_files (GFile              *directory,
                                   GPtrArray          *plan_items,
                                   GbpCodeIndexReason  reason,
                                   gpointer            user_data)
{
  GPtrArray *files = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (directory));
  g_assert (plan_items != NULL);
  g_assert (files != NULL);

  /* Skip if we don't care about these items */
  if (reason == GBP_CODE_INDEX_REASON_REMOVE_INDEX)
    return FALSE;

  for (guint i = 0; i < plan_items->len; i++)
    {
      const GbpCodeIndexPlanItem *item = g_ptr_array_index (plan_items, i);
      const gchar *name = g_file_info_get_name (item->file_info);

      g_ptr_array_add (files, g_file_get_child (directory, name));
    }

  return FALSE;
}

static gboolean
gbp_code_index_plan_fill_build_flags_cb (GFile              *directory,
                                         GPtrArray          *plan_items,
                                         GbpCodeIndexReason  reason,
                                         gpointer            user_data)
{
  GHashTable *build_flags = user_data;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (G_IS_FILE (directory));
  g_assert (plan_items != NULL);
  g_assert (build_flags != NULL);

  for (guint i = 0; i < plan_items->len; i++)
    {
      GbpCodeIndexPlanItem *item = g_ptr_array_index (plan_items, i);
      const gchar *name = g_file_info_get_name (item->file_info);
      g_autoptr(GFile) file = g_file_get_child (directory, name);
      const char * const *item_flags;

      if ((item_flags = g_hash_table_lookup (build_flags, file)))
        ide_set_strv (&item->build_flags, item_flags);
    }

  return FALSE;
}

static void
gbp_code_index_plan_get_build_flags_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeBuildSystem *build_system = (IdeBuildSystem *)object;
  g_autoptr(GHashTable) build_flags = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpCodeIndexPlan *self;

  IDE_ENTRY;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_BUILD_SYSTEM (build_system));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!(build_flags = ide_build_system_get_build_flags_for_files_finish (build_system, result, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  self = ide_task_get_source_object (task);

  gbp_code_index_plan_foreach (self,
                               gbp_code_index_plan_fill_build_flags_cb,
                               build_flags);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

void
gbp_code_index_plan_load_flags_async (GbpCodeIndexPlan    *self,
                                      IdeContext          *context,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GPtrArray) files = NULL;
  IdeBuildSystem *build_system;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_MAIN_THREAD ());
  g_return_if_fail (GBP_IS_CODE_INDEX_PLAN (self));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_plan_load_flags_async);

  /* Get build system to query */
  build_system = ide_build_system_from_context (context);

  /* Create array of files for every file we know about */
  files = g_ptr_array_new_with_free_func (g_object_unref);
  gbp_code_index_plan_foreach (self,
                               gbp_code_index_plan_collect_files,
                               files);

  ide_build_system_get_build_flags_for_files_async (build_system,
                                                    files,
                                                    cancellable,
                                                    gbp_code_index_plan_get_build_flags_cb,
                                                    g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_code_index_plan_load_flags_finish (GbpCodeIndexPlan  *self,
                                       GAsyncResult      *result,
                                       GError           **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_PLAN (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  IDE_RETURN (ret);
}

GbpCodeIndexPlanItem *
gbp_code_index_plan_item_copy (const GbpCodeIndexPlanItem *item)
{
  GbpCodeIndexPlanItem *ret;

  if (item == NULL)
    return NULL;

  ret = g_atomic_rc_box_new0 (GbpCodeIndexPlanItem);
  ret->file_info = g_object_ref (item->file_info);
  ret->build_flags = g_strdupv (item->build_flags);
  ret->indexer_module_name = item->indexer_module_name;

  return g_steal_pointer (&ret);
}

static void
gbp_code_index_plan_item_finalize (gpointer data)
{
  GbpCodeIndexPlanItem *item = data;

  g_clear_object (&item->file_info);
  g_clear_pointer (&item->build_flags, g_strfreev);
  item->indexer_module_name = NULL;
}

void
gbp_code_index_plan_item_unref (GbpCodeIndexPlanItem *item)
{
  g_atomic_rc_box_release_full (item, gbp_code_index_plan_item_finalize);
}
