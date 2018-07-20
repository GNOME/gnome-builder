/* ide-gi-file-builder.c
 *
 * Copyright © 2017 Sebastien Lafargue <slafargue@gnome.org>
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

#define G_LOG_DOMAIN "ide-gi-file-builder"

#include <dazzle.h>
#include <ide.h>
#include <string.h>

#include "ide-gi.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"

#include "ide-gi-index.h"
#include "ide-gi-namespace.h"
#include "ide-gi-parser.h"
#include "ide-gi-parser-object.h"
#include "ide-gi-parser-result.h"
#include "ide-gi-utils.h"

#include "ide-gi-file-builder.h"

struct _IdeGiFileBuilder
{
  GObject parent_instance;
};

typedef struct _TaskState
{
  GFile  *file;
  GFile  *write_path;
  gint    version_count;
} TaskState;

G_DEFINE_TYPE (IdeGiFileBuilder, ide_gi_file_builder, G_TYPE_OBJECT)

#define GIR_EXTENSION_LEN 4

static void
task_state_free (TaskState *state)
{
  g_assert (state != NULL);

  g_clear_object (&state->file);
  g_clear_object (&state->write_path);
}

static gchar *
get_dest_path (GFile *source_file,
               GFile *write_path,
               gint   version_count)
{
  g_autofree gchar *path = NULL;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *name = NULL;

  g_assert (G_IS_FILE (source_file));
  g_assert (G_IS_FILE (write_path));

  basename = g_file_get_basename (source_file);
  basename [strlen (basename) - GIR_EXTENSION_LEN] = '\0';

  path = g_file_get_path (write_path);
  name = g_strdup_printf ("%s@%d%s", basename, version_count, INDEX_NAMESPACE_EXTENSION);

  return g_build_filename (path, name, NULL);
}

static gboolean
ide_gi_file_builder_write_result (IdeGiFileBuilder   *self,
                                  IdeGiParserResult  *result,
                                  const gchar        *path,
                                  GError            **error)
{
  g_autoptr(GByteArray) ba = NULL;
  g_autoptr (GByteArray) strings = NULL;
  g_autoptr (GByteArray) doc_strings = NULL;
  g_autoptr (GByteArray) annotation_strings = NULL;
  g_autoptr(GArray) crossrefs = NULL;
  IdeGiNsIndexHeader ns_index_header = {0};
  gint32 offset = 0;
  guint elem_len;

  g_assert (IDE_IS_GI_FILE_BUILDER (self));
  g_assert (IDE_GI_PARSER_RESULT (result));

  ns_index_header.magic = NS_INDEX_HEADER_MAGIC;

  for (guint i = 0; i < IDE_GI_NS_TABLE_NB_TABLES; i++)
    {
      g_autoptr(GArray) table = ide_gi_parser_result_get_table (result, i);
      gsize size;

      if (table != NULL && table->len > 0)
        {
          ns_index_header.elements_tables[i] = offset;
          size = table->len * g_array_get_element_size (table);
          offset += size;
        }
      else
        {
          /* Flag the table as empty */
          ns_index_header.elements_tables[i] = -1;
        }
    }

  crossrefs = ide_gi_parser_result_get_crossrefs (result);
  ns_index_header.crossrefs = (crossrefs->len > 0) ? offset : -1;
  offset += crossrefs->len * sizeof (IdeGiCrossRef);

  if ((strings = ide_gi_parser_result_get_strings (result)) &&
      strings->len > 0)
    {
      ns_index_header.strings = offset;
      offset += strings->len;
    }

  if ((doc_strings = ide_gi_parser_result_get_doc_strings (result)) &&
      doc_strings->len > 0)
    {
      ns_index_header.doc_strings = offset;
      offset += doc_strings->len;
    }

  if ((annotation_strings = ide_gi_parser_result_get_annotation_strings (result)) &&
      annotation_strings->len > 0)
    {
      ns_index_header.annotation_strings = offset;
      offset += annotation_strings->len;
    }

  ba = g_byte_array_new ();
  g_byte_array_append (ba, (guint8 *)&ns_index_header, sizeof (IdeGiNsIndexHeader));
  for (guint i = 0; i < IDE_GI_NS_TABLE_NB_TABLES; i++)
    {
      if (ns_index_header.elements_tables[i] > -1)
        {
          g_autoptr(GArray) table = ide_gi_parser_result_get_table (result, i);
          elem_len = g_array_get_element_size (table);

          g_byte_array_append (ba, (guint8 *)table->data, table->len * elem_len);
        }
    }

  if (ns_index_header.crossrefs > -1)
    g_byte_array_append (ba, (guint8 *)crossrefs->data, crossrefs->len * sizeof (IdeGiCrossRef));

  if (ns_index_header.strings > 0)
    g_byte_array_append (ba, (guint8 *)strings->data, strings->len);

  if (ns_index_header.doc_strings > 0)
    g_byte_array_append (ba, (guint8 *)doc_strings->data, doc_strings->len);

  if (ns_index_header.annotation_strings > 0)
    g_byte_array_append (ba, (guint8 *)annotation_strings->data, annotation_strings->len);

  dzl_clear_pointer (&strings, g_byte_array_unref);
  dzl_clear_pointer (&doc_strings, g_byte_array_unref);
  dzl_clear_pointer (&annotation_strings, g_byte_array_unref);

  if (g_file_set_contents (path, (const gchar *)ba->data, ba->len, error))
    {
#ifdef IDE_ENABLE_TRACE
      if (ide_log_get_verbosity () >= 4)
        ide_gi_parser_result_print_stats (result);
#endif

      return TRUE;
    }

  return FALSE;
}

