/* ide-gi-index.c
 *
 * Copyright © 2017 Sébastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-gi-index"

#include "config.h"

#include <dazzle.h>
#include <string.h>

#include "./../flatpak/gbp-flatpak-runtime.h"
#include "./../flatpak/gbp-flatpak-util.h"

#include "ide-gi.h"
#include "ide-gi-macros.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-file-builder.h"
#include "ide-gi-file-builder-result.h"
#include "ide-gi-index-private.h"
#include "ide-gi-utils.h"
#include "ide-gi-version-private.h"

#include "radix-tree/ide-gi-flat-radix-tree.h"
#include "radix-tree/ide-gi-radix-tree-builder.h"

#include "ide-gi-index.h"

static void     async_initable_init   (GAsyncInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (IdeGiIndex, ide_gi_index, IDE_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_init))

#define GIR_EXTENSION_LEN 4

static guint64 index_start_time;
enum {
  PROP_0,
  PROP_CACHE_DIR,
  PROP_REPOSITORY,
  PROP_RUNTIME_ID,
  PROP_UPDATE_ON_BUILD,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

enum {
  CURRENT_VERSION_CHANGED,
  VERSION_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL];

typedef enum
{
  OFFSET_TABLE_ENTRY_TYPE_NEW,
  OFFSET_TABLE_ENTRY_TYPE_UPDATE,
  OFFSET_TABLE_ENTRY_TYPE_KEEP,
} OffsetTableEntryType;

typedef enum
{
  /* the 0 value means we set a GError */
  UPDATE_RESULT_ERROR = 0,
  UPDATE_RESULT_OK,
  UPDATE_RESULT_NEED_UPDATE,
  UPDATE_RESULT_NO_CHANGES,
} UpdateResult;

static guint64 start_update_index;

typedef struct
{
  const gchar *name;
  const gchar *ns;
} IdeGiBasicTypesInfo;

static const IdeGiBasicTypesInfo IDE_GI_BASIC_TYPES_INFO [] =
{
  { "none"     , "" },
  { "gboolean" , "" },
  { "gchar"    , "" },
  { "guchar"   , "" },
  { "gshort"   , "" },
  { "gushort"  , "" },
  { "gint"     , "" },
  { "guint"    , "" },
  { "glong"    , "" },
  { "gulong"   , "" },
  { "gssize"   , "" },
  { "gsize"    , "" },
  { "gpointer" , "" },
  { "gintptr"  , "" },
  { "guintptr" , "" },
  { "gint8"    , "" },
  { "guint8"   , "" },
  { "gint16"   , "" },
  { "guint16"  , "" },
  { "gint32"   , "" },
  { "guint32"  , "" },
  { "gint64"   , "" },
  { "guint64"  , "" },
  { "gfloat"   , "" },
  { "gdouble"  , "" },
  { "GType"    , "" },
  { "utf8"     , "" },
  { "filename" , "" },
  { "gunichar" , "" },
};

typedef struct
{
  GFile                *file;
  GArray               *global_index;
  GByteArray           *ns_ba;
  NamespaceChunk        chunk;

  gchar                *ns;
  gchar                *symbol_prefixes;
  gchar                *identifier_prefixes;

  guint8               *ro_tree_ptr;
  guint32               ro_tree_offset64b;
  guint32               ro_tree_size64b;
  guint64               mtime;

  guint8                major_version;
  guint8                minor_version;
  OffsetTableEntryType  type;
  guint16               version_count;
  guint16               has_ro_tree      : 1;
  guint16               no_minor_version : 1;
  guint16               succes           : 1;
} OffsetTableEntry;

static gboolean remove_version_emited (gpointer    data);
static void     process_queue         (IdeGiIndex *self);

static void
ns_record_free (NsRecord *self)
{
  g_return_if_fail (self != NULL);

  dzl_clear_pointer (&self->mapped_file, g_mapped_file_unref);
  g_slice_free (NsRecord, self);
}

NsRecord *
_ide_gi_index_get_ns_record (IdeGiIndex  *self,
                             const gchar *name)
{
  NsRecord *record;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (!dzl_str_empty0 (name));

  record = g_hash_table_lookup (self->files, name);
  g_assert (record != NULL);

  if (record->mapped_file == NULL)
    {
      g_autofree gchar *path = g_file_get_path (self->cache_dir);
      g_autofree gchar *ns_path = g_build_filename (path, name, NULL);

      record->mapped_file = g_mapped_file_new (ns_path, FALSE, NULL);
    }

  return record;
}

static OffsetTableEntry *
offset_table_entry_new (void)
{
  OffsetTableEntry *entry;

  entry = g_slice_new0 (OffsetTableEntry);
  entry->global_index = g_array_new (FALSE, FALSE, sizeof (IdeGiGlobalIndexEntry));
  g_array_set_clear_func (entry->global_index, ide_gi_global_index_entry_clear);

  return entry;
}

static void
offset_table_entry_free (gpointer data)
{
  OffsetTableEntry *entry = (OffsetTableEntry *)data;

  g_assert (entry != NULL);

  g_clear_object (&entry->file);

  dzl_clear_pointer (&entry->global_index, g_array_unref);
  dzl_clear_pointer (&entry->ns_ba, g_byte_array_unref);
  dzl_clear_pointer (&entry->ns, g_free);
  dzl_clear_pointer (&entry->symbol_prefixes, g_free);
  dzl_clear_pointer (&entry->identifier_prefixes, g_free);

  g_slice_free (OffsetTableEntry, entry);
}

gboolean
ide_gi_index_is_updating (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), FALSE);

  return self->is_updating;
}

static guint64
get_mtime (GFile *file)
{
  g_autoptr(GFileInfo) file_info = NULL;

  g_assert (G_IS_FILE (file));

  /* Some flatpak runtime files have the mtime set to 0, so let's use the ctime */
  file_info = g_file_query_info (file,
                                 G_FILE_ATTRIBUTE_TIME_CHANGED,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 NULL);
  if (file_info == NULL)
    return 0;

  return g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_CHANGED);
}

/* major_version, minor_version and mtime are only valid
 * for a non-NULL namespace ns returned.
 *
 * Notice that ns is truncated from the version in place.
 */
static gchar *
gir_file_get_infos (GFile    *file,
                    gint     *major_version,
                    gint     *minor_version,
                    gboolean *no_minor_version,
                    guint64  *mtime)
{
  gchar *ns;
  gchar *tmp;
  gint tmp_major_version = 0;
  gint tmp_minor_version = 0;
  gboolean tmp_no_minor_version = TRUE;

  g_assert (G_IS_FILE (file));

  ns = g_file_get_basename (file);
  if (g_str_has_suffix (ns, ".gir") && NULL != (tmp = strrchr (ns, '.')))
    {
      gchar *version_str;

      *tmp = '\0';
      if ((version_str = strchr (ns, '-')))
        {
          *version_str = '\0';
          version_str++;
          if (NULL == (tmp = strchr (version_str, '.')))
            tmp_major_version = g_ascii_strtoull (version_str, NULL, 10);
          else
            {
              *tmp = '\0';
              tmp++;
              tmp_major_version = g_ascii_strtoull (version_str, NULL, 10);
              tmp_minor_version = g_ascii_strtoull (tmp, NULL, 10);
              tmp_no_minor_version = FALSE;
            }
        }
    }
  else
    return NULL;

  if (mtime != NULL)
    *mtime = get_mtime (file);

  if (major_version != NULL)
    *major_version = tmp_major_version;

  if (minor_version != NULL)
    *minor_version = tmp_minor_version;

  if (no_minor_version != NULL)
    *no_minor_version = tmp_no_minor_version;

  return ns;
}

