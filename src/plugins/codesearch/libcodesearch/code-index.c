/*
 * code-index.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include "code-index.h"
#include "code-sparse-set.h"

#define CODE_INDEX_MAGIC     {0xC,0x0,0xD,0xE}
#define CODE_INDEX_ALIGNMENT 8

G_DEFINE_BOXED_TYPE (CodeIndex, code_index,
                     code_index_ref, code_index_unref)
G_DEFINE_BOXED_TYPE (CodeIndexBuilder, code_index_builder,
                     code_index_builder_ref, code_index_builder_unref)

static inline void
write_uint (GByteArray *bytes,
            guint       value)
{
  do
    {
      guint8 b = ((value > 0x7F) << 7) | (value & 0x7F);
      value >>= 7;
      g_byte_array_append (bytes, &b, 1);
    }
  while (value > 0);
}

static gboolean
_code_trigram_iter_next_char (CodeTrigramIter *iter,
                              gunichar        *ch)
{
  if (iter->pos >= iter->end)
    return FALSE;

  /* Since we're reading files they may not be in modified UTF-8 format.
   * If they're in regular UTF-8 there could be embedded Nil bytes. Handle
   * those specifically because g_utf8_*() will not.
   */

  if G_UNLIKELY (iter->pos[0] == 0)
    {
      *ch = 0;
      iter->pos++;
      return TRUE;
    }

  *ch = g_utf8_get_char_validated (iter->pos, iter->end - iter->pos);

  if (*ch == (gunichar)-1 || *ch == (gunichar)-2)
    {
      iter->pos = iter->end;
      return FALSE;
    }

  iter->pos = g_utf8_next_char (iter->pos);

  return TRUE;
}

void
code_trigram_iter_init (CodeTrigramIter *iter,
                        const char      *text,
                        goffset          len)
{
  if (len < 0)
    len = strlen (text);

  iter->pos = text;
  iter->end = text + len;

  if (_code_trigram_iter_next_char (iter, &iter->trigram.y))
    _code_trigram_iter_next_char (iter, &iter->trigram.z);
}

gboolean
code_trigram_iter_next (CodeTrigramIter *iter,
                        CodeTrigram     *trigram)
{
  if G_UNLIKELY (iter->pos >= iter->end)
    return FALSE;

  iter->trigram.x = iter->trigram.y;
  iter->trigram.y = iter->trigram.z;

  if (!_code_trigram_iter_next_char (iter, &iter->trigram.z))
    return FALSE;

  trigram->x = !g_unichar_isspace (iter->trigram.x) ? iter->trigram.x : '_';
  trigram->y = !g_unichar_isspace (iter->trigram.y) ? iter->trigram.y : '_';
  trigram->z = !g_unichar_isspace (iter->trigram.z) ? iter->trigram.z : '_';

  return TRUE;
}

guint
code_trigram_encode (const CodeTrigram *trigram)
{
  return ((trigram->x & 0xFF) << 16) |
         ((trigram->y & 0xFF) <<  8) |
         ((trigram->z & 0xFF) <<  0);
}

CodeTrigram
code_trigram_decode (guint encoded)
{
  return (CodeTrigram) {
    .x = ((encoded & 0xFF0000) >> 16),
    .y = ((encoded & 0xFF00) >>  8),
    .z = (encoded & 0xFF),
  };
}

typedef struct _CodeIndexBuilderTrigrams
{
  GByteArray *buffer;
  guint32     id;
  guint32     position;
  guint       last_document_id;
} CodeIndexBuilderTrigrams;

typedef struct _CodeIndexBuilderDocument
{
  const char *path;
  char       *collate;
  guint32     id;
  guint32     position;
} CodeIndexBuilderDocument;

typedef struct _CodeIndexHeader
{
  guint8  magic[4];
  guint32 n_documents;
  guint32 documents;
  guint32 n_documents_bytes;
  guint32 n_trigrams;
  guint32 trigrams;
  guint32 n_trigrams_bytes;
  guint32 trigrams_data;
  guint32 trigrams_data_bytes;
} CodeIndexHeader;

struct _CodeIndexBuilder
{
  GStringChunk  *paths;
  CodeSparseSet  trigrams_set;
  CodeSparseSet  uncommitted_set;
  GArray        *documents;
  GArray        *trigrams;
  const char    *current_path;
};

