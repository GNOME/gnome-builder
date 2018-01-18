/* ide-persistent-map-builder.c
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

#define G_LOG_DOMAIN "ide-persistent-map-builder"

#include <ide.h>
#include <string.h>

#include "ide-persistent-map-builder.h"

typedef struct
{
  /* Array of keys. */
  GByteArray *keys;

  /* Hash table of keys to remove duplicate keys. */
  GHashTable *keys_hash;

  /* Array of values. */
  GPtrArray *values;
  /*
   * Array of key value pairs. This is pair of offset of
   * key in keys array and index of value in values array.
   */
  GArray *kvpairs;

  /* Dictionary for metadata. */
  GVariantDict *metadata;

  /* Where to write the file */
  GFile *destination;
} BuildState;

typedef struct
{
  guint32 key;
  guint32 value;
} KVPair;

struct _IdePersistentMapBuilder
{
  GObject parent;

  /*
   * The build state lets us keep all the contents together, and then
   * pass it to the worker thread so the main thread can no longer access
   * the existing state.
   */
  BuildState *state;
};


G_STATIC_ASSERT (sizeof (KVPair) == 8);

G_DEFINE_TYPE (IdePersistentMapBuilder, ide_persistent_map_builder, G_TYPE_OBJECT)

static void
build_state_free (gpointer data)
{
  BuildState *state = data;

  g_clear_pointer (&state->keys, g_byte_array_unref);
  g_clear_pointer (&state->keys_hash, g_hash_table_unref);
  g_clear_pointer (&state->values, g_ptr_array_unref);
  g_clear_pointer (&state->kvpairs, g_array_unref);
  g_clear_pointer (&state->metadata, g_variant_dict_unref);
  g_clear_object (&state->destination);
  g_slice_free (BuildState, state);
}

void
ide_persistent_map_builder_insert (IdePersistentMapBuilder *self,
                                   const gchar             *key,
                                   GVariant                *value,
                                   gboolean                 replace)
{
  g_autoptr(GVariant) local_value = NULL;
  guint32 value_index;

  g_return_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);
  g_return_if_fail (self->state != NULL);
  g_return_if_fail (self->state->keys_hash != NULL);
  g_return_if_fail (self->state->values != NULL);
  g_return_if_fail (self->state->kvpairs != NULL);

  local_value = g_variant_ref_sink (value);

  if (0 != (value_index = GPOINTER_TO_UINT (g_hash_table_lookup (self->state->keys_hash, key))))
    {
      if (replace)
        {
          g_variant_unref (g_ptr_array_index (self->state->values, value_index - 1));
          g_ptr_array_index (self->state->values, value_index - 1) = g_steal_pointer (&local_value);
        }
    }
  else
    {
      KVPair kvpair;

      kvpair.key = self->state->keys->len;
      kvpair.value = self->state->values->len;

      g_byte_array_append (self->state->keys, (const guchar *)key, strlen (key) + 1);
      g_ptr_array_add (self->state->values, g_steal_pointer (&local_value));
      g_array_append_val (self->state->kvpairs, kvpair);

      /*
       * Key in hashtable is the actual key in our map.
       * Value in hash table will point to element in values array
       * where actual value of key in our map is there.
       */
      g_hash_table_insert (self->state->keys_hash,
                           g_strdup (key),
                           GUINT_TO_POINTER (kvpair.value + 1));
    }
}

void
ide_persistent_map_builder_set_metadata_int64 (IdePersistentMapBuilder *self,
                                               const gchar             *key,
                                               gint64                   value)
{
  g_return_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self));
  g_return_if_fail (key != NULL);
  g_return_if_fail (self->state != NULL);
  g_return_if_fail (self->state->metadata != NULL);

  g_variant_dict_insert (self->state->metadata, key, "x", value);
}

static gint
compare_keys (KVPair      *a,
              KVPair      *b,
              const gchar *keys)
{
  return g_strcmp0 (keys + a->key, keys + b->key);
}