static GByteArray *
ide_gi_file_builder_create_namespace (IdeGiFileBuilder  *self,
                                      IdeGiParserResult *result)
{
  g_autoptr(GByteArray) header_strings = NULL;
  const IdeGiHeaderBlob *header;
  GByteArray *ba;
  guint64 pattern = 0;
  gsize padding;

  g_assert (IDE_IS_GI_FILE_BUILDER (self));

  ba = g_byte_array_new ();
  header = ide_gi_parser_result_get_header (result);
  header_strings = ide_gi_parser_result_get_header_strings (result);

  g_byte_array_append (ba, (guint8 *)header, sizeof (IdeGiHeaderBlob));
  g_byte_array_append (ba, header_strings->data, header_strings->len);
  padding = (8 - (ba->len & 7)) & 7;
  g_byte_array_append (ba, (guint8 *)&pattern, padding);

  return ba;
}

static gboolean
get_ns_version_from_includes (const gchar *includes,
                              const gchar *name,
                              guint8      *ns_major_version,
                              guint8      *ns_minor_version)
{
  g_autofree gchar *ns_name = NULL;
  const gchar *ptr = name;
  gsize len;
  guint16 major_version = 0;
  guint16 minor_version = 0;
  gboolean ret;

  g_assert (!dzl_str_empty0 (name));

  if (NULL == (ptr = strchr (name, '.')))
    return FALSE;

  len = ptr - name;
  ns_name = g_strndup (name, len + 1);
  ns_name[len] = ':';

  if (NULL == (ptr = strstr (includes, ns_name)))
    return FALSE;

  ptr += strlen (ns_name);
  ret = ide_gi_utils_parse_version (ptr, &major_version, &minor_version, NULL);

  *ns_major_version = (guint8)major_version;
  *ns_minor_version = (guint8)minor_version;
  return ret;
}

