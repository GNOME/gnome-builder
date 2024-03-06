/* gbp-code-index-builder.c
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

#define G_LOG_DOMAIN "gbp-code-index-builder"

#include "config.h"

#include <libpeas.h>

#include <libide-code.h>
#include <libide-io.h>
#include <libide-foundry.h>
#include <libide-search.h>

#include "gbp-code-index-builder.h"
#include "gbp-code-index-plan.h"

struct _GbpCodeIndexBuilder
{
  IdeObject                parent_instance;
  GFile                   *source_dir;
  GFile                   *index_dir;
  GPtrArray               *items;
  IdePersistentMapBuilder *map;
  IdeFuzzyIndexBuilder    *fuzzy;
  guint                    next_file_id;
  guint                    has_run : 1;
};

typedef struct
{
  guint n_active;
  guint completed;
} Run;

G_DEFINE_FINAL_TYPE (GbpCodeIndexBuilder, gbp_code_index_builder, IDE_TYPE_OBJECT)

static void
run_free (Run *state)
{
  g_slice_free (Run, state);
}

static void
gbp_code_index_builder_finalize (GObject *object)
{
  GbpCodeIndexBuilder *self = (GbpCodeIndexBuilder *)object;

  g_clear_object (&self->source_dir);
  g_clear_object (&self->index_dir);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_object (&self->map);
  g_clear_object (&self->fuzzy);

  G_OBJECT_CLASS (gbp_code_index_builder_parent_class)->finalize (object);
}

static void
gbp_code_index_builder_class_init (GbpCodeIndexBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_code_index_builder_finalize;
}

static void
gbp_code_index_builder_init (GbpCodeIndexBuilder *self)
{
  self->items = g_ptr_array_new_with_free_func ((GDestroyNotify)gbp_code_index_plan_item_unref);
  self->map = ide_persistent_map_builder_new ();
  self->fuzzy = ide_fuzzy_index_builder_new ();
}

static void
gbp_code_index_builder_submit (GbpCodeIndexBuilder *self,
                               GFile               *file,
                               GPtrArray           *entries)
{
  g_autofree gchar *filename = NULL;
  gchar num[16];
  guint file_id;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));

  file_id = self->next_file_id;
  self->next_file_id++;

  /*
   * Storing file_name:id and id:file_name into index, file_name:id will be
   * used to check whether a file is there in index or not.
   *
   * This can get called multiple times, but it's fine because we're just
   * updating a GVariantDict until the file has been processed.
   */
  g_snprintf (num, sizeof (num), "%u", file_id);
  filename = g_file_get_path (file);
  ide_fuzzy_index_builder_set_metadata_uint32 (self->fuzzy, filename, file_id);
  ide_fuzzy_index_builder_set_metadata_string (self->fuzzy, num, filename);

  if (entries == NULL)
    return;

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
        ide_persistent_map_builder_insert (self->map,
                                           key,
                                           g_variant_new ("(uuuu)",
                                                          file_id,
                                                          begin_line,
                                                          begin_line_offset,
                                                          flags),
                                           !!(flags & IDE_SYMBOL_FLAGS_IS_DEFINITION));

      if (name != NULL)
        ide_fuzzy_index_builder_insert (self->fuzzy,
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

GbpCodeIndexBuilder *
gbp_code_index_builder_new (GFile *source_dir,
                            GFile *index_dir)
{
  GbpCodeIndexBuilder *self;

  g_return_val_if_fail (G_IS_FILE (source_dir), NULL);
  g_return_val_if_fail (G_IS_FILE (index_dir), NULL);

  self = g_object_new (GBP_TYPE_CODE_INDEX_BUILDER, NULL);
  self->source_dir = g_object_ref (source_dir);
  self->index_dir = g_object_ref (index_dir);

  return g_steal_pointer (&self);
}

void
gbp_code_index_builder_add_item (GbpCodeIndexBuilder        *self,
                                 const GbpCodeIndexPlanItem *item)
{
  g_return_if_fail (GBP_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (self->has_run == FALSE);
  g_return_if_fail (item != NULL);

  g_ptr_array_add (self->items, gbp_code_index_plan_item_copy (item));
}

static void
code_index_entries_collect_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  IdeCodeIndexEntries *entries = (IdeCodeIndexEntries *)object;
  GbpCodeIndexBuilder *self;
  g_autoptr(GPtrArray) items = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEX_ENTRIES (entries));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));

  if (!(items = ide_code_index_entries_collect_finish (entries, result, &error)))
    items = g_ptr_array_new_full (0, g_object_unref);

  gbp_code_index_builder_submit (self, file, items);
  ide_task_return_boolean (task, TRUE);
}

