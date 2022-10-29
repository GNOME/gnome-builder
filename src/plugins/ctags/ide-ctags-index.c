/* ide-ctags-index.c
 *
 * Copyright 2015-2019 Christian Hergert <christian@hergert.me>
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

#define G_LOG_DOMAIN "ide-ctags-index"

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "ide-ctags-index.h"

/* This object is meant to be immutable after loading so that
 * it can be used from threads safely.
 */

struct _IdeCtagsIndex
{
  IdeObject  parent_instance;

  GArray    *index;
  GBytes    *buffer;
  GFile     *file;
  gchar     *path_root;

  guint64    mtime;
};

enum {
  PROP_0,
  PROP_FILE,
  PROP_MTIME,
  PROP_PATH_ROOT,
  LAST_PROP
};

static void async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (IdeCtagsIndex, ide_ctags_index, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static GParamSpec *properties [LAST_PROP];

static gint
ide_ctags_index_entry_compare_keyword (gconstpointer a,
                                       gconstpointer b)
{
  const IdeCtagsIndexEntry *entrya = a;
  const IdeCtagsIndexEntry *entryb = b;

  return g_strcmp0 (entrya->name, entryb->name);
}

static gint
ide_ctags_index_entry_compare_prefix (gconstpointer a,
                                      gconstpointer b)
{
  const IdeCtagsIndexEntry *entrya = a;
  const IdeCtagsIndexEntry *entryb = b;

  /*
   * With bsearch(), the first element is always the key.
   */

  if (g_str_has_prefix (entryb->name, entrya->name))
    return 0;
  else
    return g_strcmp0 (entrya->name, entryb->name);
}

gint
ide_ctags_index_entry_compare (gconstpointer a,
                               gconstpointer b)
{
  const IdeCtagsIndexEntry *entrya = a;
  const IdeCtagsIndexEntry *entryb = b;
  gint ret;

  if (((ret = g_strcmp0 (entrya->name, entryb->name)) == 0) &&
      ((ret = (entrya->kind - entryb->kind)) == 0) &&
      ((ret = g_strcmp0 (entrya->pattern, entryb->pattern)) == 0) &&
      ((ret = g_strcmp0 (entrya->path, entryb->path)) == 0))
    return 0;

  return ret;
}

static inline gchar *
forward_to_tab (gchar *iter)
{
  while (*iter && g_utf8_get_char (iter) != '\t')
    iter = g_utf8_next_char (iter);
  return *iter ? iter : NULL;
}

static inline gchar *
forward_to_nontab_and_zero (gchar *iter)
{
  while (*iter && (g_utf8_get_char (iter) == '\t'))
    {
      gchar *tmp = iter;
      iter = g_utf8_next_char (iter);
      *tmp = '\0';
    }

  return *iter ? iter : NULL;
}

static gboolean
ide_ctags_index_parse_line (gchar              *line,
                            IdeCtagsIndexEntry *entry)
{
  gchar *iter = line;

  g_assert (line != NULL);
  g_assert (entry != NULL);

  memset (entry, 0, sizeof *entry);

  entry->name = iter;
  if (!(iter = forward_to_tab (iter)))
    return FALSE;
  if (!(iter = forward_to_nontab_and_zero (iter)))
    return FALSE;

  entry->path = iter;
  if (!(iter = forward_to_tab (iter)))
    return FALSE;
  if (!(iter = forward_to_nontab_and_zero (iter)))
    return FALSE;

  entry->pattern = iter;
  if (!(iter = forward_to_tab (iter)))
    return FALSE;
  if (!(iter = forward_to_nontab_and_zero (iter)))
    return FALSE;

  switch (*iter)
    {
    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
    case IDE_CTAGS_INDEX_ENTRY_IMPORT:
    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
    case IDE_CTAGS_INDEX_ENTRY_UNION:
    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      entry->kind = (IdeCtagsIndexEntryKind)*iter;
      break;

    default:
      break;
    }

  /* Store a pointer to the beginning of the key/val pairs */
  if (NULL != (iter = forward_to_tab (iter)))
    entry->keyval = iter;
  else
    entry->keyval = NULL;

  return TRUE;
}

static void
ide_ctags_index_build_index (IdeTask      *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  IdeCtagsIndex *self = source_object;
  g_autoptr(GError) error = NULL;
  IdeLineReader reader;
  GArray *index = NULL;
  gchar *contents = NULL;
  gchar *line;
  gsize length = 0;
  gsize line_length;

  IDE_ENTRY;

  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_CTAGS_INDEX (self));
  g_assert (G_IS_FILE (self->file));

  if (!g_file_load_contents (self->file, cancellable, &contents, &length, NULL, &error))
    IDE_GOTO (failure);

  if (length > G_MAXSSIZE)
    IDE_GOTO (failure);

  index = g_array_new (FALSE, FALSE, sizeof (IdeCtagsIndexEntry));

  ide_line_reader_init (&reader, contents, length);

  while ((line = ide_line_reader_next (&reader, &line_length)))
    {
      IdeCtagsIndexEntry entry;

      /* ignore header lines */
      if (line [0] == '!')
        continue;

      /*
       * Overwrite the \n with a \0 so we can treat this as a C string.
       */
      line [line_length] = '\0';

      /*
       * Now parse this line and add it to the index.
       * We'll sort things later as insertion sort would be a waste.
       * We could potentially avoid the sort later if we know the tags
       * file was sorted on creation.
       */
      if (ide_ctags_index_parse_line (line, &entry))
        g_array_append_val (index, entry);
    }

  g_array_sort (index, ide_ctags_index_entry_compare);

  self->index = index;
  self->buffer = g_bytes_new_take (contents, length);

  ide_task_return_boolean (task, TRUE);

  IDE_EXIT;

failure:
  g_clear_pointer (&contents, g_free);
  g_clear_pointer (&index, g_array_unref);

  if (error != NULL)
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to parse ctags file.");

  IDE_EXIT;
}