static void
code_index_builder_document_clear (gpointer data)
{
  CodeIndexBuilderDocument *document = data;

  g_clear_pointer (&document->collate, g_free);
  document->path = 0;
  document->position = 0;
  document->id = 0;
}

static void
code_index_builder_trigrams_clear (gpointer data)
{
  CodeIndexBuilderTrigrams *trigrams = data;

  g_clear_pointer (&trigrams->buffer, g_byte_array_unref);
  trigrams->last_document_id = 0;
}

static void
code_index_builder_finalize (CodeIndexBuilder *builder)
{
  code_sparse_set_clear (&builder->trigrams_set);
  code_sparse_set_clear (&builder->uncommitted_set);
  g_clear_pointer (&builder->paths, g_string_chunk_free);
  g_clear_pointer (&builder->documents, g_array_unref);
  g_clear_pointer (&builder->trigrams, g_array_unref);
}

void
code_index_builder_unref (CodeIndexBuilder *builder)
{
  g_atomic_rc_box_release_full (builder, (GDestroyNotify)code_index_builder_finalize);
}

CodeIndexBuilder *
code_index_builder_new (void)
{
  static const CodeIndexBuilderDocument zero = {0};
  CodeIndexBuilder *builder;

  builder = g_atomic_rc_box_new0 (CodeIndexBuilder);
  builder->documents = g_array_new (FALSE, FALSE, sizeof (CodeIndexBuilderDocument));
  builder->trigrams = g_array_new (FALSE, FALSE, sizeof (CodeIndexBuilderTrigrams));
  builder->paths = g_string_chunk_new (4096*4);
  code_sparse_set_init (&builder->trigrams_set, 1<<24);
  code_sparse_set_init (&builder->uncommitted_set, 1<<24);

  g_array_set_clear_func (builder->documents, code_index_builder_document_clear);
  g_array_set_clear_func (builder->trigrams, code_index_builder_trigrams_clear);

  g_array_append_val (builder->documents, zero);

  return builder;
}

CodeIndexBuilder *
code_index_builder_ref (CodeIndexBuilder *builder)
{
  return g_atomic_rc_box_acquire (builder);
}

guint
code_index_builder_get_n_documents (CodeIndexBuilder *builder)
{
  return builder->documents->len;
}

guint
code_index_builder_get_n_trigrams (CodeIndexBuilder *builder)
{
  return builder->trigrams_set.len;
}

guint
code_index_builder_get_uncommitted (CodeIndexBuilder *builder)
{
  return builder->uncommitted_set.len;
}

void
code_index_builder_add (CodeIndexBuilder  *builder,
                        const CodeTrigram *trigram)
{
  guint trigram_id = code_trigram_encode (trigram);

  code_sparse_set_add (&builder->uncommitted_set, trigram_id);
}

void
code_index_builder_begin (CodeIndexBuilder *builder,
                          const char       *path)
{
  builder->current_path = g_string_chunk_insert_const (builder->paths, path);
}

void
code_index_builder_commit (CodeIndexBuilder *builder)
{
  CodeIndexBuilderDocument document = {
    .path = builder->current_path,
    .collate = g_utf8_collate_key_for_filename (builder->current_path, -1),
    .id = builder->documents->len,
    .position = 0,
  };

  g_array_append_val (builder->documents, document);

  for (guint i = 0; i < builder->uncommitted_set.len; i++)
    {
      CodeIndexBuilderTrigrams *trigrams;
      guint trigram_id = builder->uncommitted_set.dense[i].value;
      guint trigrams_index;

      if (!code_sparse_set_get (&builder->trigrams_set, trigram_id, &trigrams_index))
        {
          CodeIndexBuilderTrigrams t;

          t.buffer = g_byte_array_new ();
          t.id = trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = builder->trigrams->len;
          code_sparse_set_add_with_data (&builder->trigrams_set, trigram_id, trigrams_index);
          g_array_append_val (builder->trigrams, t);
        }

      trigrams = &g_array_index (builder->trigrams, CodeIndexBuilderTrigrams, trigrams_index);
      write_uint (trigrams->buffer, document.id - trigrams->last_document_id);
      trigrams->last_document_id = document.id;
    }

  builder->current_path = NULL;

  code_sparse_set_reset (&builder->uncommitted_set);
}