/* The #IdeGiVersion of the dt need to be locked before using this */
static DtPayload *
dt_ns_lookup (IdeGiFlatRadixTree *dt,
              const gchar        *nsname,
              gint                major_version,
              gint                minor_version)
{
  guint64 *payloads;
  guint nb_payloads;

  g_assert (dt != NULL);
  g_assert (nsname != NULL && *nsname != '\0');

  if (ide_gi_flat_radix_tree_lookup (dt, nsname, &payloads, &nb_payloads))
    {
      DtPayload *payload = (DtPayload *)payloads;
      guint nb_dt_payloads;

      nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

      for (guint i = 0; i < nb_dt_payloads; i++, payload++)
        if ((payload->type & IDE_GI_PREFIX_TYPE_NAMESPACE) &&
            payload->id.major_version == major_version &&
            payload->id.minor_version == minor_version)
          {
            return payload;
          }
    }

  return NULL;
}

static GArray *
setup_basic_types (IdeGiIndex *self,
                   GByteArray *strings_pool)
{
  GArray *ar;
  IdeGiTypeBlob blob = {{0}, 0};

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (strings_pool != NULL);

  ar = g_array_new (FALSE, TRUE, sizeof (IdeGiTypeBlob));
  blob.common.blob_type = IDE_GI_BLOB_TYPE_TYPE;

  for (guint i = 0; i < G_N_ELEMENTS (IDE_GI_BASIC_TYPES_INFO); i++)
    {
      const IdeGiBasicTypesInfo *info = &IDE_GI_BASIC_TYPES_INFO [i];
      guint offset = strings_pool->len;

      g_byte_array_append (strings_pool, (guint8 *)info->name, strlen (info->name) + 1);
      blob.common.name = offset;

      g_array_append_val (ar, blob);
    }

  return ar;
}

/* Insert indexes from an entry into the tree */
static void
fill_global_indexes_builder (IdeGiRadixTreeBuilder *tree,
                             OffsetTableEntry      *entry)
{
  DtPayload payload = {0};

  g_assert (IDE_IS_GI_RADIX_TREE_BUILDER (tree));
  g_assert (entry != NULL);
  g_assert (entry->global_index != NULL);

  payload.is_new = TRUE;
  payload.id = (IdeGiNamespaceId) {.major_version = entry->major_version,
                                   .minor_version = entry->minor_version,
                                   .file_version = entry->version_count,
                                   .offset64b = entry->chunk.offset64b,
                                   .no_minor_version = entry->no_minor_version};

  payload.namespace_size64b = entry->chunk.size64b;
  payload.has_ro_tree = entry->has_ro_tree;
  payload.mtime = entry->mtime;

  for (guint i = 0; i < entry->global_index->len; i++)
    {
      IdeGiGlobalIndexEntry *index_entry;
      IdeGiRadixTreeNode *node;

      index_entry = &g_array_index (entry->global_index, IdeGiGlobalIndexEntry, i);

      payload.type = index_entry->type;
      payload.object_offset = index_entry->object_offset;
      payload.object_type = index_entry->object_type;
      payload.is_buildable = index_entry->is_buildable;

      if ((node = ide_gi_radix_tree_builder_lookup (tree, index_entry->name)))
        ide_gi_radix_tree_builder_node_append_payload (node, DT_PAYLOAD_N64_SIZE, &payload);
      else
        ide_gi_radix_tree_builder_add (tree, index_entry->name, DT_PAYLOAD_N64_SIZE, &payload);
    }
}

static void
pad_to_64b_mutliple (GByteArray *ba)
{
  guint64 pattern = 0;
  guint padding;

  g_assert (ba != NULL);

  padding = (8 - (ba->len & 7)) & 7;

  if (padding > 0)
    g_byte_array_append (ba, (guint8 *)&pattern, padding);
}

/* We accces it from a thread but a self->is_updating check when updating
 * protect us from concurent access.
 */
static void
increment_file_count (IdeGiIndex       *self,
                      OffsetTableEntry *entry)
{
  g_autofree gchar *name = NULL;
  NsRecord *record;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (entry != NULL);

  if (entry->no_minor_version)
    name = g_strdup_printf ("%s-%d@%d%s",
                            entry->ns,
                            entry->major_version,
                            entry->version_count,
                            INDEX_NAMESPACE_EXTENSION);
  else
    name = g_strdup_printf ("%s-%d.%d@%d%s",
                            entry->ns,
                            entry->major_version,
                            entry->minor_version,
                            entry->version_count,
                            INDEX_NAMESPACE_EXTENSION);

  if (NULL == (record = g_hash_table_lookup (self->files, name)))
    {
      record = g_slice_new0 (NsRecord);
      record->count = 1;
      g_hash_table_insert (self->files, g_steal_pointer (&name), record);
    }
  else
    ++record->count;
}

/* The returned array maybe empty but not NULL */
static GPtrArray *
decrement_version_files_count (IdeGiIndex   *self,
                               IdeGiVersion *version)
{
  GPtrArray *basenames;
  guint i = 0;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_GI_VERSION (version));

  basenames = ide_gi_version_get_namespaces_basenames (version);

  /* We filter basenames in-place */
  while (i < basenames->len)
    {
      const gchar *basename = g_ptr_array_index (basenames, i);
      NsRecord *record;

      if ((record = g_hash_table_lookup (self->files, basename)))
        {
          g_assert (record->count > 0);

          --record->count;
          if (record->count == 0)
            {
              g_hash_table_remove (self->files, basename);
              i++;
            }
          else
            g_ptr_array_remove_index_fast (basenames, i);
        }
      else
        g_ptr_array_remove_index_fast (basenames, i);
    }

  return basenames;
}