static void
code_indexer_index_file_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  IdeCodeIndexer *indexer = (IdeCodeIndexer *)object;
  g_autoptr(IdeCodeIndexEntries) entries = NULL;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  GbpCodeIndexBuilder *self;
  GFile *file;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_CODE_INDEXER (indexer));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  self = ide_task_get_source_object (task);
  file = ide_task_get_task_data (task);

  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));

  if (!(entries = ide_code_indexer_index_file_finish (indexer, result, &error)))
    {
      gbp_code_index_builder_submit (self, file, NULL);
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_code_index_entries_collect_async (entries,
                                        ide_task_get_cancellable (task),
                                        code_index_entries_collect_cb,
                                        g_object_ref (task));
}

static void
gbp_code_index_builder_index_file_async (GbpCodeIndexBuilder *self,
                                         GFile               *file,
                                         IdeCodeIndexer      *indexer,
                                         const gchar * const *build_flags,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (IDE_IS_CODE_INDEXER (indexer));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_builder_index_file_async);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);

#ifdef IDE_ENABLE_TRACE
  {
    g_autofree gchar *joined = build_flags ? g_strjoinv (" ", (gchar **)build_flags) : NULL;
    IDE_TRACE_MSG ("Indexing %s with flags: %s", g_file_peek_path (file), joined ?: "");
  }
#endif

  ide_code_indexer_index_file_async (indexer,
                                     file,
                                     build_flags,
                                     cancellable,
                                     code_indexer_index_file_cb,
                                     g_steal_pointer (&task));
}

