/* ide-gi-utils.c
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

#define G_LOG_DOMAIN "ide-gi-utils"

#include <dazzle.h>

#include "ide-gi-utils.h"

#define EXTENSION_LEN 4

/* Keep in sync with corresponding enums in ide-gi-types.h */
const gchar * IDE_GI_SIGNAL_WHEN_NAMES [4] =
{
  "none",
  "first",
  "last",
  "cleanup",
};

const gchar * IDE_GI_TRANSFER_OWNERSHIP_NAMES [4] =
{
  "none",
  "container",
  "full",
  "floating",
};

const gchar * IDE_GI_DIRECTION_NAMES [3] =
{
  "in",
  "out",
  "in-out",
};

const gchar * IDE_GI_SCOPE_NAMES [3] =
{
  "call",
  "async",
  "notified",
};

const gchar * IDE_GI_STABILITY_NAMES [3] =
{
  "stable",
  "unstable",
  "private",
};

/* Keep in sync with corresponding enums in ide-gi-namespace.h */
static const gchar * IDE_GI_NS_TABLE_NAMES [] =
{
  "alias",     // IDE_GI_NS_TABLE_ALIAS
  "array",     // IDE_GI_NS_TABLE_ARRAY
  "callback",  // IDE_GI_NS_TABLE_CALLBACK
  "constant",  // IDE_GI_NS_TABLE_CONSTANT
  "doc",       // IDE_GI_NS_TABLE_DOC
  "enum",      // IDE_GI_NS_TABLE_ENUM
  "field",     // IDE_GI_NS_TABLE_FIELD
  "function",  // IDE_GI_NS_TABLE_FUNCTION
  "object",    // IDE_GI_NS_TABLE_OBJECT
  "parameter", // IDE_GI_NS_TABLE_PARAMETER
  "property",  // IDE_GI_NS_TABLE_PROPERTY
  "record",    // IDE_GI_NS_TABLE_RECORD
  "signal",    // IDE_GI_NS_TABLE_SIGNAL
  "type",      // IDE_GI_NS_TABLE_TYPE
  "union",     // IDE_GI_NS_TABLE_UNION
  "value",     // IDE_GI_NS_TABLE_VALUE
};

static const gchar * IDE_GI_BASIC_TYPE_NAMES [] =
{
  "none",        // IDE_GI_BASIC_TYPE_NONE
  "boolean",     // IDE_GI_BASIC_TYPE_GBOOLEAN
  "gchar",       // IDE_GI_BASIC_TYPE_GCHAR
  "guchar",      // IDE_GI_BASIC_TYPE_GUCHAR
  "gshort",      // IDE_GI_BASIC_TYPE_GSHORT
  "gushort",     // IDE_GI_BASIC_TYPE_GUSHORT
  "gint",        // IDE_GI_BASIC_TYPE_GINT
  "guint",       // IDE_GI_BASIC_TYPE_GUINT
  "glong",       // IDE_GI_BASIC_TYPE_GLONG
  "gulong",      // IDE_GI_BASIC_TYPE_GULONG
  "gssize",      // IDE_GI_BASIC_TYPE_GSSIZE
  "gsize",       // IDE_GI_BASIC_TYPE_GSIZE
  "gpointer",    // IDE_GI_BASIC_TYPE_GPOINTER
  "gintptr",     // IDE_GI_BASIC_TYPE_GINTPTR
  "guintptr",    // IDE_GI_BASIC_TYPE_GUINTPTR
  "gint8",       // IDE_GI_BASIC_TYPE_GINT8
  "guint8",      // IDE_GI_BASIC_TYPE_GUINT8
  "gint16",      // IDE_GI_BASIC_TYPE_GINT16
  "guint16",     // IDE_GI_BASIC_TYPE_GUINT16
  "gint32",      // IDE_GI_BASIC_TYPE_GINT32
  "guint32",     // IDE_GI_BASIC_TYPE_GUINT32
  "gint64",      // IDE_GI_BASIC_TYPE_GINT64
  "guint64",     // IDE_GI_BASIC_TYPE_GUINT64
  "gfloat",      // IDE_GI_BASIC_TYPE_GFLOAT
  "gdouble",     // IDE_GI_BASIC_TYPE_GDOUBLE
  "GType",       // IDE_GI_BASIC_TYPE_GTYPE
  "gutf8",       // IDE_GI_BASIC_TYPE_GUTF8
  "filename",    // IDE_GI_BASIC_TYPE_FILENAME
  "gunichar",    // IDE_GI_BASIC_TYPE_GUNICHAR
  "c array",     // IDE_GI_BASIC_TYPE_C_ARRAY
  "GArray",      // IDE_GI_BASIC_TYPE_G_ARRAY
  "GPtrArray",   // IDE_GI_BASIC_TYPE_G_PTR_ARRAY
  "GBytesArray", // IDE_GI_BASIC_TYPE_G_BYTES_ARRAY
  "varargs",     // IDE_GI_BASIC_TYPE_VARARGS
  "callback",    // IDE_GI_BASIC_TYPE_CALLBACK
};