static GByteArray *
index_create (IdeGiIndex  *self,
              GPtrArray   *offset_table,
              const gchar *runtime_id)
{
  g_autoptr(IdeGiRadixTreeBuilder) new_dt_builder = NULL;
  g_autoptr(GByteArray) index_strings = NULL;
  g_autoptr(GArray) basic_types_ar = NULL;
  g_autoptr(GByteArray) dt_ba = NULL;
  GByteArray *index_ba;
  IndexHeader header = {0};
  guint32 offset64b = 0;
  guint32 basic_types_size;
  guint32 basic_types_padding_64;
  guint32 namespace_offset64b = 0;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (offset_table != NULL);
  g_assert (!dzl_str_empty0 (runtime_id));

  index_strings = g_byte_array_new ();
  g_byte_array_append (index_strings, (guint8 *)runtime_id, strlen (runtime_id) + 1);
  basic_types_ar = setup_basic_types (self, index_strings);

  new_dt_builder = ide_gi_radix_tree_builder_new ();
  for (guint i = 0; i < offset_table->len; i++)
    {
      OffsetTableEntry *entry = g_ptr_array_index (offset_table, i);

      if (!entry->succes)
        continue;

      entry->chunk.offset64b = namespace_offset64b;
      namespace_offset64b += entry->chunk.size64b;

      fill_global_indexes_builder (new_dt_builder, entry);
      increment_file_count (self, entry);
    }

  dt_ba = ide_gi_radix_tree_builder_serialize (new_dt_builder);
  g_assert (dt_ba != NULL && IS_64B_MULTIPLE (dt_ba->len));

  header.abi_version = INDEX_ABI_VERSION;
  header.id_offset64b = 0;

  offset64b += sizeof (IndexHeader) >> 3;
  header.dt_offset64b = offset64b;
  header.dt_size64b = dt_ba->len >> 3;

  offset64b += header.dt_size64b;
  header.namespaces_offset64b = offset64b;
  header.namespaces_size64b = namespace_offset64b;

  offset64b += namespace_offset64b;
  header.basic_types_offset64b = offset64b;
  header.n_basic_types = basic_types_ar->len;

  basic_types_size = basic_types_ar->len * g_array_get_element_size (basic_types_ar);
  basic_types_padding_64 = (8 - (basic_types_size & 7)) & 7;
  offset64b += (basic_types_size + basic_types_padding_64) >> 3;

  header.strings_offset64b = offset64b;
  header.strings_size = index_strings->len;

  index_ba = g_byte_array_new ();
  g_byte_array_append (index_ba, (guint8 *)&header, sizeof (IndexHeader));
  g_byte_array_append (index_ba, dt_ba->data, dt_ba->len);

  for (guint i = 0; i < offset_table->len; i++)
    {
      OffsetTableEntry *entry = g_ptr_array_index (offset_table, i);
      IdeGiNamespaceHeader ns_header = {0};
      /* Only used when the traces are activated */
      gsize len G_GNUC_UNUSED;

      if (!entry->succes)
        continue;

      if (entry->type == OFFSET_TABLE_ENTRY_TYPE_KEEP)
        {
          g_byte_array_append (index_ba, entry->chunk.ptr, entry->chunk.size64b << 3);
        }
      else
        {
          ns_header.ro_tree_offset64b = (sizeof (IdeGiNamespaceHeader) + entry->ns_ba->len) >> 3;
          ns_header.ro_tree_size64b = entry->ro_tree_size64b;
          ns_header.size64b = ns_header.ro_tree_offset64b + entry->ro_tree_size64b;
          len = index_ba->len;

          g_byte_array_append (index_ba, (guint8 *)&ns_header, sizeof (IdeGiNamespaceHeader));
          g_byte_array_append (index_ba, entry->ns_ba->data, entry->ns_ba->len);
          g_byte_array_append (index_ba, entry->ro_tree_ptr, entry->ro_tree_size64b << 3);

          IDE_TRACE_MSG ("Index namespace: %s offset:%d (%ldb)",
                         entry->ns,
                         entry->chunk.offset64b,
                         index_ba->len - len);
        }
    }

  g_byte_array_append (index_ba, (guint8 *)basic_types_ar->data, basic_types_size);
  pad_to_64b_mutliple (index_ba);

  g_byte_array_append (index_ba, (guint8 *)(index_strings->data), index_strings->len);
  pad_to_64b_mutliple (index_ba);

  g_assert (IS_64B_MULTIPLE (index_ba->len));
  IDE_TRACE_MSG ("New index: size:%db", index_ba->len);

  return index_ba;
}

static void
pick_global_indexes_filter_func (const gchar *word,
                                 guint64     *payloads,
                                 guint        nb_payloads,
                                 gpointer     user_data)
{
  OffsetTableEntry *entry = (OffsetTableEntry *)user_data;
  DtPayload *payload;
  guint nb_dt_payloads;

  g_assert (entry != NULL);
  g_assert (payloads != NULL);
  g_assert (word != NULL);
  g_assert (nb_payloads % (sizeof (DtPayload) >> 3) == 0);

  payload = (DtPayload *)payloads;
  nb_dt_payloads = nb_payloads / DT_PAYLOAD_N64_SIZE;

  for (guint i = 0; i < nb_dt_payloads; i++, payload++)
    {
      /* We only pick the entries from the same namespace */
      if (payload->id.offset64b == entry->chunk.offset64b)
        {
          IdeGiGlobalIndexEntry index_entry;

          index_entry.name = g_strdup (word);
          index_entry.object_offset = payload->object_offset;
          index_entry.type = payload->type;
          index_entry.object_type = payload->object_type;
          index_entry.is_buildable = payload->is_buildable;

          g_array_append_val (entry->global_index, index_entry);
        }
    }
}

/* We pick the global indexes from an existing global tree
 * and we push them into to entry for the current namespace.
 */
static void
pick_global_indexes (OffsetTableEntry   *entry,
                     IdeGiFlatRadixTree *index_dt)
{
  g_assert (entry != NULL);
  g_assert (index_dt != NULL);

  ide_gi_flat_radix_tree_foreach (index_dt, pick_global_indexes_filter_func, entry);
}

static void G_GNUC_UNUSED
dt_tree_dump_func (const gchar *word,
                   guint64     *payloads,
                   guint        nb_payloads,
                   gpointer     user_data)
{
  DtPayload *dt_payload = (DtPayload *)payloads;
  guint nb_dt_payloads;
  const gchar *type;

  g_assert (word != NULL);
  g_assert (payloads != NULL);
  g_assert (nb_payloads % (sizeof (DtPayload) >> 3) == 0);

  nb_dt_payloads = nb_payloads / (sizeof (DtPayload) >> 3);
  g_debug ("DT_PAYLOADS:%s %d", word, nb_dt_payloads);
  while (nb_dt_payloads > 0)
    {
      type = ide_gi_utils_prefix_type_to_string (dt_payload->type);
      g_debug ("%s %s M:%d m:%d mtime:%ld",
               word,
               type,
               dt_payload->id.major_version,
               dt_payload->id.minor_version,
               dt_payload->mtime);

      dt_payload++;
      --nb_dt_payloads;
    }
}

static void
ide_gi_index_get_runtime_xdg_data_dirs_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GBytes) stdout_buf = NULL;
  g_autoptr(GBytes) stderr_buf = NULL;
  g_autoptr(GError) error = NULL;
  gchar *xdg_data_dirs = NULL;
  gsize size;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (!ide_subprocess_communicate_finish (subprocess, result, &stdout_buf, &stderr_buf, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      if (stdout_buf != NULL)
        xdg_data_dirs = g_bytes_unref_to_data (g_steal_pointer (&stdout_buf), &size);

      if (xdg_data_dirs == NULL)
        xdg_data_dirs = g_strdup ("");

      ide_task_return_pointer (task, xdg_data_dirs, g_free);
    }
}

