/* ide-ctags-index.h
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

#pragma once

#include <libide-core.h>
#include <libide-code.h>

G_BEGIN_DECLS

#define IDE_TYPE_CTAGS_INDEX (ide_ctags_index_get_type())

G_DECLARE_FINAL_TYPE (IdeCtagsIndex, ide_ctags_index, IDE, CTAGS_INDEX, IdeObject)

typedef enum
{
  IDE_CTAGS_INDEX_ENTRY_ANCHOR = 'a',
  IDE_CTAGS_INDEX_ENTRY_CLASS_NAME = 'c',
  IDE_CTAGS_INDEX_ENTRY_DEFINE = 'd',
  IDE_CTAGS_INDEX_ENTRY_ENUMERATOR = 'e',
  IDE_CTAGS_INDEX_ENTRY_FUNCTION = 'f',
  IDE_CTAGS_INDEX_ENTRY_FILE_NAME = 'F',
  IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME = 'g',
  IDE_CTAGS_INDEX_ENTRY_IMPORT = 'i',
  IDE_CTAGS_INDEX_ENTRY_MEMBER = 'm',
  IDE_CTAGS_INDEX_ENTRY_PROTOTYPE = 'p',
  IDE_CTAGS_INDEX_ENTRY_STRUCTURE = 's',
  IDE_CTAGS_INDEX_ENTRY_TYPEDEF = 't',
  IDE_CTAGS_INDEX_ENTRY_UNION = 'u',
  IDE_CTAGS_INDEX_ENTRY_VARIABLE = 'v',
} IdeCtagsIndexEntryKind;

typedef struct
{
  const gchar            *name;
  const gchar            *path;
  const gchar            *pattern;
  const gchar            *keyval;
  IdeCtagsIndexEntryKind  kind : 8;
  guint8                  padding[3];
} IdeCtagsIndexEntry;

/* This object is meant to be immutable after loading so that
 * it can be used from threads safely.
 */

IdeCtagsIndex            *ide_ctags_index_new           (GFile                    *file,
                                                         const gchar              *path_root,
                                                         guint64                   mtime);
void                      ide_ctags_index_load_async    (IdeCtagsIndex            *self,
                                                         GFile                    *file,
                                                         GCancellable             *cancellable,
                                                         GAsyncReadyCallback       callback,
                                                         gpointer                  user_data);
gboolean                  ide_ctags_index_load_finish   (IdeCtagsIndex            *index,
                                                         GAsyncResult             *result,
                                                         GError                  **error);
GPtrArray                *ide_ctags_index_find_with_path(IdeCtagsIndex           *self,
                                                         const gchar             *relative_path);
gchar                    *ide_ctags_index_resolve_path  (IdeCtagsIndex            *self,
                                                         const gchar              *path);
GFile                    *ide_ctags_index_get_file      (IdeCtagsIndex            *self);
gboolean                  ide_ctags_index_get_is_empty  (IdeCtagsIndex            *self);
gsize                     ide_ctags_index_get_size      (IdeCtagsIndex            *self);
const gchar              *ide_ctags_index_get_path_root (IdeCtagsIndex            *self);
const IdeCtagsIndexEntry *ide_ctags_index_lookup        (IdeCtagsIndex            *self,
                                                         const gchar              *keyword,
                                                         gsize                    *length);
const IdeCtagsIndexEntry *ide_ctags_index_lookup_prefix (IdeCtagsIndex            *self,
                                                         const gchar              *keyword,
                                                         gsize                    *length);
guint64                   ide_ctags_index_get_mtime     (IdeCtagsIndex            *self);
gint                      ide_ctags_index_entry_compare (gconstpointer             a,
                                                         gconstpointer             b);
IdeCtagsIndexEntry       *ide_ctags_index_entry_copy    (const IdeCtagsIndexEntry *entry);
void                      ide_ctags_index_entry_free    (IdeCtagsIndexEntry       *entry);

static inline IdeSymbolKind
ide_ctags_index_entry_kind_to_symbol_kind (IdeCtagsIndexEntryKind kind)
{
  switch (kind)
    {
    case IDE_CTAGS_INDEX_ENTRY_TYPEDEF:
    case IDE_CTAGS_INDEX_ENTRY_PROTOTYPE:
      /* bit of an impedenece mismatch */
    case IDE_CTAGS_INDEX_ENTRY_CLASS_NAME:
      return IDE_SYMBOL_KIND_CLASS;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATOR:
      return IDE_SYMBOL_KIND_ENUM;

    case IDE_CTAGS_INDEX_ENTRY_ENUMERATION_NAME:
      return IDE_SYMBOL_KIND_ENUM_VALUE;

    case IDE_CTAGS_INDEX_ENTRY_FUNCTION:
      return IDE_SYMBOL_KIND_FUNCTION;

    case IDE_CTAGS_INDEX_ENTRY_MEMBER:
      return IDE_SYMBOL_KIND_FIELD;

    case IDE_CTAGS_INDEX_ENTRY_STRUCTURE:
      return IDE_SYMBOL_KIND_STRUCT;

    case IDE_CTAGS_INDEX_ENTRY_UNION:
      return IDE_SYMBOL_KIND_UNION;

    case IDE_CTAGS_INDEX_ENTRY_VARIABLE:
      return IDE_SYMBOL_KIND_VARIABLE;

    case IDE_CTAGS_INDEX_ENTRY_IMPORT:
      return IDE_SYMBOL_KIND_PACKAGE;

    case IDE_CTAGS_INDEX_ENTRY_ANCHOR:
    case IDE_CTAGS_INDEX_ENTRY_DEFINE:
    case IDE_CTAGS_INDEX_ENTRY_FILE_NAME:
    default:
      return IDE_SYMBOL_KIND_NONE;
    }
}

G_END_DECLS
