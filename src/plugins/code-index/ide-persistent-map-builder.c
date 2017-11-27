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

struct _IdePersistentMapBuilder
{
  GObject      parent;

  /* Array of keys. */
  GByteArray   *keys;
  /* Hash table of keys to remove duplicate keys. */
  GHashTable   *keys_hash;
  /* Array of values. */
  GPtrArray    *values;
  /*
   * Array of key value pairs. This is pair of offset of
   * key in keys array and index of value in values array.
   */
  GArray       *kvpairs;
  /* Dictionary for metadata. */
  GVariantDict *metadata;
};

typedef struct
{
  guint32 key;
  guint32 value;
} KVPair;

G_DEFINE_TYPE (IdePersistentMapBuilder, ide_persistent_map_builder, G_TYPE_OBJECT)

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

  local_value = g_variant_ref_sink (value);

  if ((value_index = GPOINTER_TO_UINT (g_hash_table_lookup (self->keys_hash, key))))
    {
      value_index--;
      if (replace)
        {
          g_clear_pointer (&g_ptr_array_index (self->values, value_index), g_variant_unref);
          g_ptr_array_index (self->values, value_index) = g_steal_pointer (&local_value);
        }
    }
  else
    {
      KVPair kvpair;

      kvpair.key = self->keys->len;
      kvpair.value = self->values->len;

      g_byte_array_append (self->keys, (const guchar *)key, strlen (key) + 1);
      g_ptr_array_add (self->values, g_steal_pointer (&local_value));
      g_array_append_val (self->kvpairs, kvpair);

      /*
       * Key in hashtable is the actual key in our map.
       * Value in hash table will point to element in values array
       * where actual value of key in our map is there.
       */
      g_hash_table_insert (self->keys_hash,
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

  g_variant_dict_insert (self->metadata,
                         key,
                         "x",
                         value);
}

gint
compare (KVPair       *a,
         KVPair       *b,
         gchar        *keys)
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
  GFile *destination = task_data;
  GVariantDict dict;
  g_autoptr(GVariant) data = NULL;
  GVariant *keys;
  GVariant *values;
  GVariant *kvpairs;
  GVariant *metadata;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_PERSISTENT_MAP_BUILDER (self));
  g_assert (G_IS_FILE (destination));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  if (!self->keys->len)
    g_task_return_boolean (task, TRUE);

  g_variant_dict_init (&dict, NULL);

  keys = g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE,
                                    self->keys->data,
                                    self->keys->len,
                                    sizeof (guint8));

  values = g_variant_new_array (NULL,
                                (GVariant * const *)(gpointer)self->values->pdata,
                                self->values->len);

  g_array_sort_with_data (self->kvpairs, (GCompareDataFunc)compare, self->keys->data);
  kvpairs = g_variant_new_fixed_array (G_VARIANT_TYPE ("(uu)"),
                                       self->kvpairs->data,
                                       self->kvpairs->len,
                                       sizeof (KVPair));

  metadata = g_variant_dict_end (self->metadata);

  /* Insert Keys. */
  g_variant_dict_insert_value (&dict, "keys", keys);
  /* Insert values. */
  g_variant_dict_insert_value (&dict, "values", values);
  /* Insert key value pairs. */
  g_variant_dict_insert_value (&dict, "kvpairs", kvpairs);
  /* Insert metadata. */
  g_variant_dict_insert_value (&dict, "metadata", metadata);
  /* Insert verion number. */
  g_variant_dict_insert (&dict,
                         "version",
                         "i",
                         2);
  /* Insert byte order*/
  g_variant_dict_insert (&dict,
                         "byte-order",
                         "i",
                         G_BYTE_ORDER);

  /* Write to file. */
  data = g_variant_ref_sink (g_variant_dict_end (&dict));

  if (g_file_replace_contents (destination,
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
ide_persistent_map_builder_write (IdePersistentMapBuilder *self,
                                  GFile                   *destination,
                                  gint                     io_priority,
                                  GCancellable            *cancellable,
                                  GError                 **error)
{
  g_autoptr(GTask) task = NULL;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP_BUILDER (self), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  task = g_task_new (self, cancellable, NULL, NULL);
  g_task_set_source_tag (task, ide_persistent_map_builder_write);
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, g_object_ref (destination), g_object_unref);

  if (self->values->len)
    ide_persistent_map_builder_write_worker (task, self, destination, cancellable);
  else
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_INVALID_DATA,
                             "No entries to write");

  return g_task_propagate_boolean (task, error);
}

void
ide_persistent_map_builder_write_async  (IdePersistentMapBuilder *self,
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

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, g_object_ref (destination), g_object_unref);
  g_task_set_source_tag (task, ide_persistent_map_builder_write_async);

  g_task_run_in_thread (task, ide_persistent_map_builder_write_worker);
}

/**
 * ide_persistent_map_builder_write_finish:
 * @self: An #IdePersistentMapBuilder instance.
 * @result: result of writing process
 * @error: error in writing process
 *
 * Returns: Whether file is written or not.
 */
gboolean
ide_persistent_map_builder_write_finish (IdePersistentMapBuilder  *self,
                                         GAsyncResult             *result,
                                         GError                  **error)
{
  GTask *task = (GTask *)result;

  g_assert (G_IS_TASK (task));

  return g_task_propagate_boolean (task, error);
}

static void
ide_persistent_map_builder_finalize (GObject *object)
{
  IdePersistentMapBuilder *self = (IdePersistentMapBuilder *)object;

  g_clear_pointer (&self->keys, g_byte_array_unref);
  g_clear_pointer (&self->keys_hash, g_hash_table_unref);
  g_clear_pointer (&self->values, g_ptr_array_unref);
  g_clear_pointer (&self->kvpairs, g_array_unref);
  g_clear_pointer (&self->metadata, g_variant_dict_unref);

  G_OBJECT_CLASS (ide_persistent_map_builder_parent_class)->finalize (object);
}

static void
ide_persistent_map_builder_init (IdePersistentMapBuilder *self)
{
  self->keys = g_byte_array_new ();
  self->keys_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->values = g_ptr_array_new ();
  self->kvpairs = g_array_new (FALSE, FALSE, sizeof (KVPair));
  self->metadata = g_variant_dict_new (NULL);
}

static void
ide_persistent_map_builder_class_init (IdePersistentMapBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_persistent_map_builder_finalize;
}

IdePersistentMapBuilder*
ide_persistent_map_builder_new (void)
{
  return g_object_new (IDE_TYPE_PERSISTENT_MAP_BUILDER, NULL);
}