static const gchar * IDE_GI_PREFIX_TYPE_NAMES [] =
{
  "namespace",  // IDE_GI_PREFIX_TYPE_NAMESPACE
  "symbol",     // IDE_GI_PREFIX_TYPE_SYMBOL
  "identifier", // IDE_GI_PREFIX_TYPE_IDENTIFIER
  "GType",      // IDE_GI_PREFIX_TYPE_GTYPE
  "package",    // IDE_GI_PREFIX_TYPE_PACKAGE
};

static const gchar * IDE_GI_BLOB_TYPE_NAMES [] =
{
  "Unknow",      // IDE_GI_BLOB_TYPE_UNKNOW
  "alias",       // IDE_GI_BLOB_TYPE_ALIAS
  "array",       // IDE_GI_BLOB_TYPE_ARRAY
  "boxed",       // IDE_GI_BLOB_TYPE_BOXED
  "callback",    // IDE_GI_BLOB_TYPE_CALLBACK
  "class",       // IDE_GI_BLOB_TYPE_CLASS
  "constant",    // IDE_GI_BLOB_TYPE_CONSTANT
  "constructor", // IDE_GI_BLOB_TYPE_CONSTRUCTOR
  "doc",         // IDE_GI_BLOB_TYPE_DOC
  "enum",        // IDE_GI_BLOB_TYPE_ENUM
  "field",       // IDE_GI_BLOB_TYPE_FIELD
  "function",    // IDE_GI_BLOB_TYPE_FUNCTION
  "header",      // IDE_GI_BLOB_TYPE_HEADER
  "interface",   // IDE_GI_BLOB_TYPE_INTERFACE
  "method",      // IDE_GI_BLOB_TYPE_METHOD
  "parameter",   // IDE_GI_BLOB_TYPE_PARAMETER
  "property",    // IDE_GI_BLOB_TYPE_PROPERTY
  "record",      // IDE_GI_BLOB_TYPE_RECORD
  "signal",      // IDE_GI_BLOB_TYPE_SIGNAL
  "type",        // IDE_GI_BLOB_TYPE_TYPE
  "union",       // IDE_GI_BLOB_TYPE_UNION
  "value",       // IDE_GI_BLOB_TYPE_VALUE
  "vfunc",       // IDE_GI_BLOB_TYPE_VFUNC
};