void
code_index_builder_rollback (CodeIndexBuilder *builder)
{
  builder->current_path = NULL;

  code_sparse_set_reset (&builder->uncommitted_set);
}

static int
sort_by_trigram (gconstpointer a,
                 gconstpointer b)
{
  const CodeIndexBuilderTrigrams *atri = a;
  const CodeIndexBuilderTrigrams *btri = b;

  if (atri->id < btri->id)
    return -1;
  else if (atri->id > btri->id)
    return 1;
  else
    return 0;
}

static guint
realign (GByteArray *buffer)
{
  static const guint8 zero[CODE_INDEX_ALIGNMENT] = {0};
  gsize rem = buffer->len % CODE_INDEX_ALIGNMENT;

  if (rem > 0)
    g_byte_array_append (buffer, zero, CODE_INDEX_ALIGNMENT-rem);
  return buffer->len;
}

DexFuture *
code_index_builder_write (CodeIndexBuilder *builder,
                          GOutputStream    *stream,
                          int               io_priority)
{
  GByteArray *buffer;
  DexFuture *future;
  GBytes *bytes;
  guint begin_documents_pos;

  CodeIndexHeader header = {
    .magic = CODE_INDEX_MAGIC,
    .n_documents = builder->documents->len,
    .n_trigrams = builder->trigrams->len,
  };

  g_array_sort (builder->trigrams, sort_by_trigram);

  buffer = g_byte_array_new ();
  g_byte_array_append (buffer, (const guint8 *)&header, sizeof header);

  begin_documents_pos = realign (buffer);
  for (guint i = 1; i < builder->documents->len; i++)
    {
      CodeIndexBuilderDocument *document = &g_array_index (builder->documents, CodeIndexBuilderDocument, i);

      document->position = buffer->len;
      g_byte_array_append (buffer,
                           (const guint8 *)document->path,
                           strlen (document->path) + 1);
    }

  header.documents = realign (buffer);
  for (guint i = 0; i < builder->documents->len; i++)
    {
      CodeIndexBuilderDocument *document = &g_array_index (builder->documents, CodeIndexBuilderDocument, i);

      g_byte_array_append (buffer, (const guint8 *)&document->position, sizeof document->position);
    }
  header.n_documents_bytes = buffer->len - begin_documents_pos;

  header.trigrams_data = realign (buffer);
  for (guint i = 0; i < builder->trigrams->len; i++)
    {
      CodeIndexBuilderTrigrams *trigrams = &g_array_index (builder->trigrams, CodeIndexBuilderTrigrams, i);

      g_assert (trigrams->buffer->len > 0);

      trigrams->position = buffer->len;
      g_byte_array_append (buffer,
                           (const guint8 *)trigrams->buffer->data,
                           trigrams->buffer->len);
    }
  header.trigrams_data_bytes = buffer->len - header.trigrams_data;

  header.trigrams = realign (buffer);
  for (guint i = 0; i < builder->trigrams->len; i++)
    {
      CodeIndexBuilderTrigrams *trigrams = &g_array_index (builder->trigrams, CodeIndexBuilderTrigrams, i);
      guint32 end = trigrams->position + trigrams->buffer->len;

      g_byte_array_append (buffer, (const guint8 *)&trigrams->id, sizeof trigrams->id);
      g_byte_array_append (buffer, (const guint8 *)&trigrams->position, sizeof trigrams->position);
      g_byte_array_append (buffer, (const guint8 *)&end, sizeof end);
    }
  header.n_trigrams_bytes = buffer->len - header.trigrams;

  memcpy (buffer->data, &header, sizeof header);

  bytes = g_byte_array_free_to_bytes (buffer);
  future = dex_output_stream_write_bytes (stream, bytes, io_priority);
  g_bytes_unref (bytes);

  return future;
}

static DexFuture *
code_index_builder_write_file_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  g_autoptr(GOutputStream) stream = dex_await_object (dex_ref (completed), NULL);
  CodeIndexBuilder *builder = user_data;

  return code_index_builder_write (builder, stream, 0);
}