GFile *
ide_ctags_index_get_file (IdeCtagsIndex *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), NULL);

  return self->file;
}

static void
ide_ctags_index_set_file (IdeCtagsIndex *self,
                          GFile         *file)
{
  g_assert (IDE_IS_CTAGS_INDEX (self));
  g_assert (!file || G_IS_FILE (file));

  if (g_set_object (&self->file, file))
    g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_FILE]);
}

static void
ide_ctags_index_set_path_root (IdeCtagsIndex *self,
                               const gchar   *path_root)
{
  g_return_if_fail (IDE_IS_CTAGS_INDEX (self));

  if (g_set_str (&self->path_root, path_root))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PATH_ROOT]);
    }
}

static void
ide_ctags_index_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeCtagsIndex *self = IDE_CTAGS_INDEX (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, ide_ctags_index_get_file (self));
      break;

    case PROP_MTIME:
      g_value_set_uint64 (value, self->mtime);
      break;

    case PROP_PATH_ROOT:
      g_value_set_string (value, ide_ctags_index_get_path_root (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_ctags_index_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeCtagsIndex *self = IDE_CTAGS_INDEX (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ide_ctags_index_set_file (self, g_value_get_object (value));
      break;

    case PROP_MTIME:
      self->mtime = g_value_get_uint64 (value);
      break;

    case PROP_PATH_ROOT:
      ide_ctags_index_set_path_root (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_ctags_index_finalize (GObject *object)
{
  IdeCtagsIndex *self = (IdeCtagsIndex *)object;

  g_clear_object (&self->file);
  g_clear_pointer (&self->index, g_array_unref);
  g_clear_pointer (&self->buffer, g_bytes_unref);
  g_clear_pointer (&self->path_root, g_free);

  G_OBJECT_CLASS (ide_ctags_index_parent_class)->finalize (object);
}

static void
ide_ctags_index_class_init (IdeCtagsIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_ctags_index_finalize;
  object_class->get_property = ide_ctags_index_get_property;
  object_class->set_property = ide_ctags_index_set_property;

  properties [PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "The file containing the ctags data.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |G_PARAM_STATIC_STRINGS));

  properties [PROP_MTIME] =
    g_param_spec_uint64 ("mtime",
                         "Mtime",
                         "Mtime",
                         0,
                         G_MAXUINT64,
                         0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties [PROP_PATH_ROOT] =
    g_param_spec_string ("path-root",
                         "Path Root",
                         "The root path to use when resolving relative paths.",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
ide_ctags_index_init (IdeCtagsIndex *self)
{
}

static void
ide_ctags_index_init_async (GAsyncInitable      *initable,
                            gint                 priority,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  IdeCtagsIndex *self = (IdeCtagsIndex *)initable;
  g_autoptr(IdeTask) task = NULL;

  g_assert (IDE_IS_CTAGS_INDEX (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_priority (task, G_PRIORITY_LOW + 100);
  ide_task_set_source_tag (task, ide_ctags_index_init_async);
  ide_task_set_kind (task, IDE_TASK_KIND_INDEXER);

  if (self->file == NULL)
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "You must set IdeCtagsIndex:file before async initialization");
  else
    ide_task_run_in_thread (task, ide_ctags_index_build_index);
}

static gboolean
ide_ctags_index_init_finish (GAsyncInitable  *initable,
                             GAsyncResult    *result,
                             GError         **error)
{
  g_assert (IDE_IS_CTAGS_INDEX (initable));
  g_assert (IDE_IS_TASK (result));

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_ctags_index_init_async;
  iface->init_finish = ide_ctags_index_init_finish;
}

IdeCtagsIndex *
ide_ctags_index_new (GFile       *file,
                     const gchar *path_root,
                     guint64      mtime)
{
  g_autoptr(GFile) parent = NULL;
  g_autofree gchar *real_path_root = NULL;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (path_root == NULL)
    {
      if ((parent = g_file_get_parent (file)))
        path_root = real_path_root = g_file_get_path (parent);
    }

  return g_object_new (IDE_TYPE_CTAGS_INDEX,
                       "file", file,
                       "path-root", path_root,
                       "mtime", mtime,
                       NULL);
}

const gchar *
ide_ctags_index_get_path_root (IdeCtagsIndex *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), NULL);

  return self->path_root;
}

gsize
ide_ctags_index_get_size (IdeCtagsIndex *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), 0);

  if (self->index != NULL)
    return self->index->len;

  return 0;
}

static const IdeCtagsIndexEntry *
ide_ctags_index_lookup_full (IdeCtagsIndex *self,
                             const gchar   *keyword,
                             gsize         *length,
                             GCompareFunc   compare_func)
{
  IdeCtagsIndexEntry key = { 0 };
  IdeCtagsIndexEntry *ret;

  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), NULL);
  g_return_val_if_fail (keyword != NULL, NULL);

  if (length != NULL)
    *length = 0;

  if ((self->index == NULL) || (self->index->data == NULL) || (self->index->len == 0))
    return NULL;

  key.name = keyword;

  ret = bsearch (&key,
                 self->index->data,
                 self->index->len,
                 sizeof (IdeCtagsIndexEntry),
                 compare_func);

  if (ret != NULL)
    {
      IdeCtagsIndexEntry *last;
      IdeCtagsIndexEntry *first;
      gsize count = 0;
      gsize i;

      first = &g_array_index (self->index, IdeCtagsIndexEntry, 0);
      last = &g_array_index (self->index, IdeCtagsIndexEntry, self->index->len - 1);

      /*
       * We might be smack in the middle of a group of items that match this keyword.
       * So let's walk backwards to the first match, being careful not to access the
       * array out of bounds.
       */
      while ((ret > first) && (compare_func (&key, &ret [-1]) == 0))
        ret--;

      /*
       * Now count how many index entries match this.
       */
      for (i = 0; &ret[i] <= last; i++)
        {
          if (compare_func (&key, &ret [i]) == 0)
            count++;
          else
            break;
        }

      if (length != NULL)
        *length = count;
    }

  return ret;
}

gchar *
ide_ctags_index_resolve_path (IdeCtagsIndex *self,
                              const gchar   *relative_path)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), NULL);
  g_return_val_if_fail (relative_path != NULL, NULL);

  return g_build_filename (self->path_root, relative_path, NULL);
}