/* dot directories are filtered */
static gboolean
append_files_from_directory (GPtrArray   *files,
                             GFile       *directory,
                             const gchar *suffix,
                             gboolean     recursif)
{
  g_autoptr(GFileEnumerator) enumerator = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *attributes;
  gboolean with_suffix;

  g_assert (files != NULL);
  g_assert (G_IS_FILE (directory));

  attributes = G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE;
  if (NULL == (enumerator = g_file_enumerate_children (directory,
                                                       attributes,
                                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                       NULL,
                                                       &error)))
    {
      goto error;
    }

  with_suffix = !dzl_str_empty0 (suffix);
  while (TRUE)
    {
      GFileInfo *info;
      const gchar *name;
      GFile *file;
      GFileType type;

      if (!g_file_enumerator_iterate (enumerator, &info, &file, NULL, &error))
        goto error;

      if (info == NULL)
        break;

      type = g_file_info_get_file_type (info);
      name = g_file_info_get_name (info);
      if (type == G_FILE_TYPE_REGULAR)
        {
          if (!with_suffix || g_str_has_suffix (name, suffix))
            g_ptr_array_add (files, g_object_ref (file));
        }
      else if (recursif && type == G_FILE_TYPE_DIRECTORY && *name != '.')
        append_files_from_directory (files, file, suffix, TRUE);
    }

  return TRUE;

error:
  g_debug ("%s", error->message);
  return FALSE;
}

/**
 * ide_gi_utils_get_files_from_directories:
 * @directories: a #GPtrArray of #GFile directories.
 * @suffix: (nullable): a suffix to filter the filenames.
 * @recursif: %TRUE if recursif, %FALSE otherwise
 *
 * Returns: (transfer full) (element-type Gio.File): a #GPtrArray of #GFile (regular files only).
 *          The array is empty if there's no results.
 */
GPtrArray *
ide_gi_utils_get_files_from_directories (GPtrArray   *directories,
                                         const gchar *suffix,
                                         gboolean     recursif)
{
  GPtrArray *files;

  g_return_val_if_fail (directories != NULL, NULL);

  files = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < directories->len; i++)
    {
      GFile *directory = g_ptr_array_index (directories, i);

      append_files_from_directory (files, directory, suffix, recursif);
    }

  return files;
}

/**
 * ide_gi_utils_get_files_from_directory:
 * @directory: a #GFile
 * @suffix: (nullable): a suffix to filter the filenames.
 * @recursif: %TRUE if recursif, %FALSE otherwise
 *
 * Returns: (transfer full) (element-type Gio.File): a #GPtrArray of #GFile (regular files only).
 *          The array is empty if there's no results.
 */
GPtrArray *
ide_gi_utils_get_files_from_directory (GFile       *directory,
                                       const gchar *suffix,
                                       gboolean     recursif)
{
  GPtrArray *files;

  g_return_val_if_fail (G_IS_FILE (directory), NULL);

  files = g_ptr_array_new_with_free_func (g_object_unref);
  append_files_from_directory (files, directory, suffix, recursif);

  return files;
}

typedef struct
{
  gchar     *suffix;
  GPtrArray *directories;
  gboolean   recursif;
} FromDirectoriesState;

static void
from_directories_state_free (FromDirectoriesState *state)
{
  g_assert (state != NULL);

  dzl_clear_pointer (&state->suffix, g_free);
  dzl_clear_pointer (&state->directories, g_ptr_array_unref);
}

static void
get_files_from_directories_worker (IdeTask      *task,
                                   gpointer      source_object,
                                   gpointer      task_data,
                                   GCancellable *cancellable)
{
  FromDirectoriesState *state = (FromDirectoriesState *)task_data;
  GPtrArray *files;

  files = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < state->directories->len; i++)
    {
      GFile *file = g_ptr_array_index (state->directories, i);

      append_files_from_directory (files, file, state->suffix, state->recursif);
    }

  ide_task_return_pointer (task, files, (GDestroyNotify)g_ptr_array_unref);
}

void
ide_gi_utils_get_files_from_directories_async (GPtrArray           *directories,
                                               const gchar         *suffix,
                                               gboolean             recursif,
                                               GCancellable        *cancellable,
                                               GAsyncReadyCallback  callback,
                                               gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  FromDirectoriesState *state;

  g_assert (directories != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_utils_get_files_from_directories_async);

  state = g_slice_new0 (FromDirectoriesState);
  state->directories = g_ptr_array_ref (directories);
  state->suffix = g_strdup (suffix);
  state->recursif = recursif;

  ide_task_set_task_data (task, state, from_directories_state_free);
  ide_task_run_in_thread (task, get_files_from_directories_worker);
}

