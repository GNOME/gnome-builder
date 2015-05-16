/* ide-ctags-index.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
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

#ifndef IDE_CTAGS_INDEX_H
#define IDE_CTAGS_INDEX_H

#include <gio/gio.h>

#include "ide-object.h"

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
  IdeCtagsIndexEntryKind  kind : 8;
  guint8                  padding[3];
} IdeCtagsIndexEntry;

IdeCtagsIndex            *ide_ctags_index_new           (GFile                *file);
void                      ide_ctags_index_load_async    (IdeCtagsIndex        *self,
                                                         GFile                *file,
                                                         GCancellable         *cancellable,
                                                         GAsyncReadyCallback   callback,
                                                         gpointer              user_data);
gboolean                  ide_ctags_index_load_finish   (IdeCtagsIndex        *index,
                                                         GAsyncResult         *result,
                                                         GError              **error);
GFile                    *ide_ctags_index_get_file      (IdeCtagsIndex         *self);
gsize                     ide_ctags_index_get_size      (IdeCtagsIndex         *self);
const IdeCtagsIndexEntry *ide_ctags_index_lookup        (IdeCtagsIndex         *self,
                                                         const gchar           *keyword,
                                                         gsize                 *length);
const IdeCtagsIndexEntry *ide_ctags_index_lookup_prefix (IdeCtagsIndex         *self,
                                                         const gchar           *keyword,
                                                         gsize                 *length);

gint ide_ctags_index_entry_compare (gconstpointer a,
                                    gconstpointer b);

G_END_DECLS

#endif /* IDE_CTAGS_INDEX_H */
