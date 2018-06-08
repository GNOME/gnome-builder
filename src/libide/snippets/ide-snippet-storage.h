/* ide-snippet-storage.h
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <gio/gio.h>

#include "ide-types.h"
#include "ide-version-macros.h"

G_BEGIN_DECLS

#define IDE_TYPE_SNIPPET_STORAGE (ide_snippet_storage_get_type())

typedef struct
{
  const gchar *lang;
  const gchar *name;
  const gchar *desc;
  /*< private >*/
  const gchar *begin;
  goffset      len;
} IdeSnippetInfo;

typedef void (*IdeSnippetStorageForeach) (IdeSnippetStorage    *self,
                                          const IdeSnippetInfo *info,
                                          gpointer              user_data);

IDE_AVAILABLE_IN_3_30
G_DECLARE_FINAL_TYPE (IdeSnippetStorage, ide_snippet_storage, IDE, SNIPPET_STORAGE, GObject)

IDE_AVAILABLE_IN_3_30
IdeSnippetStorage *ide_snippet_storage_new         (void);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_storage_add         (IdeSnippetStorage         *self,
                                                    const gchar               *default_scope,
                                                    GBytes                    *bytes);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_storage_foreach     (IdeSnippetStorage         *self,
                                                    IdeSnippetStorageForeach   foreach,
                                                    gpointer                   user_data);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_storage_query       (IdeSnippetStorage         *self,
                                                    const gchar               *lang,
                                                    const gchar               *prefix,
                                                    IdeSnippetStorageForeach   foreach,
                                                    gpointer                   user_data);
IDE_AVAILABLE_IN_3_30
void               ide_snippet_storage_load_async  (IdeSnippetStorage         *self,
                                                    GCancellable              *cancellable,
                                                    GAsyncReadyCallback        callback,
                                                    gpointer                   user_data);
IDE_AVAILABLE_IN_3_30
gboolean           ide_snippet_storage_load_finish (IdeSnippetStorage         *self,
                                                    GAsyncResult              *result,
                                                    GError                   **error);

G_END_DECLS