GPtrArray *
ide_gi_utils_get_files_from_directories_finish (GAsyncResult  *result,
                                                GError       **error)
{
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

typedef struct
{
  GFile        *directory;
  gchar        *suffix;
  gboolean      recursif;
} FromDirectoryState;

static void
from_directory_state_free (FromDirectoryState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->directory);
  dzl_clear_pointer (&state->suffix, g_free);
}

static void
get_files_from_directory_worker (IdeTask      *task,
                                 gpointer      source_object,
                                 gpointer      task_data,
                                 GCancellable *cancellable)
{
  FromDirectoryState *state = (FromDirectoryState *)task_data;
  GPtrArray *files;

  files = g_ptr_array_new_with_free_func (g_object_unref);
  append_files_from_directory (files, state->directory, state->suffix, state->recursif);

  ide_task_return_pointer (task, files, (GDestroyNotify)g_ptr_array_unref);
}

void
ide_gi_utils_get_files_from_directory_async (GFile               *directory,
                                             const gchar         *suffix,
                                             gboolean             recursif,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  FromDirectoryState *state;

  g_assert (G_IS_FILE (directory));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_utils_get_files_from_directory_async);

  state = g_slice_new0 (FromDirectoryState);
  state->directory = g_object_ref (directory);
  state->suffix = g_strdup (suffix);
  state->recursif = recursif;

  ide_task_set_task_data (task, state, from_directory_state_free);
  ide_task_run_in_thread (task, get_files_from_directory_worker);
}

/**
 * ide_gi_utils_get_files_from_directory_finish:
 * @result: a #GAsyncResult
 * @error: a #GError, or NULL.
 *
 * Finishes getting the files started in ide_gi_utils_get_files_from_directory_async.
 *
 * Returns: (transfer full) (element-type Gio.File): a #GPtrArray of #GFile (regular files only).
 *          The array is empty if there's no results.
 */
GPtrArray *
ide_gi_utils_get_files_from_directory_finish (GAsyncResult  *result,
                                              GError       **error)
{
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

/**
 * ide_gi_utils_files_list_dedup:
 *
 * De-duplicate the array entries.
 *
 * Returns: %TRUE if there's entries removed, %FALSE otheerwise.
 */
gboolean
ide_gi_utils_files_list_dedup (GPtrArray *files_list)
{
  g_autoptr(GHashTable) ht = NULL;
  guint i = 0;
  gboolean ret = FALSE;

  g_return_val_if_fail (files_list != NULL, FALSE);

  if (files_list->len > 0)
    {
      ht = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  NULL);

      while (i < files_list->len)
        {
          GFile *file = g_ptr_array_index (files_list, i);
          g_autofree gchar *basename = g_file_get_basename (file);

          if (g_hash_table_contains (ht, basename))
            {
              g_ptr_array_remove_index_fast (files_list, i);
              ret = TRUE;
            }
          else
            {
              g_hash_table_add (ht, g_steal_pointer (&basename));
              i++;
            }
        }
    }

  return ret;
}

/**
 * ide_gi_utils_files_list_difference:
 *
 * Returns: an array of #GFile in 'a' but not in 'b' so 'a-b' in math.
 * comparaison act only on the basename.
 */
GPtrArray *
ide_gi_utils_files_list_difference (GPtrArray *a,
                                    GPtrArray *b)
{
  g_autoptr(GHashTable) ht = NULL;
  GPtrArray *diff;
  gboolean add_all = FALSE;

  g_return_val_if_fail (a != NULL, NULL);
  g_return_val_if_fail (b != NULL, NULL);

  diff = g_ptr_array_new_with_free_func (g_object_unref);
  if (b->len > 0)
    {
      ht = g_hash_table_new_full (g_str_hash,
                                  g_str_equal,
                                  g_free,
                                  NULL);

      for (guint i = 0; i < b->len; i++)
        {
          GFile *fb = g_ptr_array_index (b, i);
          gchar *basename = g_file_get_basename (fb);
          gchar *pos;

          if ((pos = strchr (basename, '.')))
            *pos = '\0';

          g_hash_table_add (ht, basename);

        }
    }
  else
    add_all = TRUE;

  for (guint i = 0; i < a->len; i++)
    {
      GFile *fa = g_ptr_array_index (a, i);
      g_autofree gchar *basename = NULL;
      gchar *pos;

      if (add_all)
        {
          g_ptr_array_add (diff, g_object_ref (fa));
          continue;
        }

      basename = g_file_get_basename (fa);
      if ((pos = strchr (basename, '.')))
        *pos = '\0';

      if (!g_hash_table_contains (ht, basename))
        g_ptr_array_add (diff, g_object_ref (fa));
    }

  return diff;
}