static void
ide_gi_index_get_runtime_xdg_data_dirs_async (IdeGiIndex          *self,
                                              IdeRuntime          *runtime,
                                              GCancellable        *cancellable,
                                              GAsyncReadyCallback  callback,
                                              gpointer             user_data)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_RUNTIME (runtime));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_index_get_runtime_xdg_data_dirs_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  /* TODO: bypass and use g_get_environ/g_environ_getenv if we are on host runtime */
  if (NULL == (launcher = ide_runtime_create_launcher (runtime, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_launcher_set_clear_env (launcher, FALSE);
  ide_subprocess_launcher_push_argv (launcher, "printenv");
  ide_subprocess_launcher_push_argv (launcher, "XDG_DATA_DIRS");

  if (NULL == (subprocess = ide_subprocess_launcher_spawn (launcher, cancellable, &error)))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ide_subprocess_communicate_async (subprocess,
                                    NULL,
                                    cancellable,
                                    ide_gi_index_get_runtime_xdg_data_dirs_cb,
                                    g_steal_pointer (&task));
}

static gchar *
ide_gi_index_get_runtime_xdg_data_dirs_finish (IdeGiIndex    *self,
                                               GAsyncResult  *result,
                                               GError       **error)
{
  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
ide_gi_index_get_gir_directories_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(IdeTask) task = user_data;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *xdg_data_dirs = NULL;
  g_autoptr(GPtrArray) gir_search_paths = NULL;
  IdeContext *context;
  IdeRuntime *runtime;
  IdeProject *project;
  GPtrArray *gir_directories;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));

  if (NULL == (xdg_data_dirs = ide_gi_index_get_runtime_xdg_data_dirs_finish (self, result, &error)))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      /* TODO: handle from project run env: "GI_GIR_PATH" */
      runtime = ide_task_get_task_data (task);
      gir_directories = g_ptr_array_new_with_free_func (g_object_unref);
      if (!dzl_str_empty0 (xdg_data_dirs))
        {
          g_auto(GStrv) paths = g_strsplit (xdg_data_dirs, ":", -1);

          for (guint i = 0; paths[i] != NULL; i++)
            {
              g_autofree gchar *path = g_build_filename (paths[i], "gir-1.0", NULL);
              g_autoptr(GFile) src_file = g_file_new_for_path (path);

              g_ptr_array_add (gir_directories, ide_runtime_translate_file (runtime, src_file));
            }
        }
      else
        {
          {
            g_autoptr (GFile) src_file = g_file_new_for_path ("/usr/share/gir-1.0");
            g_ptr_array_add (gir_directories, ide_runtime_translate_file (runtime, src_file));
          }

          {
            g_autoptr (GFile) src_file = g_file_new_for_path ("/usr/local/share/gir-1.0");
            g_ptr_array_add (gir_directories, ide_runtime_translate_file (runtime, src_file));
          }
        }

      if ((context = ide_object_get_context (IDE_OBJECT (self))) &&
          (project = ide_context_get_project (context)))
        {
          g_autoptr(GFile) src_file = NULL;
          g_autofree gchar *project_gir = NULL;

          project_gir = g_build_filename ("/usr/share",
                                          ide_project_get_name (project),
                                          "gir-1.0",
                                          NULL);

          src_file = g_file_new_for_path (project_gir);
          g_ptr_array_add (gir_directories, ide_runtime_translate_file (runtime, src_file));
        }

      if (G_IS_FILE (self->staging_dir))
        g_ptr_array_add (gir_directories, g_object_ref (self->staging_dir));

      gir_search_paths = ide_gi_repository_get_gir_search_paths (self->repository);
      for (guint i = 0; i < gir_search_paths->len; i++)
        g_ptr_array_add (gir_directories,
                         g_object_ref (g_ptr_array_index (gir_search_paths, i)));

      ide_task_return_pointer (task, gir_directories, (GDestroyNotify)g_ptr_array_unref);
    }
}

static void
ide_gi_index_get_gir_directories_async (IdeGiIndex          *self,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  IdeContext *context;
  IdeRuntimeManager *rt_manager;
  IdeRuntime *runtime;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_index_get_gir_directories_async);
  ide_task_set_priority (task, G_PRIORITY_LOW);

  context = ide_object_get_context (IDE_OBJECT (self));
  rt_manager = ide_context_get_runtime_manager (context);
  if ((runtime = ide_runtime_manager_get_runtime (rt_manager, self->runtime_id)))
    {
      ide_task_set_task_data (task, g_object_ref (runtime), g_object_unref);
      ide_gi_index_get_runtime_xdg_data_dirs_async (self,
                                                    runtime,
                                                    cancellable,
                                                    ide_gi_index_get_gir_directories_cb,
                                                    g_steal_pointer (&task));
    }
  else
    ide_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Runtime '%s' not found",
                               self->runtime_id);
}

/**
 * ide_gi_index_get_gir_directories_finish:
 *
 * Returns: (transfer full) (element-type Gio.File): A #GPtrArray
 */
static GPtrArray *
ide_gi_index_get_gir_directories_finish (IdeGiIndex    *self,
                                         GAsyncResult  *result,
                                         GError       **error)
{
  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

static void
remove_basenames_cb (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr(IdeGiIndex) self = (IdeGiIndex *)user_data;

  g_return_if_fail (IDE_IS_GI_INDEX (self));
  g_return_if_fail (G_IS_ASYNC_RESULT (result));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  ide_gi_utils_remove_basenames_finish (result, NULL);

  /* Lock for remove_queue */
  g_mutex_lock (&self->mutex);

  if (!g_queue_is_empty (self->remove_queue))
    g_idle_add_full (G_PRIORITY_LOW,
                     remove_version_emited,
                     g_object_ref (self),
                     g_object_unref);

  g_mutex_unlock (&self->mutex);
}

static gboolean
remove_version_emited (gpointer data)
{
  IdeGiIndex *self = (IdeGiIndex *)data;
  g_autoptr(IdeGiVersion) version = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_MAIN_THREAD ());

  /* Lock for versions table and remove_queue */
  g_mutex_lock (&self->mutex);

  if ((version = g_queue_pop_head (self->remove_queue)))
    {
      g_autoptr(GPtrArray) ar = NULL;
      gchar *index_name;
      guint count;
      gboolean found;

      g_assert (version->is_removing == TRUE);

      ar = decrement_version_files_count (self, version);
      count = ide_gi_version_get_count (version);
      index_name = ide_gi_version_get_versionned_index_name (version);
      g_ptr_array_add (ar, index_name);
      ide_gi_utils_remove_basenames_async (self->cache_dir,
                                           ar,
                                           NULL,
                                           remove_basenames_cb,
                                           g_object_ref (self));

      found = g_hash_table_remove (self->versions, version);
      g_assert (found == TRUE);

      g_mutex_unlock (&self->mutex);
      IDE_TRACE_MSG ("Version @%i removed", ide_gi_version_get_count (version));

      g_signal_emit (self, signals [VERSION_REMOVED], 0, count);
    }
  else
    g_mutex_unlock (&self->mutex);

  return G_SOURCE_REMOVE;
}

/* This method is used under a lock from:
 * - ide_gi_namespace_ref/unref
 * - ide_gi_version_set_namespace_state
 */
void
_ide_gi_index_version_remove (IdeGiIndex   *self,
                              IdeGiVersion *version)
{
  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_GI_VERSION (version));

  /* Lock for versions table, remove_queue and current_version */
  g_mutex_lock (&self->mutex);

  g_assert (version != self->current_version);

  /* The version has a last ref from the dispose that will be
   * freed when removed from the queue.
   */
  g_queue_push_tail (self->remove_queue, version);
  g_idle_add_full (G_PRIORITY_LOW,
                   remove_version_emited,
                   g_object_ref (self),
                   g_object_unref);

  g_mutex_unlock (&self->mutex);
}