void
ide_persistent_map_builder_write_worker (GTask        *task,
                                         gpointer      source_object,
                                         gpointer      task_data,
                                         GCancellable *cancellable)
{
  IdePersistentMapBuilder *self = source_object;
  BuildState *state = task_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) data = NULL;
  GVariantDict dict;
  GVariant *keys;
  GVariant *values;
  GVariant *kvpairs;
  GVariant *metadata;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (self));
  g_assert (state != NULL);
  g_assert (state->keys != NULL);
  g_assert (state->keys_hash != NULL);
  g_assert (state->values != NULL);
  g_assert (state->kvpairs != NULL);
  g_assert (state->metadata != NULL);
  g_assert (G_IS_FILE (state->destination));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (g_task_return_error_if_cancelled (task))
    return;

  if (state->keys->len == 0)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "No entries to write");
      return;
    }

  g_variant_dict_init (&dict, NULL);

  keys = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                    state->keys->data,
                                    state->keys->len,
                                    sizeof (guint8));

  values = g_variant_new_array (NULL,
                                (GVariant * const *)(gpointer)state->values->pdata,
                                state->values->len);

  g_array_sort_with_data (state->kvpairs,
                          (GCompareDataFunc)compare_keys,
                          state->keys->data);

  kvpairs = g_variant_new_fixed_array (G_VARIANT_TYPE ("(uu)"),
                                       state->kvpairs->data,
                                       state->kvpairs->len,
                                       sizeof (KVPair));

  metadata = g_variant_dict_end (state->metadata);

  g_variant_dict_insert_value (&dict, "keys", keys);
  g_variant_dict_insert_value (&dict, "values", values);
  g_variant_dict_insert_value (&dict, "kvpairs", kvpairs);
  g_variant_dict_insert_value (&dict, "metadata", metadata);
  g_variant_dict_insert (&dict, "version", "i", 2);
  g_variant_dict_insert (&dict, "byte-order", "i", G_BYTE_ORDER);

  data = g_variant_take_ref (g_variant_dict_end (&dict));

  if (g_task_return_error_if_cancelled (task))
    return;

  if (g_file_replace_contents (state->destination,
                               g_variant_get_data (data),
                               g_variant_get_size (data),
                               NULL,
                               FALSE,
                               G_FILE_CREATE_NONE,
                               NULL,
                               cancellable,
                               &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

gboolean
ide_persistent_map_builder_write (IdePersistentMapBuilder  *self,
                                  GFile                    *destination,
                                  gint                      io_priority,
                                  GCancellable             *cancellable,
                                  GError                  **error)
{
  g_autoptr(GTask) task = NULL;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (self->state != NULL, FALSE);
  g_return_val_if_fail (self->state->destination == NULL, FALSE);

  self->state->destination = g_object_ref (destination);

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_source_tag (task, ide_persistent_map_builder_write);
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, g_steal_pointer (&self->state), build_state_free);
  g_task_run_in_thread_sync (task, ide_persistent_map_builder_write_worker);

  return g_task_propagate_boolean (task, error);
}

void
ide_persistent_map_builder_write_async (IdePersistentMapBuilder *self,
                                        GFile                   *destination,
                                        gint                     io_priority,
                                        GCancellable            *cancellable,
                                        GAsyncReadyCallback      callback,
                                        gpointer                 user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self));
  g_return_if_fail (G_IS_FILE (destination));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (self->state != NULL);
  g_return_if_fail (self->state->destination != NULL);

  self->state->destination = g_object_ref (destination);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, ide_persistent_map_builder_write_async);
  g_task_set_task_data (task, g_steal_pointer (&self->state), build_state_free);
  g_task_run_in_thread (task, ide_persistent_map_builder_write_worker);
}

/**
 * ide_persistent_map_builder_write_finish:
 * @self: an #IdePersistentMapBuilder
 * @result: a #GAsyncResult provided to callback
 * @error: location for a #GError, or %NULL
 *
 * Returns: %TRUE if the while was written successfully; otherwise %FALSE
 *   and @error is set.
 */
gboolean
ide_persistent_map_builder_write_finish (IdePersistentMapBuilder  *self,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_TASK (result), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
ide_persistent_map_builder_finalize (GObject *object)
{
  IdePersistentMapBuilder *self = (IdePersistentMapBuilder *)object;

  g_clear_pointer (&self->state, build_state_free);

  G_OBJECT_CLASS (ide_persistent_map_builder_parent_class)->finalize (object);
}

static void
ide_persistent_map_builder_init (IdePersistentMapBuilder *self)
{
  self->state = g_slice_new0 (BuildState);
  self->state->keys = g_byte_array_new ();
  self->state->keys_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->state->values = g_ptr_array_new_with_free_func ((GDestroyNotify)g_variant_unref);
  self->state->kvpairs = g_array_new (FALSE, FALSE, sizeof (KVPair));
  self->state->metadata = g_variant_dict_new (NULL);
}

static void
ide_persistent_map_builder_class_init (IdePersistentMapBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_persistent_map_builder_finalize;
}

IdePersistentMapBuilder *
ide_persistent_map_builder_new (void)
{
  return g_object_new (IDE_TYPE_PERSISTENT_MAP_BUILDER, NULL);
}
