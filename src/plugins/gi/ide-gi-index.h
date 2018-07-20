/* ide-gi-index.h
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

#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <ide.h>

#include "ide-gi.h"
#include "ide-gi-types.h"

#include "ide-gi-complete.h"
#include "ide-gi-complete-item.h"
#include "ide-gi-namespace.h"
#include "ide-gi-repository.h"
#include "ide-gi-require.h"
#include "ide-gi-version.h"

G_BEGIN_DECLS

/* If the index file layout change, the ABI version need to be bumped */
#define INDEX_ABI_VERSION         1
#define INDEX_FILE_NAME           "index"
#define INDEX_FILE_EXTENSION      ".tree"
#define INDEX_NAMESPACE_EXTENSION ".ns"

typedef enum {
  IDE_GI_INDEX_STATE_NOT_INIT,
  IDE_GI_INDEX_STATE_ERROR,
  IDE_GI_INDEX_STATE_READY,
} IdeGiIndexState;

GFile              *ide_gi_index_get_cache_dir                  (IdeGiIndex           *self);
IdeGiVersion       *ide_gi_index_get_current_version            (IdeGiIndex           *self);
IdeGiRepository    *ide_gi_index_get_repository                 (IdeGiIndex           *self);
const gchar        *ide_gi_index_get_runtime_id                 (IdeGiIndex           *self);
IdeGiIndexState     ide_gi_index_get_state                      (IdeGiIndex           *self);
gboolean            ide_gi_index_is_updating                    (IdeGiIndex           *self);
void                ide_gi_index_queue_update                   (IdeGiIndex           *self,
                                                                 GCancellable         *cancellable);
void                ide_gi_index_update_async                   (IdeGiIndex           *self,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
gboolean            ide_gi_index_update_finish                  (IdeGiIndex           *self,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);

void                ide_gi_index_new_async                      (IdeGiRepository      *repository,
                                                                 IdeContext           *context,
                                                                 GFile                *cache_dir,
                                                                 const gchar          *runtime_id,
                                                                 gboolean              update_on_build,
                                                                 GCancellable         *cancellable,
                                                                 GAsyncReadyCallback   callback,
                                                                 gpointer              user_data);
IdeGiIndex         *ide_gi_index_new_finish                     (GAsyncInitable       *initable,
                                                                 GAsyncResult         *result,
                                                                 GError              **error);

G_END_DECLS
