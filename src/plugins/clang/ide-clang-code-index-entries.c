/* ide-clang-code-index-entries.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright 2018-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-clang-code-index-entries"

#include "ide-clang-code-index-entries.h"

struct _IdeClangCodeIndexEntries
{
  GObject   parent;
  gchar    *path;
  GVariant *entries;
  guint     has_run : 1;
};

IdeCodeIndexEntries *
ide_clang_code_index_entries_new (const gchar *path,
                                  GVariant    *entries)
{
  IdeClangCodeIndexEntries *self;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (entries != NULL, NULL);

  self = g_object_new (IDE_TYPE_CLANG_CODE_INDEX_ENTRIES, NULL);
  self->path = g_strdup (path);
  self->entries = g_variant_ref_sink (entries);

  return IDE_CODE_INDEX_ENTRIES (self);
}

static GFile *
ide_clang_code_index_entries_get_file (IdeCodeIndexEntries *entries)
{
  g_return_val_if_fail (IDE_IS_CLANG_CODE_INDEX_ENTRIES (entries), NULL);

  return g_file_new_for_path (IDE_CLANG_CODE_INDEX_ENTRIES (entries)->path);
}

static void
ide_clang_code_index_entries_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  g_autoptr(GPtrArray) ret = NULL;
  g_autoptr(IdeCodeIndexEntryBuilder) builder = NULL;
  GVariant *entries = task_data;
  GVariantIter iter;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (source_object));
  g_assert (entries != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  ret = g_ptr_array_new_with_free_func ((GDestroyNotify)ide_code_index_entry_free);
  builder = ide_code_index_entry_builder_new ();

  if (g_variant_iter_init (&iter, entries))
    {
      GVariant *kv;

      while ((kv = g_variant_iter_next_value (&iter)))
        {
          g_autoptr(GVariant) unboxed = NULL;
          const gchar *name = NULL;
          const gchar *key = NULL;
          IdeSymbolKind kind = 0;
          IdeSymbolFlags flags = 0;
          GVariantDict dict;
          struct {
            guint32 line;
            guint32 column;
          } begin, end;

          if (g_variant_is_of_type (kv, G_VARIANT_TYPE_VARIANT))
            {
              unboxed = g_variant_get_variant (kv);
              g_variant_dict_init (&dict, unboxed);
            }
          else
            g_variant_dict_init (&dict, kv);

          g_variant_dict_lookup (&dict, "name", "&s", &name);
          g_variant_dict_lookup (&dict, "key", "&s", &key);
          g_variant_dict_lookup (&dict, "kind", "i", &kind);
          g_variant_dict_lookup (&dict, "flags", "i", &flags);
          g_variant_dict_lookup (&dict, "range", "(uuuu)",
                                 &begin.line, &begin.column,
                                 &end.line, &end.column);

          if (ide_str_empty0 (key))
            key = NULL;

          ide_code_index_entry_builder_set_name (builder, name);
          ide_code_index_entry_builder_set_key (builder, key);
          ide_code_index_entry_builder_set_flags (builder, flags);
          ide_code_index_entry_builder_set_kind (builder, kind);
          ide_code_index_entry_builder_set_range (builder,
                                                  begin.line, begin.column,
                                                  end.line, end.column);

          g_ptr_array_add (ret, ide_code_index_entry_builder_build (builder));

          g_variant_dict_clear (&dict);
          g_variant_unref (kv);
        }
    }

  ide_task_return_pointer (task,
                           g_steal_pointer (&ret),
                           g_ptr_array_unref);
}

static void
ide_clang_code_index_entries_next_entries_async (IdeCodeIndexEntries *entries,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)entries;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (self));
  g_assert (self->entries != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_clang_code_index_entries_next_entries_async);
  ide_task_set_priority (task, G_PRIORITY_LOW + 1000);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_task_set_task_data (task, g_variant_ref (self->entries), g_variant_unref);

  if (self->has_run)
    ide_task_return_pointer (task,
                             g_ptr_array_new_with_free_func ((GDestroyNotify)ide_code_index_entry_free),
                             g_ptr_array_unref);
  else
    ide_task_run_in_thread (task, ide_clang_code_index_entries_worker);

  self->has_run = TRUE;
}

static GPtrArray *
ide_clang_code_index_entries_next_entries_finish (IdeCodeIndexEntries  *entries,
                                                  GAsyncResult         *result,
                                                  GError              **error)
{
  GPtrArray *ret;

  g_assert (IDE_IS_CLANG_CODE_INDEX_ENTRIES (entries));
  g_assert (IDE_IS_TASK (result));
  g_assert (ide_task_is_valid (result, entries));

  ret = ide_task_propagate_pointer (IDE_TASK (result), error);

  return IDE_PTR_ARRAY_STEAL_FULL (&ret);
}

static void
index_entries_iface_init (IdeCodeIndexEntriesInterface *iface)
{
  /*
   * We only implement the Async API, not the sync API so that we can generate
   * the results inside of a thread.
   */

  iface->get_file = ide_clang_code_index_entries_get_file;
  iface->next_entries_async = ide_clang_code_index_entries_next_entries_async;
  iface->next_entries_finish = ide_clang_code_index_entries_next_entries_finish;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeClangCodeIndexEntries, ide_clang_code_index_entries, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_CODE_INDEX_ENTRIES, index_entries_iface_init))

static void
ide_clang_code_index_entries_finalize (GObject *object)
{
  IdeClangCodeIndexEntries *self = (IdeClangCodeIndexEntries *)object;

  g_clear_pointer (&self->entries, g_variant_unref);
  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS(ide_clang_code_index_entries_parent_class)->finalize (object);
}

static void
ide_clang_code_index_entries_class_init (IdeClangCodeIndexEntriesClass *klass)
{
  GObjectClass *object_class = (GObjectClass *)klass;

  object_class->finalize = ide_clang_code_index_entries_finalize;
}

static void
ide_clang_code_index_entries_init (IdeClangCodeIndexEntries *self)
{
}