static void
set_current_version (IdeGiIndex   *self,
                     IdeGiVersion *version)
{
  IdeGiVersion *old_current_version;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_GI_VERSION (version) && version != self->current_version);
  g_assert (IDE_IS_MAIN_THREAD ());

  /* Lock for versions table, remove_queue and current_version */
  g_mutex_lock (&self->mutex);

  /* We do the version change here because the unref can lead us
   * to the dispose method that check this variable.
   */
  old_current_version = self->current_version;
  self->current_version = g_object_ref (version);
  g_signal_emit (self, signals [CURRENT_VERSION_CHANGED], 0, self->current_version);

  if (old_current_version != NULL)
    {
      g_hash_table_add (self->versions, old_current_version);
      g_mutex_unlock (&self->mutex);

      g_object_unref (old_current_version);
      return;
    }

  g_mutex_unlock (&self->mutex);
}

/**
 * ide_gi_index_get_current_version:
 * @self: a #IdeGiIndex
 *
 * Get a ref on the current #IdeGiVersion.
 * Because it operate under a lock and ref the version, it's thread safe.
 *
 * Returns: (nullable) (transfer full): The current #IdeGiVersion.
 *          If no current version is set, %NULL is returned.
 */
IdeGiVersion *
ide_gi_index_get_current_version (IdeGiIndex *self)
{
  IdeGiVersion *version = NULL;

  g_return_val_if_fail (IDE_IS_GI_INDEX (self), NULL);

  /* Lock for current_version */
  g_mutex_lock (&self->mutex);

  if (self->current_version != NULL)
    version = g_object_ref (self->current_version);

  g_mutex_unlock (&self->mutex);
  return version;
}

typedef struct {
  IdeGiIndex       *index;
  GPtrArray        *offset_table;
  GPtrArray        *gir_paths;
  OffsetTableEntry *entry;
  GCancellable     *cancellable;
} UpdateState;

static UpdateState *
update_state_new (IdeGiIndex       *index,
                  GPtrArray        *offset_table,
                  OffsetTableEntry *entry,
                  GCancellable     *cancellable)
{
  UpdateState *state;

  g_assert (IDE_IS_GI_INDEX (index));
  g_assert (offset_table != NULL);
  g_assert (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  state = g_slice_new0 (UpdateState);
  state->index = g_object_ref (index);
  state->offset_table = g_ptr_array_ref (offset_table);
  state->gir_paths = g_ptr_array_new_with_free_func (g_object_unref);
  state->entry = entry;

  return state;
}

static void
update_state_free (UpdateState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->index);
 dzl_clear_pointer (&state->offset_table, g_ptr_array_unref);
 dzl_clear_pointer (&state->gir_paths, g_ptr_array_unref);

  g_slice_free (UpdateState, state);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (UpdateState, update_state_free)

static void
ide_gi_index_end_worker (IdeTask      *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  IdeGiIndex *self = (IdeGiIndex *)source_object;
  UpdateState *state = (UpdateState *)task_data;
  IdeGiVersion *version;
  g_autofree gchar *index_path = NULL;
  g_autofree gchar *index_name = NULL;
  g_autoptr(GByteArray) index = NULL;
  g_autoptr(GFile) index_file = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  index = index_create (self, state->offset_table, ide_gi_index_get_runtime_id (self));

  index_name = g_strdup_printf ("%s@%d%s", INDEX_FILE_NAME, self->version_count, INDEX_FILE_EXTENSION);
  index_file = g_file_get_child (self->cache_dir, index_name);
  index_path = g_file_get_path (index_file);

  if (!g_file_set_contents (index_path, (const gchar *)index->data, index->len, &error))
    {
      self->state = IDE_GI_INDEX_STATE_NOT_INIT;
      ide_task_return_error (task, g_steal_pointer (&error));
    }
  else
    {
      version = ide_gi_version_new (self,
                                    self->cache_dir,
                                    self->version_count,
                                    NULL,
                                    &error);
      if (version != NULL)
        {
          g_debug ("Index files written in %ldµs to:%s", g_get_monotonic_time () - index_start_time, index_path);
          ide_task_return_pointer (task, version, g_object_unref);
        }
      else
        ide_task_return_error (task, g_steal_pointer (&error));
    }
}

static void
ide_gi_index_start_worker (IdeTask      *updater_task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  IdeGiIndex *self = (IdeGiIndex *)source_object;
  UpdateState *state = (UpdateState *)task_data;
  IdeGiVersion *current_version;
  g_autoptr(GPtrArray) gir_files = NULL;
  g_autoptr(GPtrArray) project_girs = NULL;
  IdeGiFlatRadixTree *current_index_dt = NULL;
  guint new_gir = 0;
  guint update_gir = 0;
  guint keep_gir = 0;

  IDE_ENTRY;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_TASK (updater_task));
  g_assert (state != NULL);
  g_assert (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  index_start_time = g_get_monotonic_time ();

  /* TODO: protect from a new current version ? */
  current_version = self->current_version;
  gir_files = ide_gi_utils_get_files_from_directories (state->gir_paths, ".gir", FALSE);
  project_girs = ide_gi_repository_get_project_girs (self->repository);
  for (guint i = 0; i <project_girs->len; i++)
    {
      GFile *file = g_ptr_array_index (project_girs, i);
      g_ptr_array_add (gir_files, g_object_ref (file));
    }

  ide_gi_utils_files_list_dedup (gir_files);

  if (current_version != NULL)
    current_index_dt = _ide_gi_version_get_index_dt (current_version);

  for (guint i = 0; i < gir_files->len; i++)
    {
      g_autofree gchar *nsname = NULL;
      GFile *file;
      gint major_version;
      gint minor_version;
      guint64 mtime;
      gboolean no_minor_version;

      file = g_ptr_array_index (gir_files, i);
      if ((nsname = gir_file_get_infos (file,
                                        &major_version,
                                        &minor_version,
                                        &no_minor_version,
                                        &mtime)))
        {
          OffsetTableEntry *entry;
          DtPayload *payload;

          entry = offset_table_entry_new ();
          entry->file = g_object_ref (file);
          entry->no_minor_version = no_minor_version;
          entry->major_version = major_version;
          entry->minor_version = minor_version;
          entry->mtime = mtime;

          if (current_version != NULL &&
              NULL != (payload = dt_ns_lookup (current_index_dt, nsname, major_version, minor_version)))
            {
              /* TODO: We can check against a file hash too but it complicates the design */
              if (mtime <= payload->mtime)
                {
                  keep_gir++;
                  entry->type = OFFSET_TABLE_ENTRY_TYPE_KEEP;
                  entry->succes = TRUE;

                  entry->chunk = _ide_gi_version_get_namespace_chunk_from_id (current_version, payload->id);
                  entry->ns = g_strdup (_namespacechunk_get_namespace (&entry->chunk));
                  entry->version_count = payload->id.file_version;
                  entry->symbol_prefixes = g_strdup (_namespacechunk_get_c_symbol_prefixes (&entry->chunk));
                  entry->identifier_prefixes = g_strdup (_namespacechunk_get_c_identifier_prefixes (&entry->chunk));

                  pick_global_indexes (entry, current_index_dt);

                  {
                    g_autofree gchar *path = g_file_get_path (file);
                    IDE_TRACE_MSG ("Keep namespace:%s (version=%d)", path, entry->version_count);
                  }
                }
              else
                {
                  update_gir++;
                  entry->type = OFFSET_TABLE_ENTRY_TYPE_UPDATE;
                  entry->version_count = self->version_count;

                  {
                    g_autofree gchar *path = g_file_get_path (file);
                    IDE_TRACE_MSG ("Update namespace:%s (version=%d)", path, entry->version_count);
                  }
                }
            }
          else
            {
              new_gir++;
              entry->type = OFFSET_TABLE_ENTRY_TYPE_NEW;
              entry->version_count = self->version_count;

              {
                g_autofree gchar *path = g_file_get_path (file);
                IDE_TRACE_MSG ("New namespace:%s (version=%d)", path, entry->version_count);
              }
            }

          g_ptr_array_add (state->offset_table, entry);
        }
    }

  if (new_gir == 0 && update_gir == 0)
    {
      g_debug ("No changes: namespaces keeped:%d", keep_gir);
      g_debug ("Index no change in %ldµs", g_get_monotonic_time () - index_start_time);
      ide_task_return_int (updater_task, UPDATE_RESULT_NO_CHANGES);
    }
  else
    {
      g_debug ("Generated files: new:%d update:%d keep:%d", new_gir, update_gir, keep_gir);
      ide_task_return_int (updater_task, UPDATE_RESULT_NEED_UPDATE);
    }

  IDE_EXIT;
}

static void
process_result (IdeGiIndex   *self,
                UpdateResult  result,
                GError       *error)
{
  g_autoptr(IdeTask) update_task = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_MAIN_THREAD ());

  update_task = g_queue_pop_head (self->update_queue);
  g_assert (IDE_IS_TASK (update_task));

  if (result == UPDATE_RESULT_ERROR)
    ide_task_return_error (update_task, g_error_copy (error));
  else
    ide_task_return_boolean (update_task, TRUE);

  self->is_updating = FALSE;
}