static void
resolve_local_crossrefs (IdeGiFileBuilder  *self,
                         IdeGiParserResult *result)
{
  g_autoptr(IdeGiRadixTreeBuilder) ro_tree = NULL;
  g_autoptr(GArray) crossrefs = NULL;
  g_autoptr (GByteArray) strings = NULL;
  const gchar *includes;
  const IdeGiHeaderBlob *ns_header;
  guint8 local_ns_major_version;
  guint8 local_ns_minor_version;

  ro_tree = ide_gi_parser_result_get_object_index_builder (result);
  if (NULL == (strings = ide_gi_parser_result_get_strings (result)) ||
      NULL == (crossrefs = ide_gi_parser_result_get_crossrefs (result)))
    return;

  includes = ide_gi_parser_result_get_includes (result);
  ns_header = ide_gi_parser_result_get_header (result);
  local_ns_major_version = ns_header->major_version;
  local_ns_minor_version = ns_header->minor_version;

  for (guint i =0; i < crossrefs->len; i++)
    {
      IdeGiCrossRef *crossref = &g_array_index (crossrefs, IdeGiCrossRef, i);
      IdeGiRadixTreeNode *node;
      RoTreePayload *payload;
      guint nb_rot_payloads;
      const gchar *qname;

      qname = (const gchar *)strings->data + crossref->qname;
      if (crossref->is_local)
        {
          /* Get the name part of the qname */
          const gchar *name = strchr (qname, '.');
          g_assert (name != NULL);
          name++;

          if (NULL == (node = ide_gi_radix_tree_builder_lookup (ro_tree, name)))
            {
              crossref->is_resolved = FALSE;
              IDE_TRACE_MSG ("Unresolved local crossref: %s (%d.%d)",
                             qname,
                             local_ns_major_version,
                             local_ns_minor_version);
              continue;
            }

          payload = (RoTreePayload *)node->payloads;
          nb_rot_payloads = node->nb_payloads / RO_TREE_PAYLOAD_N64_SIZE;

          for (guint j = 0; j < nb_rot_payloads; j++, payload++)
            {
              /* TODO: get only the registered type */
              IDE_TRACE_MSG ("Resolved local crossref: %s (%d.%d)",
                             qname,
                             local_ns_major_version,
                             local_ns_minor_version);

              crossref->is_resolved = TRUE;
              crossref->type = payload->type;
              crossref->offset = payload->offset;
              crossref->ns_major_version = local_ns_major_version;
              crossref->ns_minor_version = local_ns_minor_version;
              break;
            }
        }
      else
        {
          guint8 ns_major_version;
          guint8 ns_minor_version;

          if (get_ns_version_from_includes (includes, qname, &ns_major_version, &ns_minor_version))
            {
              crossref->is_resolved = TRUE;
              crossref->ns_major_version = ns_major_version;
              crossref->ns_minor_version = ns_minor_version;

              IDE_TRACE_MSG ("Resolved distant crossref version:%s (%d.%d)",
                             qname,
                             ns_major_version,
                             ns_minor_version);
            }
          else
            IDE_TRACE_MSG ("Unresolved distant crossref version :%s", qname);
        }
    }
}