DexFuture *
code_index_builder_write_file (CodeIndexBuilder *builder,
                               GFile            *file,
                               int               io_priority)
{
  DexFuture *future;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  future = dex_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, io_priority);
  future = dex_future_then (future,
                            code_index_builder_write_file_cb,
                            code_index_builder_ref (builder),
                            (GDestroyNotify)code_index_builder_unref);

  return future;
}

DexFuture *
code_index_builder_write_filename (CodeIndexBuilder *builder,
                                   const char       *filename,
                                   int               io_priority)
{
  g_autoptr(GFile) file = NULL;

  g_return_val_if_fail (builder != NULL, NULL);
  g_return_val_if_fail (filename != NULL, NULL);

  file = g_file_new_for_path (filename);

  return code_index_builder_write_file (builder, file, io_priority);
}

typedef struct _CodeIndexTrigram
{
  guint32 trigram_id;
  guint32 position;
  guint32 end;
} CodeIndexTrigram;

struct _CodeIndex
{
  GMappedFile             *map;
  CodeIndexTrigram        *trigrams;
  guint32                 *documents;
  CodeIndexDocumentLoader  loader;
  gpointer                 loader_data;
  GDestroyNotify           loader_data_destroy;
  CodeIndexHeader          header;
};

#define SIZE_OVERFLOWS(a,b) (G_UNLIKELY ((b) > 0 && (a) > G_MAXSIZE / (b)))

static inline gboolean
has_space_for (gsize length,
               gsize offset,
               gsize n_items,
               gsize item_size)
{
  gsize avail;
  gsize needed;

  if (offset >= length)
    return FALSE;

  avail = length - offset;

  if (SIZE_OVERFLOWS (n_items, item_size))
    return FALSE;

  needed = n_items * item_size;

  return needed <= avail;
}