static void
update_async_cb2 (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(IdeGiVersion) version = NULL;
  g_autoptr(GError) error = NULL;
  UpdateResult update_result;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  if ((version = ide_task_propagate_pointer ((IdeTask *)result, &error)))
    {
      update_result = UPDATE_RESULT_OK;
      self->state = IDE_GI_INDEX_STATE_READY;

      set_current_version (self, version);

      /* We update the version count for the next version */
      self->version_count++;
    }
  else
    update_result = UPDATE_RESULT_ERROR;

  process_result (self, update_result, error);
  process_queue (self);
}

/* We run this task for each new or changed .gir file, it generates the .ns file
 * and return some global data like global indexes and root objects tree.
 */
static void
pool_func (GTask        *task,
           gpointer      source_object,
           gpointer      task_data,
           GCancellable *cancellable)
{
  IdeGiIndex *self = (IdeGiIndex *)source_object;
  UpdateState *state = (UpdateState *)task_data;
  OffsetTableEntry *entry = NULL;
  g_autoptr(IdeGiFileBuilderResult) result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);

  entry = state->entry;
  if ((result = ide_gi_file_builder_generate (self->file_builder,
                                              entry->file,
                                              self->cache_dir,
                                              entry->version_count,
                                              &error)))
    {
      GByteArray *ro_tree_ba;

      entry->ns = g_steal_pointer (&result->ns);
      entry->symbol_prefixes = g_steal_pointer (&result->symbol_prefixes);
      entry->identifier_prefixes = g_steal_pointer (&result->identifier_prefixes);
      entry->ns_ba = g_byte_array_ref (result->ns_ba);
      entry->global_index = g_array_ref (result->global_index);

      g_assert (IS_64B_MULTIPLE (entry->ns_ba->len));

      if (result->ro_tree != NULL && !ide_gi_radix_tree_builder_is_empty (result->ro_tree))
        {
          if ((ro_tree_ba = ide_gi_radix_tree_builder_serialize (result->ro_tree)))
            {
              g_assert (IS_64B_MULTIPLE (ro_tree_ba->data));

              entry->has_ro_tree = TRUE;
              entry->ro_tree_size64b = ro_tree_ba->len >> 3;
              entry->ro_tree_ptr = g_byte_array_free (ro_tree_ba, FALSE);
            }
          else
            {
              g_autofree gchar *path = g_file_get_path (entry->file);
              g_debug ("Serialization error in root objects tree for '%s'", path);
            }
        }

      entry->chunk.size64b = ((sizeof (IdeGiNamespaceHeader) + entry->ns_ba->len) >> 3) + entry->ro_tree_size64b;
      entry->succes = TRUE;

      IDE_TRACE_MSG ("Generated namespace '%s' (%db) with: header:%ldb data:%db ro_tree:(%db)",
                     entry->ns,
                     entry->chunk.size64b << 3,
                     sizeof (IdeGiNamespaceHeader),
                     entry->ns_ba->len,
                     entry->ro_tree_size64b << 3);

      g_task_return_boolean (task, TRUE);
    }
  else
    {
      g_assert (error != NULL);
      entry->succes = FALSE;
      g_task_return_error (task, g_steal_pointer (&error));
    }
}

static void
pool_func_cb (GObject      *object,
              GAsyncResult *result,
              gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(UpdateState) state = (UpdateState *)user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  if (!g_task_propagate_boolean ((GTask *)result, &error))
    {
      /* TODO: process cancellable */
      g_warning ("%s", error->message);
    }

  if (--self->pool_count == 0 && self->pool_all_pushed)
    {
      IdeTask *end_task = ide_task_new (self,
                                        state->cancellable,
                                        update_async_cb2,
                                        NULL);

      self->pool_all_pushed = FALSE;
      ide_task_set_task_data (end_task, g_steal_pointer (&state), (GDestroyNotify)update_state_free);
      ide_task_run_in_thread (end_task, ide_gi_index_end_worker);
    }
}

static void
update_async_cb1 (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  UpdateState *state = (UpdateState *)user_data;
  g_autoptr(GError) error = NULL;
  UpdateResult update_result;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (state != NULL);
  g_assert (IDE_IS_MAIN_THREAD ());

  update_result = ide_task_propagate_int (IDE_TASK (result), &error);
  if (update_result == UPDATE_RESULT_NEED_UPDATE)
    {
      g_assert (state->offset_table->len > 0);
      for (guint i = 0; i < state->offset_table->len; i++)
        {
          OffsetTableEntry *entry = g_ptr_array_index (state->offset_table, i);

          if (entry->type != OFFSET_TABLE_ENTRY_TYPE_KEEP)
            {
              g_autoptr(GTask) pool_task = NULL;
              UpdateState *pool_state;

              pool_state = update_state_new (self, state->offset_table, entry, NULL);
              pool_task = g_task_new (self,
                                      state->cancellable,
                                      pool_func_cb,
                                      pool_state);

              /* We free the state if needed in pool_func_cb. This way,
               * we can keep the last ref alive for the following operations
               */
              g_task_set_task_data (pool_task, pool_state, NULL);
              g_task_set_source_tag (pool_task, update_async_cb1);

              self->pool_count++;
              ide_thread_pool_push_task (IDE_THREAD_POOL_INDEXER, pool_task, pool_func);
            }
        }

      self->pool_all_pushed = TRUE;
    }
  else
    {
      /* We have either no changes or an error */
      process_result (self, update_result, error);
      process_queue (self);
    }
}