static IdeGiFileBuilderResult *
generate (IdeGiFileBuilder  *self,
          GFile             *file,
          GFile             *write_path,
          gint               version_count,
          GCancellable      *cancellable,
          GError           **error)
{
  g_autoptr(IdeGiParser) parser = NULL;
  g_autoptr(IdeGiPool) pool = NULL;
  g_autoptr(IdeGiParserResult) result = NULL;
  g_autoptr(GByteArray) ns_ba = NULL;
  g_autoptr(IdeGiRadixTreeBuilder) ro_tree = NULL;
  g_autoptr(GArray) global_index = NULL;
  g_autofree gchar *path = NULL;
  const gchar *ns;
  const gchar *symbol_prefixes;
  const gchar *identifier_prefixes;
  IdeGiFileBuilderResult *ret;

  g_assert (IDE_IS_GI_FILE_BUILDER (self));
  g_assert (G_IS_FILE (file));
  g_assert (G_IS_FILE (write_path));
  g_assert (G_IS_CANCELLABLE (cancellable) || cancellable == NULL);

  parser = ide_gi_parser_new ();
  pool = ide_gi_pool_new (FALSE);
  ide_gi_parser_set_pool (parser, pool);

  if (NULL == (result = ide_gi_parser_parse_file (parser, file, cancellable, error)))
    return NULL;

  /* We modify in-place the result crossrefs array */
  resolve_local_crossrefs (self, result);

  path = get_dest_path (file, write_path, version_count);
  if (!ide_gi_file_builder_write_result (self, result, path, error))
    return NULL;

  ns_ba = ide_gi_file_builder_create_namespace (self, result);
  ro_tree = ide_gi_parser_result_get_object_index_builder (result);
  global_index = ide_gi_parser_result_get_global_index (result);

  ns = ide_gi_parser_result_get_namespace (result);
  symbol_prefixes = ide_gi_parser_result_get_c_symbol_prefixes (result);
  identifier_prefixes = ide_gi_parser_result_get_c_identifier_prefixes (result);

  ret = ide_gi_file_builder_result_new (ns_ba,
                                        ro_tree,
                                        global_index,
                                        ns,
                                        symbol_prefixes,
                                        identifier_prefixes);

  return ret;
}

static void
ide_gi_file_builder_worker (IdeTask      *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  IdeGiFileBuilder *self = (IdeGiFileBuilder *)source_object;
  TaskState *state = (TaskState *)task_data;
  IdeGiFileBuilderResult *result;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GI_FILE_BUILDER (self));
  g_assert (IDE_IS_TASK (task));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if ((result = generate (self,
                          state->file,
                          state->write_path,
                          state->version_count,
                          cancellable,
                          &error)))
    {
      ide_task_return_pointer (task, result, (GDestroyNotify)ide_gi_file_builder_result_unref);
    }
  else
    {
      ide_task_return_error (task, g_steal_pointer (&error));
    }
}

void
ide_gi_file_builder_generate_async (IdeGiFileBuilder    *self,
                                    GFile               *file,
                                    GFile               *write_path,
                                    gint                 version_count,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(IdeTask) task = NULL;
  TaskState *state;

  g_return_if_fail (IDE_IS_GI_FILE_BUILDER (self));
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE (write_path));

  state = g_slice_new0 (TaskState);
  state->file = g_object_ref (file);
  state->write_path = g_object_ref (write_path);
  state->version_count = version_count;

  task = ide_task_new (self, cancellable, callback, user_data);
  ide_task_set_task_data (task, state, task_state_free);

  ide_task_run_in_thread (task, ide_gi_file_builder_worker);
}

IdeGiFileBuilderResult *
ide_gi_file_builder_generate_finish (IdeGiFileBuilder  *self,
                                     GAsyncResult      *result,
                                     GError           **error)
{
  g_return_val_if_fail (IDE_IS_GI_FILE_BUILDER (self), NULL);
  g_return_val_if_fail (IDE_IS_TASK (result), NULL);

  return ide_task_propagate_pointer (IDE_TASK (result), error);
}

IdeGiFileBuilderResult *
ide_gi_file_builder_generate (IdeGiFileBuilder  *self,
                              GFile             *file,
                              GFile             *write_path,
                              gint              version_count,
                              GError           **error)
{
  g_return_val_if_fail (IDE_IS_GI_FILE_BUILDER (self), NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_FILE (write_path), NULL);

  return generate (self, file, write_path, version_count, NULL, error);
}

IdeGiFileBuilder *
ide_gi_file_builder_new (void)
{
  return g_object_new (IDE_TYPE_GI_FILE_BUILDER, NULL);
}

static void
ide_gi_file_builder_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_gi_file_builder_parent_class)->finalize (object);
}

static void
ide_gi_file_builder_class_init (IdeGiFileBuilderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_gi_file_builder_finalize;
}

static void
ide_gi_file_builder_init (IdeGiFileBuilder *self)
{
}
