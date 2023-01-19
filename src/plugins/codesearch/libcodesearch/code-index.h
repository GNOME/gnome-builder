/*
 * code-index.h
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

#pragma once

#include <libdex.h>

G_BEGIN_DECLS

#define CODE_TYPE_INDEX         (code_index_get_type())
#define CODE_TYPE_INDEX_BUILDER (code_index_builder_get_type())

typedef struct _CodeIndex        CodeIndex;
typedef struct _CodeIndexBuilder CodeIndexBuilder;

typedef struct _CodeTrigram
{
  gunichar x;
  gunichar y;
  gunichar z;
} CodeTrigram;

typedef struct _CodeTrigramIter
{
  const char *pos;
  const char *end;
  CodeTrigram trigram;
} CodeTrigramIter;

typedef struct _CodeIndexIter
{
  CodeIndex *index;
  const guint8 *pos;
  const guint8 *end;
  guint last;
} CodeIndexIter;

typedef struct _CodeDocument
{
  const char *path;
  guint id;
} CodeDocument;

typedef struct _CodeIndexStat
{
  guint n_documents;
  guint n_documents_bytes;
  guint n_trigrams;
  guint n_trigrams_bytes;
  guint trigrams_data_bytes;
} CodeIndexStat;

/**
 * CodeIndexDocumentLoader:
 * @index: a #CodeIndex
 * @path: the path from the index
 * @user_data: closure data supplied to code_index_set_document_loader()
 *
 * This function is responsible for loading the contents of a document.
 *
 * It should return a #DexFuture that will either reject or resolve to
 * a #GBytes. Anything else is a programming error.
 *
 * The default implementation loads @path directly. If path is relative.
 * the current working directory is used.
 *
 * Returns: a #DexFuture that resolves to a #GBytes
 */
typedef DexFuture *(*CodeIndexDocumentLoader) (CodeIndex  *index,
                                               const char *path,
                                               gpointer    user_data);

GType             code_index_get_type                (void) G_GNUC_CONST;
GType             code_index_builder_get_type        (void) G_GNUC_CONST;
CodeIndexBuilder *code_index_builder_new             (void);
CodeIndexBuilder *code_index_builder_ref             (CodeIndexBuilder   *builder);
void              code_index_builder_unref           (CodeIndexBuilder   *builder);
void              code_index_builder_begin           (CodeIndexBuilder   *builder,
                                                      const char         *path);
void              code_index_builder_rollback        (CodeIndexBuilder   *builder);
void              code_index_builder_commit          (CodeIndexBuilder   *builder);
void              code_index_builder_add             (CodeIndexBuilder   *builder,
                                                      const CodeTrigram  *trigram);
guint             code_index_builder_get_n_documents (CodeIndexBuilder   *builder);
guint             code_index_builder_get_n_trigrams  (CodeIndexBuilder   *builder);
guint             code_index_builder_get_uncommitted (CodeIndexBuilder   *builder);
gboolean          code_index_builder_merge           (CodeIndexBuilder   *builder,
                                                      CodeIndex          *index);
DexFuture        *code_index_builder_write           (CodeIndexBuilder   *builder,
                                                      GOutputStream      *stream,
                                                      int                 io_priority);
DexFuture        *code_index_builder_write_file      (CodeIndexBuilder   *builder,
                                                      GFile              *file,
                                                      int                 io_priority);
DexFuture        *code_index_builder_write_filename  (CodeIndexBuilder   *builder,
                                                      const char         *filename,
                                                      int                 io_priority);
CodeIndex        *code_index_new                     (const char         *filename,
                                                      GError            **error);
CodeIndex        *code_index_ref                     (CodeIndex          *index);
void              code_index_unref                   (CodeIndex          *index);
void              code_index_stat                    (CodeIndex          *index,
                                                      CodeIndexStat      *stat);
const char       *code_index_get_document_path       (CodeIndex          *index,
                                                      guint               document_id);
void              code_index_set_document_loader     (CodeIndex               *index,
                                                      CodeIndexDocumentLoader  loader,
                                                      gpointer                 loader_data,
                                                      GDestroyNotify           loader_data_destroy);
DexFuture        *code_index_load_document           (CodeIndex          *index,
                                                      guint               document_id);
DexFuture        *code_index_load_document_path      (CodeIndex          *index,
                                                      const char         *path);
gboolean          code_index_iter_init               (CodeIndexIter      *iter,
                                                      CodeIndex          *index,
                                                      const CodeTrigram  *trigram);
gboolean          code_index_iter_next               (CodeIndexIter      *iter,
                                                      CodeDocument       *out_document);
gboolean          code_index_iter_seek_to            (CodeIndexIter      *iter,
                                                      guint               document_id);
guint             code_trigram_encode                (const CodeTrigram  *trigram);
CodeTrigram       code_trigram_decode                (guint               encoded);
void              code_trigram_iter_init             (CodeTrigramIter    *iter,
                                                      const char         *text,
                                                      goffset             len);
gboolean          code_trigram_iter_next             (CodeTrigramIter    *iter,
                                                      CodeTrigram        *trigram);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CodeIndex, code_index_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (CodeIndexBuilder, code_index_builder_unref)

G_END_DECLS
