/* ide-persistent-map.c
 *
 * Copyright 2017 Anoop Chandu <anoopchandu96@gmail.com>
 * Copyright 2017-2019 Christian Hergert <chergert@redhat.com>
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

#define G_LOG_DOMAIN "ide-persistent-map"

#include "config.h"

#include <libide-threading.h>

#include "ide-persistent-map.h"

typedef struct
{
  guint32 key;
  guint32 value;
} KVPair;

struct _IdePersistentMap
{
  GObject            parent;

  GMappedFile       *mapped_file;

  GVariant          *data;

  GVariant          *keys_var;
  const gchar       *keys;

  GVariant          *values;

  GVariant          *kvpairs_var;
  const KVPair      *kvpairs;

  GVariantDict      *metadata;

  gsize              n_kvpairs;

  gint32             byte_order;

  guint              load_called : 1;
  guint              loaded : 1;
};

G_STATIC_ASSERT (sizeof (KVPair) == 8);

G_DEFINE_FINAL_TYPE (IdePersistentMap, ide_persistent_map, G_TYPE_OBJECT)

static void
ide_persistent_map_load_file_worker (IdeTask      *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdePersistentMap *self = source_object;
  GFile *file = task_data;
  g_autofree gchar *path = NULL;
  g_autoptr(GMappedFile) mapped_file = NULL;
  g_autoptr(GVariant) data = NULL;
  g_autoptr(GVariant) keys = NULL;
  g_autoptr(GVariant) values = NULL;
  g_autoptr(GVariant) metadata = NULL;
  g_autoptr(GVariant) kvpairs = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariantDict) dict = NULL;
  gint32 version;
  gsize n_elements;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_PERSISTENT_MAP (self));
  g_assert (G_IS_FILE (file));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->loaded == FALSE);

  self->loaded = TRUE;

  if (!g_file_is_native (file) || NULL == (path = g_file_get_path (file)))
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_FILENAME,
                                 "Index must be a local file");
      return;
    }

  mapped_file = g_mapped_file_new (path, FALSE, &error);

  if (mapped_file == NULL)
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  data = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT,
                                  g_mapped_file_get_contents (mapped_file),
                                  g_mapped_file_get_length (mapped_file),
                                  FALSE, NULL, NULL);

  if (data == NULL)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Failed to parse GVariant");
      return;
    }

  g_variant_take_ref (data);

  dict = g_variant_dict_new (data);

  if (!g_variant_dict_lookup (dict, "version", "i", &version) || version != 2)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Version mismatch in gvariant. Got %d, expected 1",
                                 version);
      return;
    }

  keys = g_variant_dict_lookup_value (dict, "keys", G_VARIANT_TYPE_ARRAY);
  values = g_variant_dict_lookup_value (dict, "values", G_VARIANT_TYPE_ARRAY);
  kvpairs = g_variant_dict_lookup_value (dict, "kvpairs", G_VARIANT_TYPE_ARRAY);
  metadata = g_variant_dict_lookup_value (dict, "metadata", G_VARIANT_TYPE_VARDICT);

  if (!g_variant_dict_lookup (dict, "byte-order", "i", &self->byte_order))
    self->byte_order = G_BYTE_ORDER;

  if (keys == NULL || values == NULL || kvpairs == NULL || metadata == NULL || !self->byte_order)
    {
      ide_task_return_new_error (task,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVAL,
                                 "Invalid GVariant index");
      return;
    }

  self->keys = g_variant_get_fixed_array (keys, &n_elements, sizeof (guint8));
  self->kvpairs = g_variant_get_fixed_array (kvpairs, &self->n_kvpairs, sizeof (KVPair));

  self->mapped_file = g_steal_pointer (&mapped_file);
  self->data = g_steal_pointer (&data);
  self->keys_var = g_steal_pointer (&keys);
  self->values = g_steal_pointer (&values);
  self->kvpairs_var = g_steal_pointer (&kvpairs);
  self->metadata = g_variant_dict_new (metadata);

  g_assert (!g_variant_is_floating (self->data));
  g_assert (!g_variant_is_floating (self->keys_var));
  g_assert (!g_variant_is_floating (self->values));
  g_assert (!g_variant_is_floating (self->kvpairs_var));
  g_assert (self->keys != NULL);
  g_assert (self->kvpairs != NULL);
  g_assert (self->metadata != NULL);

  ide_task_return_boolean (task, TRUE);
}

gboolean
ide_persistent_map_load_file (IdePersistentMap *self,
                              GFile            *file,
                              GCancellable     *cancellable,
                              GError          **error)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), FALSE);
  g_return_val_if_fail (self->load_called == FALSE, FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), FALSE);

  self->load_called = TRUE;

  task = ide_task_new (self, cancellable, NULL, NULL);
  ide_task_set_source_tag (task, ide_persistent_map_load_file);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_persistent_map_load_file_worker (task, self, file, cancellable);

  return ide_task_propagate_boolean (task, error);
}

void
ide_persistent_map_load_file_async (IdePersistentMap    *self,
                                    GFile               *file,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;

  g_return_if_fail (IDE_IS_PERSISTENT_MAP (self));
  g_return_if_fail (self->load_called == FALSE);
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  self->load_called = TRUE;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_persistent_map_load_file_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);
  ide_task_set_task_data (task, g_object_ref (file), g_object_unref);
  ide_task_run_in_thread (task, ide_persistent_map_load_file_worker);
}

/**
 * ide_persistent_map_load_file_finish:
 * @self: an #IdePersistentMap
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Returns: Whether file is loaded or not.
 */