static void
process_queue (IdeGiIndex *self)
{
  IdeTask *next_update_task;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_MAIN_THREAD ());

  if (!self->is_updating &&
      NULL != (next_update_task = g_queue_peek_head (self->update_queue)))
    {
      UpdateState *state = ide_task_get_task_data (next_update_task);
      g_autoptr(IdeTask) start_task = NULL;

      g_assert (state != NULL);

      start_update_index = g_get_monotonic_time ();

      self->is_updating = TRUE;
      start_task = ide_task_new (self,
                                 state->cancellable,
                                 update_async_cb1,
                                 state);
      ide_task_set_task_data (start_task, state, NULL);
      ide_task_run_in_thread (start_task, ide_gi_index_start_worker);
    }
}

static void
get_gir_directories_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(IdeTask) update_task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;
  UpdateState *state;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (update_task));
  g_assert (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  state = ide_task_get_task_data (update_task);
  g_assert (state != NULL);

  if (NULL == (state->gir_paths = ide_gi_index_get_gir_directories_finish (self, result, &error)))
    ide_task_return_error (update_task, g_steal_pointer (&error));
  else
    {
      g_queue_push_tail (self->update_queue, g_steal_pointer (&update_task));
      if (!self->is_updating)
        {
          g_autoptr(IdeTask) start_task = NULL;

          /* If there's no updates in process, the only element in the queue
           * is the one we just pushed.
           */
          g_assert (self->update_queue->length == 1);

          start_update_index = g_get_monotonic_time ();

          self->is_updating = TRUE;
          start_task = ide_task_new (self,
                                     state->cancellable,
                                     update_async_cb1,
                                     state);
          ide_task_set_task_data (start_task, state, NULL);
          ide_task_run_in_thread (start_task, ide_gi_index_start_worker);
        }
    }

  IDE_EXIT;
}

void
ide_gi_index_update_async (IdeGiIndex          *self,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  IdeTask *update_task;
  GPtrArray *offset_table;
  UpdateState *state;

  g_return_if_fail (IDE_IS_GI_INDEX (self));
  g_return_if_fail (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  offset_table = g_ptr_array_new_with_free_func (offset_table_entry_free);
  state = update_state_new (self, offset_table, NULL, cancellable);

  update_task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (update_task, state, (GDestroyNotify)update_state_free);

  ide_gi_index_get_gir_directories_async (self,
                                          cancellable,
                                          get_gir_directories_cb,
                                          update_task);

  IDE_EXIT;
}

gboolean
ide_gi_index_update_finish (IdeGiIndex    *self,
                            GAsyncResult  *result,
                            GError       **error)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

/**
 * ide_gi_index_get_runtime_id:
 * @self: #IdeGiIndex
 *
 * Get the runtime id of the #IdeGiIndex.
 *
 * Returns: (transfer none) a runtime id utf8 string
 */
const gchar *
ide_gi_index_get_runtime_id (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), NULL);

  return self->runtime_id;
}

/**
 * ide_gi_index_get_state:
 * @self: #IdeGiIndex
 *
 * Get the state of the #IdeGiIndex.
 *
 * Returns: #IdeGiIndexState
 */
IdeGiIndexState
ide_gi_index_get_state (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), IDE_GI_INDEX_STATE_NOT_INIT);
  g_return_val_if_fail (IDE_IS_MAIN_THREAD (), IDE_GI_INDEX_STATE_NOT_INIT);

  return self->state;
}

/**
 * ide_gi_index_get_cache_dir:
 * @self: #IdeGiIndex
 *
 * Get a #GFile of the indexes cached location.
 *
 * Returns: (transfer full): a #GFile.
 */
GFile *
ide_gi_index_get_cache_dir (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), NULL);

  return g_object_ref (self->cache_dir);
}

/**
 * ide_gi_index_get_repository:
 * @self: #IdeGiIndex
 *
 * Get the parent #IdeGiRepository of the #IdeGiIndex.
 *
 * Returns: (transfer none)
 */
IdeGiRepository *
ide_gi_index_get_repository (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), NULL);

  return self->repository;
}

/**
 * ide_gi_index_get_update_on_build:
 * @self: #IdeGiIndex
 *
 * Get the update-on-build state.
 *
 * Returns: %TRUE if set, %FALSE otherwise.
 */
gboolean
_ide_gi_index_get_update_on_build (IdeGiIndex *self)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (self), FALSE);

  return self->update_on_build;
}

/**
 * ide_gi_index_set_update_on_build:
 * @self: #IdeGiIndex
 *
 * Set the update-on-build state.
 *
 * Returns:
 */
void
_ide_gi_index_set_update_on_build (IdeGiIndex *self,
                                   gboolean    state)
{
  gboolean old_state = self->update_on_build;

  g_return_if_fail (IDE_IS_GI_INDEX (self));

  self->update_on_build = state;

  /* if it comes from the construct time update-on-build,
   * the async init is not fully done yet, so the possible
   * update is defered later.
   */
  if (state)
    {
      if (self->state != IDE_GI_INDEX_STATE_NOT_INIT && state != old_state)
        ide_gi_index_queue_update (self, NULL);
    }
  else
    {
      IdeTask *head;

      if ((head = g_queue_pop_head (self->update_queue)))
        {
          g_queue_clear (self->update_queue);
          g_queue_push_head (self->update_queue, head);
        }
    }
}

static void
queue_update_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (IDE_IS_TASK (result));
  g_assert (IDE_IS_MAIN_THREAD ());

  if (!ide_gi_index_update_finish (self, result, &error))
    {
      g_debug ("%s", error->message);
      return;
    }

  self->state = IDE_GI_INDEX_STATE_READY;
}

void
ide_gi_index_queue_update (IdeGiIndex   *self,
                           GCancellable *cancellable)
{
  g_return_if_fail (IDE_IS_GI_INDEX (self));
  g_return_if_fail (IDE_IS_MAIN_THREAD ());

  IDE_ENTRY;

  ide_gi_index_update_async (self, cancellable, queue_update_cb, NULL);

  IDE_EXIT;
}

static void
ide_gi_index_constructed (GObject *object)
{
  IdeGiIndex *self = IDE_GI_INDEX (object);
  IdeContext *context = ide_object_get_context (IDE_OBJECT (self));
  IdeBuildManager *build_manager = ide_context_get_build_manager (context);
  IdeRuntimeManager *rt_manager;
  IdeRuntime *runtime;

  G_OBJECT_CLASS (ide_gi_index_parent_class)->constructed (object);

  rt_manager = ide_context_get_runtime_manager (context);
  if ((runtime = ide_runtime_manager_get_runtime (rt_manager, self->runtime_id)) &&
      GBP_IS_FLATPAK_RUNTIME (runtime))
    {
      IdeBuildPipeline *pipeline;
      g_autofree gchar *staging_dir;
      g_autofree gchar *path;

      pipeline = ide_build_manager_get_pipeline (build_manager);
      staging_dir = gbp_flatpak_get_staging_dir (pipeline);
      path = g_build_filename (staging_dir, "files", "share", "gir-1.0", NULL);
      self->staging_dir = g_file_new_for_path (path);
    }
}

/**
 * ide_gi_index_new_async:
 * @repository: the #IdeGiRepository
 * @context: an #IdeContext
 * @cache_dir: the folder used to store cached files.
 * @runtime_id: the id of a #IdeRuntime
 * @update_on_build: update-on-build state
 * @cancellable: a #GCancellable
 * @callback: a #GAsyncReadyCallback
 * @user_data:
 *
 * Create a new #IdeGiIndex.
 * Call ide_gi_index_new_finish in the callback to get the result.
 *
 * Returns: none
 */