IdeCtagsIndexEntry *
ide_ctags_index_entry_copy (const IdeCtagsIndexEntry *entry)
{
  IdeCtagsIndexEntry *copy;

  copy = g_slice_new0 (IdeCtagsIndexEntry);
  copy->name = g_strdup (entry->name);
  copy->path = g_strdup (entry->path);
  copy->pattern = g_strdup (entry->pattern);
  copy->kind = entry->kind;

  return copy;
}

void
ide_ctags_index_entry_free (IdeCtagsIndexEntry *entry)
{
  g_free ((gchar *)entry->name);
  g_free ((gchar *)entry->path);
  g_free ((gchar *)entry->pattern);
  g_slice_free (IdeCtagsIndexEntry, entry);
}

const IdeCtagsIndexEntry *
ide_ctags_index_lookup (IdeCtagsIndex *self,
                        const gchar   *keyword,
                        gsize         *length)
{
  return ide_ctags_index_lookup_full (self, keyword, length,
                                      ide_ctags_index_entry_compare_keyword);
}

const IdeCtagsIndexEntry *
ide_ctags_index_lookup_prefix (IdeCtagsIndex *self,
                               const gchar   *keyword,
                               gsize         *length)
{
  return ide_ctags_index_lookup_full (self, keyword, length,
                                      ide_ctags_index_entry_compare_prefix);
}

guint64
ide_ctags_index_get_mtime (IdeCtagsIndex *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), 0);

  return self->mtime;
}

/**
 * ide_ctags_index_find_with_path:
 * @self: an #IdeCtagsIndex
 * @relative_path: A path relative to the indexes base_path.
 *
 * This will return a GPtrArray of #IdeCtagsIndex pointers. These
 * pointers are const and should not be modified or freed.
 *
 * The container is owned by the caller and should be freed by the
 * caller with g_ptr_array_unref().
 *
 * Note that this function is not indexed, and therefore is O(n)
 * running time with `n` is the number of items in the index.
 *
 * Returns: (transfer container) (element-type Ide.CtagsIndexEntry): An array
 *   of items matching the relative path.
 */
GPtrArray *
ide_ctags_index_find_with_path (IdeCtagsIndex *self,
                                const gchar   *relative_path)
{
  GPtrArray *ar;

  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), NULL);
  g_return_val_if_fail (relative_path != NULL, NULL);

  ar = g_ptr_array_new ();

  for (guint i = 0; i < self->index->len; i++)
    {
      IdeCtagsIndexEntry *entry = &g_array_index (self->index, IdeCtagsIndexEntry, i);

      if (g_str_equal (entry->path, relative_path))
        g_ptr_array_add (ar, entry);
    }

  return ar;
}

gboolean
ide_ctags_index_get_is_empty (IdeCtagsIndex *self)
{
  g_return_val_if_fail (IDE_IS_CTAGS_INDEX (self), FALSE);

  return self->index == NULL || self->index->len == 0;
}