static gboolean
gbp_code_index_builder_index_file_finish (GbpCodeIndexBuilder  *self,
                                          GAsyncResult         *result,
                                          GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_code_index_builder_index_file_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
  GbpCodeIndexBuilder *self = (GbpCodeIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  Run *state;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  gbp_code_index_builder_index_file_finish (self, result, &error);

  state = ide_task_get_task_data (task);
  state->n_active--;
  state->completed++;

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_code_index_builder_aggregate_async (GbpCodeIndexBuilder *self,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GHashTable) indexers = NULL;
  PeasEngine *engine;
  Run *state;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_builder_aggregate_async);

  if (self->items->len == 0)
    {
      ide_task_return_boolean (task, TRUE);
      IDE_EXIT;
    }

  state = g_slice_new0 (Run);
  state->n_active = 1;
  ide_task_set_task_data (task, state, run_free);

  engine = peas_engine_get_default ();
  indexers = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  /* We just queue up all of our indexer work up-front, and let the various
   * backends manage their own queue-depth. That way, we don't waste time
   * coordinating back and forth for async-but-serial processing.
   */

  for (guint i = 0; i < self->items->len; i++)
    {
      const GbpCodeIndexPlanItem *item = g_ptr_array_index (self->items, i);
      const gchar *name = g_file_info_get_name (item->file_info);
      g_autoptr(GFile) child = NULL;
      IdeCodeIndexer *indexer;

      if (name == NULL)
        continue;

      if (!(indexer = g_hash_table_lookup (indexers, item->indexer_module_name)))
        {
          PeasPluginInfo *plugin_info;

          if (!(plugin_info = peas_engine_get_plugin_info (engine, item->indexer_module_name)))
            continue;

          indexer = (IdeCodeIndexer *)
            peas_engine_create_extension (engine, plugin_info, IDE_TYPE_CODE_INDEXER,
                                          "parent", self,
                                          NULL);

          if (indexer == NULL)
            continue;

          g_hash_table_insert (indexers, (gchar *)item->indexer_module_name, indexer);
        }

      state->n_active++;

      child = g_file_get_child (self->source_dir, name);

      gbp_code_index_builder_index_file_async (self,
                                               child,
                                               indexer,
                                               (const gchar * const *)item->build_flags,
                                               cancellable,
                                               gbp_code_index_builder_index_file_cb,
                                               g_object_ref (task));
    }

  state->n_active--;

  g_ptr_array_remove_range (self->items, 0, self->items->len);

  if (state->n_active == 0)
    ide_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static gboolean
gbp_code_index_builder_aggregate_finish (GbpCodeIndexBuilder  *self,
                                         GAsyncResult         *result,
                                         GError              **error)
{
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_BUILDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_code_index_builder_persist_write_fuzzy_cb (GObject      *object,
                                               GAsyncResult *result,
                                               gpointer      user_data)
{
  IdeFuzzyIndexBuilder *fuzzy = (IdeFuzzyIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_FUZZY_INDEX_BUILDER (fuzzy));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_fuzzy_index_builder_write_finish (fuzzy, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_code_index_builder_persist_write_map_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data)
{
  IdePersistentMapBuilder *map = (IdePersistentMapBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GFile) file = NULL;
  GbpCodeIndexBuilder *self;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (map));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_persistent_map_builder_write_finish (map, result, &error))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self = ide_task_get_source_object (task);
  file = g_file_get_child (self->index_dir, "SymbolNames");

  ide_fuzzy_index_builder_set_metadata_uint32 (self->fuzzy, "n_files", self->next_file_id);

  IDE_TRACE_MSG ("Writing %s", g_file_peek_path (file));

  ide_fuzzy_index_builder_write_async (self->fuzzy,
                                       file,
                                       G_PRIORITY_DEFAULT,
                                       ide_task_get_cancellable (task),
                                       gbp_code_index_builder_persist_write_fuzzy_cb,
                                       g_object_ref (task));
}

static void
gbp_code_index_builder_persist_async (GbpCodeIndexBuilder *self,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GFile) file = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_builder_persist_async);

  file = g_file_get_child (self->index_dir, "SymbolKeys");

  g_file_make_directory_with_parents (self->index_dir, cancellable, NULL);

  IDE_TRACE_MSG ("Writing %s", g_file_peek_path (file));

  ide_persistent_map_builder_write_async (self->map,
                                          file,
                                          G_PRIORITY_DEFAULT,
                                          cancellable,
                                          gbp_code_index_builder_persist_write_map_cb,
                                          g_steal_pointer (&task));
}

static gboolean
gbp_code_index_builder_persist_finish (GbpCodeIndexBuilder  *self,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
gbp_code_index_builder_persist_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GbpCodeIndexBuilder *self = (GbpCodeIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_builder_persist_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_boolean (task, TRUE);
}

static void
gbp_code_index_builder_aggregate_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GbpCodeIndexBuilder *self = (GbpCodeIndexBuilder *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_MAIN_THREAD ());
  g_assert (GBP_IS_CODE_INDEX_BUILDER (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!gbp_code_index_builder_aggregate_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    gbp_code_index_builder_persist_async (self,
                                          ide_task_get_cancellable (task),
                                          gbp_code_index_builder_persist_cb,
                                          g_object_ref (task));
}

void
gbp_code_index_builder_run_async (GbpCodeIndexBuilder *self,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  IDE_ENTRY;

  g_return_if_fail (GBP_IS_CODE_INDEX_BUILDER (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->has_run == FALSE);

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, gbp_code_index_builder_run_async);

  self->has_run = TRUE;

  gbp_code_index_builder_aggregate_async (self,
                                          cancellable,
                                          gbp_code_index_builder_aggregate_cb,
                                          g_steal_pointer (&task));

  IDE_EXIT;
}

gboolean
gbp_code_index_builder_run_finish (GbpCodeIndexBuilder  *self,
                                   GAsyncResult         *result,
                                   GError              **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);
  g_return_val_if_fail (GBP_IS_CODE_INDEX_BUILDER (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  ret = ide_task_propagate_boolean (IDE_TASK (result), error);

  /* Drop extraneous resources immediately */
  g_clear_object (&self->source_dir);
  g_clear_object (&self->index_dir);
  g_clear_pointer (&self->items, g_ptr_array_unref);
  g_clear_object (&self->map);
  g_clear_object (&self->fuzzy);

  IDE_RETURN (ret);
}