void
ide_gi_index_new_async (IdeGiRepository     *repository,
                        IdeContext          *context,
                        GFile               *cache_dir,
                        const gchar         *runtime_id,
                        gboolean             update_on_build,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GI_REPOSITORY (repository));
  g_return_if_fail (IDE_IS_CONTEXT (context));
  g_return_if_fail (G_IS_FILE (cache_dir));
  g_return_if_fail (runtime_id != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  /* TODO: protect against multiple requests of new index
   * with the same runtime on the same repository
   */
  g_async_initable_new_async (IDE_TYPE_GI_INDEX,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "repository", repository,
                              "context", context,
                              "cache-dir", cache_dir,
                              "runtime-id", runtime_id,
                              "update-on-build", update_on_build,
                              NULL);

  IDE_EXIT;
}

/**
 * ide_gi_index_new_finish:
 * @initable: the object received from the #GAsyncReadyCallback
 * @result: the #GAsyncResult received from the #GAsyncReadyCallback
 * @error: a #GError
 *
 * Returns: a #IdeGiIndex or %NULL with @error set.
 */
IdeGiIndex *
ide_gi_index_new_finish (GAsyncInitable  *initable,
                         GAsyncResult    *result,
                         GError         **error)
{
  GObject *obj;

  IDE_ENTRY;

  g_return_val_if_fail (G_IS_ASYNC_INITABLE (initable), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  obj = g_async_initable_new_finish (initable, result, error);
  if (obj != NULL)
    IDE_RETURN (IDE_GI_INDEX (obj));
  else
    IDE_RETURN (NULL);
};

static void
index_update_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)object;
  g_autoptr(IdeTask) task = (IdeTask *)user_data;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_INDEX (self));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (IDE_IS_TASK (task));
  g_assert (IDE_IS_MAIN_THREAD ());

  if (!ide_gi_index_update_finish (self, result, &error))
    ide_task_return_error (task, g_steal_pointer (&error));
  else
    {
      ide_task_return_boolean (task, TRUE);
      self->state = IDE_GI_INDEX_STATE_READY;
    }
}

static void
ide_gi_index_init_async (GAsyncInitable      *initable,
                         int                  io_priority,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  IdeGiIndex *self = (IdeGiIndex *)initable;
  g_autoptr(IdeTask) task = NULL;
  g_autoptr(GError) error = NULL;

  g_return_if_fail (G_IS_ASYNC_INITABLE (self));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_MAIN_THREAD ());

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_source_tag (task, ide_gi_index_init_async);

  /* Be sure our cache dir exist */
  if (!g_file_make_directory_with_parents (self->cache_dir, cancellable, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      ide_task_return_error (task, g_steal_pointer (&error));
    }
  else if (self->update_on_build)
    ide_gi_index_update_async (self,
                               cancellable,
                               index_update_cb,
                               g_steal_pointer (&task));
  else
    {
      ide_task_return_boolean (task, TRUE);
      self->state = IDE_GI_INDEX_STATE_READY;
    }
}

static gboolean
ide_gi_index_init_finish (GAsyncInitable  *initable,
                          GAsyncResult    *result,
                          GError         **error)
{
  g_return_val_if_fail (IDE_IS_GI_INDEX (initable), FALSE);
  g_return_val_if_fail (IDE_IS_TASK (result), FALSE);

  return ide_task_propagate_boolean (IDE_TASK (result), error);
}

static void
async_initable_init (GAsyncInitableIface *iface)
{
  iface->init_async = ide_gi_index_init_async;
  iface->init_finish = ide_gi_index_init_finish;
}

static void
ide_gi_index_dispose (GObject *object)
{
  IdeGiIndex *self = (IdeGiIndex *)object;

  g_clear_object (&self->repository);

  G_OBJECT_CLASS (ide_gi_index_parent_class)->dispose (object);
}

static void
ide_gi_index_finalize (GObject *object)
{
  IdeGiIndex *self = (IdeGiIndex *)object;

  g_clear_object (&self->file_builder);
  g_clear_object (&self->cache_dir);
  g_clear_object (&self->staging_dir);
  g_clear_object (&self->current_version);

  dzl_clear_pointer (&self->versions, g_hash_table_unref);
  dzl_clear_pointer (&self->files, g_hash_table_unref);
  dzl_clear_pointer (&self->runtime_id, g_free);
  dzl_clear_pointer (&self->update_queue, g_queue_free);
  dzl_clear_pointer (&self->remove_queue, g_queue_free);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (ide_gi_index_parent_class)->finalize (object);
}

static void
ide_gi_index_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  IdeGiIndex *self = IDE_GI_INDEX (object);

  switch (prop_id)
    {
    case PROP_UPDATE_ON_BUILD:
      g_value_set_boolean (value, self->update_on_build);
      break;

    case PROP_CACHE_DIR:
      g_value_set_object (value, self->cache_dir);
      break;

    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    case PROP_RUNTIME_ID:
      g_value_set_string (value, self->runtime_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_index_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  IdeGiIndex *self = IDE_GI_INDEX (object);

  switch (prop_id)
    {
    case PROP_UPDATE_ON_BUILD:
      _ide_gi_index_set_update_on_build (self, g_value_get_boolean (value));
      break;

    case PROP_CACHE_DIR:
      self->cache_dir = g_value_dup_object (value);
      break;

    case PROP_REPOSITORY:
      self->repository = g_value_dup_object (value);
      break;

    case PROP_RUNTIME_ID:
      self->runtime_id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_gi_index_class_init (IdeGiIndexClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = ide_gi_index_dispose;
  object_class->finalize = ide_gi_index_finalize;
  object_class->get_property = ide_gi_index_get_property;
  object_class->set_property = ide_gi_index_set_property;
  object_class->constructed = ide_gi_index_constructed;

  properties [PROP_UPDATE_ON_BUILD] =
      g_param_spec_boolean ("update-on-build",
                            "update-on-build",
                            "set the update-on-build feature",
                            TRUE,
                            (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  properties [PROP_CACHE_DIR] =
    g_param_spec_object ("cache-dir",
                         "Files cache directory",
                         "The directory where index and objects files are cached.",
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         "GI Repository",
                         "The repository parent of the index.",
                         IDE_TYPE_GI_REPOSITORY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties [PROP_RUNTIME_ID] =
    g_param_spec_string ("runtime-id",
                         "Runtime Id",
                         "The runtime identifier",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals [CURRENT_VERSION_CHANGED] = g_signal_new ("current-version-changed",
                                                    G_TYPE_FROM_CLASS (klass),
                                                    G_SIGNAL_RUN_LAST,
                                                    0,
                                                    NULL, NULL, NULL,
                                                    G_TYPE_NONE,
                                                    1,
                                                    IDE_TYPE_GI_VERSION);

  signals [VERSION_REMOVED] = g_signal_new ("version-removed",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_LAST,
                                            0,
                                            NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            G_TYPE_UINT);
}

static void
ide_gi_index_init (IdeGiIndex *self)
{
  self->files = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       g_free,
                                       (GDestroyNotify)ns_record_free);

  /* Currently only used for some additional checks */
  self->versions = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->file_builder = ide_gi_file_builder_new ();
  self->update_queue = g_queue_new ();
  self->remove_queue = g_queue_new ();

  self->state = IDE_GI_INDEX_STATE_NOT_INIT;
  g_mutex_init (&self->mutex);
}