static DexFuture *
code_index_default_loader (CodeIndex  *index,
                           const char *path,
                           gpointer    user_data)
{
  g_autoptr(GMappedFile) mapped = NULL;
  g_autoptr(GError) error = NULL;

  if (!(mapped = g_mapped_file_new (path, FALSE, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_boxed (G_TYPE_BYTES,
                                    g_mapped_file_get_bytes (mapped));
}


CodeIndex *
code_index_new (const char  *filename,
                GError     **error)
{
  static const guint8 magic[] = CODE_INDEX_MAGIC;
  CodeIndex *index;
  GMappedFile *mf;
  const char *data;
  gsize len;

  if (!(mf = g_mapped_file_new (filename, FALSE, error)))
    return NULL;

  if (g_mapped_file_get_length (mf) < sizeof index->header)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Not a codeindex");
      g_mapped_file_unref (mf);
      return NULL;
    }

  data = g_mapped_file_get_contents (mf);
  len = g_mapped_file_get_length (mf);

  index = g_atomic_rc_box_new0 (CodeIndex);

  memcpy (&index->header, data, sizeof index->header);
  index->map = mf;

  index->loader = code_index_default_loader;
  index->loader_data = NULL;
  index->loader_data_destroy = NULL;

  if (memcmp (&index->header.magic, magic, sizeof magic) != 0 ||
      !has_space_for (len, index->header.trigrams, index->header.n_trigrams, 12) ||
      !has_space_for (len, index->header.documents, index->header.n_documents, 4) ||
      index->header.trigrams % CODE_INDEX_ALIGNMENT != 0 ||
      index->header.documents % CODE_INDEX_ALIGNMENT != 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Not a codeindex");
      code_index_unref (index);
      return NULL;
    }

  index->trigrams = (CodeIndexTrigram *)(gpointer)&data[index->header.trigrams];
  index->documents = (guint32 *)(gpointer)&data[index->header.documents];

  return index;
}

CodeIndex *
code_index_ref (CodeIndex *index)
{
  return g_atomic_rc_box_acquire (index);
}

static void
code_index_finalize (CodeIndex *index)
{
  g_clear_pointer (&index->map, g_mapped_file_unref);
}

void
code_index_unref (CodeIndex *index)
{
  return g_atomic_rc_box_release_full (index, (GDestroyNotify)code_index_finalize);
}

const char *
code_index_get_document_path (CodeIndex *index,
                              guint      document_id)
{
  const char *data;
  const char *path;
  const char *end;
  const char *iter;
  guint position;
  gsize len;

  g_return_val_if_fail (document_id > 0, NULL);
  g_return_val_if_fail (document_id < index->header.n_documents, NULL);

  position = index->documents[document_id];
  data = g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  g_return_val_if_fail (position < len, NULL);

  end = &data[len];
  path = &data[position];

  for (iter = path; iter < end; iter++)
    {
      if (*iter == 0)
        return path;
    }

  g_return_val_if_reached (NULL);
}

static int
find_trigram_by_id_cmp (gconstpointer keyptr,
                        gconstpointer trigramptr)
{
  const guint *key = keyptr;
  const CodeIndexTrigram *trigram = trigramptr;

  if (*key < trigram->trigram_id)
    return -1;
  else if (*key > trigram->trigram_id)
    return 1;
  else
    return 0;
}

static const CodeIndexTrigram *
code_index_find_trigram_by_id (CodeIndex *index,
                               guint      trigram_id)
{
  return bsearch (&trigram_id,
                  index->trigrams,
                  index->header.n_trigrams,
                  sizeof *index->trigrams,
                  find_trigram_by_id_cmp);
}

static inline gboolean
code_index_iter_init_raw (CodeIndexIter          *iter,
                          CodeIndex              *index,
                          const guint8           *data,
                          gsize                   len,
                          const CodeIndexTrigram *trigrams)
{
  if (trigrams->position >= len || trigrams->end >= len || trigrams->end < trigrams->position)
    return FALSE;

  iter->index = index;
  iter->pos = &data[trigrams->position];
  iter->end = &data[trigrams->end];
  iter->last = 0;

  return TRUE;
}

gboolean
code_index_iter_init (CodeIndexIter     *iter,
                      CodeIndex         *index,
                      const CodeTrigram *trigram)
{
  const CodeIndexTrigram *trigrams;
  const guint8 *data;
  guint trigram_id;
  gsize len;

  trigram_id = code_trigram_encode (trigram);

  if (!(trigrams = code_index_find_trigram_by_id (index, trigram_id)))
    return FALSE;

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);
  if (trigrams->position >= len || trigrams->end >= len || trigrams->end < trigrams->position)
    return FALSE;

  return code_index_iter_init_raw (iter, index, data, len, trigrams);
}

static gboolean
code_index_iter_next_id (CodeIndexIter *iter,
                         guint         *out_document_id)
{
  guint u = 0, o = 0;
  guint8 b;

  do
    {
      if (iter->pos >= iter->end || o > 28)
        return FALSE;

      b = *iter->pos;;
      u |= ((guint32)(b & 0x7F) << o);
      o += 7;

      iter->pos++;
    }
  while ((b & 0x80) != 0);

  iter->last += u;

  *out_document_id = iter->last;

  return TRUE;
}

gboolean
code_index_iter_next (CodeIndexIter *iter,
                      CodeDocument  *out_document)
{
  guint document_id;

  if (code_index_iter_next_id (iter, &document_id))
    {
      out_document->id = document_id;
      out_document->path = code_index_get_document_path (iter->index, document_id);

      return out_document->path != NULL;
    }

  return FALSE;
}

gboolean
code_index_iter_seek_to (CodeIndexIter *iter,
                         guint          document_id)
{
  guint ignored;

  do
    {
      if (iter->last >= document_id)
        break;
    }
  while (code_index_iter_next_id (iter, &ignored));

  return iter->last == document_id;
}

gboolean
code_index_builder_merge (CodeIndexBuilder *builder,
                          CodeIndex        *index)
{
  guint document_id_offset;
  const guint8 *data;
  gsize len;

  g_assert (builder->documents != NULL);
  g_assert (builder->documents->len >= 1);

  /* get our starting document id */
  document_id_offset = builder->documents->len - 1;

  /* Make sure enough space for document ids */
  if (G_MAXUINT - document_id_offset < index->header.n_documents)
    return FALSE;

  /* Add all of the documents to the index */
  for (guint i = 1; i < index->header.n_documents; i++)
    {
      const char *path = code_index_get_document_path (index, i);
      CodeIndexBuilderDocument document = {
        .path = g_string_chunk_insert_const (builder->paths, path),
        .collate = g_utf8_collate_key_for_filename (path, -1),
        .id = builder->documents->len,
        .position = 0,
      };

      g_array_append_val (builder->documents, document);
    }

  data = (const guint8 *)g_mapped_file_get_contents (index->map);
  len = g_mapped_file_get_length (index->map);

  /* Get the array of trigrams so we can iterate them */
  for (guint i = 0; i < index->header.n_trigrams; i++)
    {
      const CodeIndexTrigram *trigrams = &index->trigrams[i];
      CodeIndexBuilderTrigrams *builder_trigrams;
      CodeIndexIter iter;
      guint trigrams_index;
      guint id;

      if (!code_index_iter_init_raw (&iter, index, data, len, trigrams))
        continue;

      if (!code_sparse_set_get (&builder->trigrams_set, trigrams->trigram_id, &trigrams_index))
        {
          CodeIndexBuilderTrigrams t;

          t.buffer = g_byte_array_new ();
          t.id = trigrams->trigram_id;
          t.last_document_id = 0;
          t.position = 0;

          trigrams_index = builder->trigrams->len;
          code_sparse_set_add_with_data (&builder->trigrams_set, trigrams->trigram_id, trigrams_index);
          g_array_append_val (builder->trigrams, t);
        }

      builder_trigrams = &g_array_index (builder->trigrams, CodeIndexBuilderTrigrams, trigrams_index);

      while (code_index_iter_next_id (&iter, &id))
        {
          id += document_id_offset;
          write_uint (builder_trigrams->buffer, id - builder_trigrams->last_document_id);
          builder_trigrams->last_document_id = id;
        }

    }

  return TRUE;
}

void
code_index_stat (CodeIndex     *index,
                 CodeIndexStat *stat)
{
  stat->n_documents = index->header.n_documents;
  stat->n_documents_bytes = index->header.n_documents_bytes;
  stat->n_trigrams = index->header.n_trigrams;
  stat->n_trigrams_bytes = index->header.n_trigrams_bytes;
  stat->trigrams_data_bytes = index->header.trigrams_data_bytes;
}

/**
 * code_index_set_document_loader:
 * @self: a #CodeIndex
 * @loader: (scope async) (nullable): a #CodeIndexDocumentLoader or %NULL for the default
 * @loader_data: closure data for @loader
 * @loader_data_destroy: destroy callback for @loader_data
 *
 * Sets the document loader for @index.
 *
 * This allows the query system to load the contents of the document using
 * an abstracted loader which might fetch the contents from another location
 * that specified within the index.
 *
 * This is useful when using relative paths to shrink the index size.
 *
 * It can also be useful when loading contents not in a file-system such as
 * indexed commits from Git.
 */
void
code_index_set_document_loader (CodeIndex               *index,
                                CodeIndexDocumentLoader  loader,
                                gpointer                 loader_data,
                                GDestroyNotify           loader_data_destroy)
{
  g_return_if_fail (index != NULL);

  if (index->loader_data_destroy)
    index->loader_data_destroy (index->loader_data);

  if (loader == NULL)
    {
      loader = code_index_default_loader;
      loader_data = NULL;
      loader_data_destroy = NULL;
    }

  index->loader = loader;
  index->loader_data = loader_data;
  index->loader_data_destroy = loader_data_destroy;
}

/**
 * code_index_load_document_path:
 * @index: a #CodeIndex
 * @path: a path for a document
 *
 * Loads the document expected at @path
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GBytes
 *   or rejects with an error.
 */
DexFuture *
code_index_load_document_path (CodeIndex  *index,
                               const char *path)
{
  g_return_val_if_fail (index != NULL, NULL);

  return index->loader (index, path, index->loader_data);
}

/**
 * code_index_load_document:
 * @index: a #CodeIndex
 * @document_id: the document identifier
 *
 * Finds the path of @document_id and loads it using
 * code_index_load_document_path().
 *
 * Returns: (transfer full): a #DexFuture that resolves to a #GBytes
 *   or rejects with an error.
 */
DexFuture *
code_index_load_document (CodeIndex *index,
                          guint      document_id)
{
  const char *path;

  g_return_val_if_fail (index != NULL, NULL);

  if (!(path = code_index_get_document_path (index, document_id)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_FOUND,
                                  "Failed to locate document %u.",
                                  document_id);

  return code_index_load_document_path (index, path);
}