gboolean
ide_persistent_map_load_file_finish (IdePersistentMap  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_persistent_map_lookup_value:
 * @self: An #IdePersistentMap instance.
 * @key: key to lookup value
 *
 * Returns: (transfer full) : value associalted with @key.
 */
GVariant *
ide_persistent_map_lookup_value (IdePersistentMap *self,
                                 const gchar      *key)
{
  g_autoptr(GVariant) value = NULL;
  gint64 l;
  gint64 r;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), NULL);
  g_return_val_if_fail (self->loaded, NULL);
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->kvpairs != NULL, NULL);
  g_return_val_if_fail (self->keys != NULL, NULL);
  g_return_val_if_fail (self->values != NULL, NULL);
  g_return_val_if_fail (self->n_kvpairs < G_MAXINT64, NULL);

  if (self->n_kvpairs == 0)
    return NULL;

  /* unsigned long to signed long */
  r = (gint64)self->n_kvpairs - 1;
  l = 0;

  while (l <= r)
    {
      gint64 m;
      gint32 k;
      gint cmp;

      m = (l + r) / 2;
      g_assert (m >= 0);

      k = self->kvpairs [m].key;
      g_assert (k >= 0);

      cmp = g_strcmp0 (key, &self->keys [k]);

      if (cmp < 0)
        r = m - 1;
      else if (cmp > 0)
        l = m + 1;
      else
        {
          value = g_variant_get_child_value (self->values, self->kvpairs [m].value);
          break;
        }
    }

  if (value != NULL && self->byte_order != G_BYTE_ORDER)
    return g_variant_byteswap (value);

  return g_steal_pointer (&value);
}

gint64
ide_persistent_map_builder_get_metadata_int64 (IdePersistentMap *self,
                                               const gchar      *key)
{
  guint64 value = 0;

  g_return_val_if_fail (IDE_IS_PERSISTENT_MAP (self), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (self->metadata != NULL, 0);

  if (!g_variant_dict_lookup (self->metadata, key, "x", &value))
    return 0;

  return value;
}

static void
ide_persistent_map_finalize (GObject *object)
{
  IdePersistentMap *self = (IdePersistentMap *)object;

  self->keys = NULL;
  self->kvpairs = NULL;

  g_clear_pointer (&self->data, g_variant_unref);
  g_clear_pointer (&self->keys_var, g_variant_unref);
  g_clear_pointer (&self->values, g_variant_unref);
  g_clear_pointer (&self->kvpairs_var, g_variant_unref);
  g_clear_pointer (&self->metadata, g_variant_dict_unref);
  g_clear_pointer (&self->mapped_file, g_mapped_file_unref);

  G_OBJECT_CLASS (ide_persistent_map_parent_class)->finalize (object);
}

static void
ide_persistent_map_init (IdePersistentMap *self)
{
}

static void
ide_persistent_map_class_init (IdePersistentMapClass *self)
{
  GObjectClass *object_class = G_OBJECT_CLASS (self);

  object_class->finalize = ide_persistent_map_finalize;
}

IdePersistentMap *
ide_persistent_map_new (void)
{
  return g_object_new (IDE_TYPE_PERSISTENT_MAP, NULL);
}