void
ide_gi_utils_remove_files_list (GPtrArray *files_list)
{
  g_return_if_fail (files_list != NULL);

  for (guint i = 0; i < files_list->len; i++)
    {
      GFile *file = g_ptr_array_index (files_list, i);

      g_file_delete (file, NULL, NULL);
    }
}

typedef struct
{
  GFile     *base_dir;
  GPtrArray *basenames;
} RemoveState;

static void
remove_state_free (RemoveState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->base_dir);
  dzl_clear_pointer (&state->basenames, g_ptr_array_unref);
}

static void
remove_basenames_worker (IdeTask      *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  RemoveState *state = (RemoveState *)task_data;

  g_assert (IDE_IS_TASK (task));
  g_assert (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  for (guint i = 0; i < state->basenames->len; i++)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GFile) file = g_file_get_child (state->base_dir,
                                                g_ptr_array_index (state->basenames, i));

      if (!g_file_delete (file, cancellable, &error))
        g_debug ("%s", error->message);
      else
        {
          g_autofree gchar *path = g_file_get_path (file);
          g_debug ("file delete:%s", path);
        }

      if (g_cancellable_is_cancelled (cancellable))
        break;
    }

  ide_task_return_boolean (task, TRUE);
}

void
ide_gi_utils_remove_basenames_async (GFile               *base_dir,
                                     GPtrArray           *basenames,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  RemoveState *state;

  g_return_if_fail (G_IS_FILE (base_dir));
  g_return_if_fail (basenames != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (callback != NULL);

  /* TODO: check we are in a cache or temp dir to be extra-sure */

  task = ide_task_new (NULL, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_utils_remove_basenames_async);

  state = g_slice_new0 (RemoveState);
  state->base_dir = g_object_ref (base_dir);
  state->basenames = g_ptr_array_ref (basenames);
  ide_task_set_task_data (task, state, remove_state_free);
  ide_task_run_in_thread (task, remove_basenames_worker);
}

gboolean
ide_gi_utils_remove_basenames_finish (GAsyncResult  *result,
                                      GError       **error)
{
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

gboolean
ide_gi_utils_get_gir_components (GFile *file,
                                 gchar **name,
                                 gchar **version)
{
  g_autofree gchar *basename = NULL;
  gchar *version_pos;
  gint len;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  basename = g_file_get_basename (file);
  len = strlen (basename);

  if (!g_str_has_suffix (basename, ".gir"))
    return FALSE;

  if (NULL == (version_pos = g_utf8_strrchr (basename, len, '-')))
    {
      if (name != NULL)
        {
          *name = g_strndup (basename, len - EXTENSION_LEN);
          return TRUE;
        }
    }

  if (name != NULL)
    *name = g_strndup (basename, version_pos - basename);

  if (version != NULL)
    *version = g_strndup (version_pos + 1, len - (version_pos - basename) - EXTENSION_LEN - 1);

  return TRUE;
}

const gchar *
ide_gi_utils_stability_to_string (IdeGiStability stability)
{
  g_return_val_if_fail ((gint)stability < G_N_ELEMENTS (IDE_GI_STABILITY_NAMES), NULL);

  return IDE_GI_STABILITY_NAMES[stability];
}

const gchar *
ide_gi_utils_scope_to_string (IdeGiScope scope)
{
  g_return_val_if_fail ((gint)scope < G_N_ELEMENTS (IDE_GI_SCOPE_NAMES), NULL);

  return IDE_GI_SCOPE_NAMES[scope];
}

const gchar *
ide_gi_utils_direction_to_string (IdeGiDirection direction)
{
  g_return_val_if_fail ((gint)direction < G_N_ELEMENTS (IDE_GI_DIRECTION_NAMES), NULL);

  return IDE_GI_DIRECTION_NAMES[direction];
}

const gchar *
ide_gi_utils_transfer_ownership_to_string (IdeGiTransferOwnership transfer_ownership)
{
  g_return_val_if_fail ((gint)transfer_ownership < G_N_ELEMENTS (IDE_GI_TRANSFER_OWNERSHIP_NAMES), NULL);

  return IDE_GI_TRANSFER_OWNERSHIP_NAMES[transfer_ownership];
}

const gchar *
ide_gi_utils_signal_when_to_string (IdeGiSignalWhen signal_when)
{
  g_return_val_if_fail ((gint)signal_when < G_N_ELEMENTS (IDE_GI_SIGNAL_WHEN_NAMES), NULL);

  return IDE_GI_SIGNAL_WHEN_NAMES[signal_when];
}

const gchar *
ide_gi_utils_type_to_string (IdeGiBasicType type)
{
  g_return_val_if_fail ((gint)type < G_N_ELEMENTS (IDE_GI_BASIC_TYPE_NAMES), NULL);

  return IDE_GI_BASIC_TYPE_NAMES[type];
}

const gchar *
ide_gi_utils_prefix_type_to_string (IdeGiPrefixType type)
{
  g_return_val_if_fail (__builtin_popcountll (type) == 1 &&
                        (gint)type < (1 << G_N_ELEMENTS (IDE_GI_PREFIX_TYPE_NAMES)), NULL);

  return IDE_GI_PREFIX_TYPE_NAMES[__builtin_ffsll (type) - 1];
}

const gchar *
ide_gi_utils_blob_type_to_string (IdeGiBlobType type)
{
  g_return_val_if_fail ((gint)type < G_N_ELEMENTS (IDE_GI_BLOB_TYPE_NAMES), NULL);

  return IDE_GI_BLOB_TYPE_NAMES[type];
}

const gchar *
ide_gi_utils_ns_table_to_string (IdeGiNsTable table)
{
  g_return_val_if_fail ((gint)table < IDE_GI_NS_TABLE_NB_TABLES, NULL);

  return IDE_GI_NS_TABLE_NAMES[table];
}

void
ide_gi_utils_typeref_dump (IdeGiTypeRef typeref,
                           guint        depth)
{
  g_print ("TYPEREF: type:%s is const:%d is pointer:%d offset:%d\n",
           IDE_GI_BASIC_TYPE_NAMES[typeref.type],
           typeref.is_const,
           typeref.is_pointer,
           typeref.offset);

  if (depth > 0)
    {
      /* TODO: typref need the ns to interpret the offset */
    }
}

gboolean
ide_gi_utils_parse_version (const gchar *version,
                            guint16     *major,
                            guint16     *minor,
                            guint16     *micro)
{
  gchar *end;
  guint64 tmp_major = 0;
  guint64 tmp_minor = 0;
  guint64 tmp_micro = 0;

  g_assert (version != NULL);

  tmp_major = g_ascii_strtoull (version, &end, 10);
  if (tmp_major >= 0x100 || end == version)
    return FALSE;

  if (*end != '.')
    goto next;

  version = end + 1;
  tmp_minor = g_ascii_strtoull (version, &end, 10);
  if (tmp_minor >= 0x100 || end == version)
    return FALSE;

  if (*end != '.')
    goto next;

  version = end + 1;
  tmp_micro = g_ascii_strtoull (version, &end, 10);
  if (tmp_micro >= 0x100 || end == version)
    return FALSE;

next:
  if (major != NULL)
    *major = tmp_major;

  if (minor != NULL)
    *minor = tmp_minor;

  if (micro != NULL)
    *micro = tmp_micro;

  return TRUE;
}
