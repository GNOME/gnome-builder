/* ide-gi-repository.h
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

#pragma once

#include <glib.h>
#include <ide.h>

#include "ide-gi.h"
#include "ide-gi-types.h"

#include "ide-gi-namespace.h"
#include "ide-gi-version.h"

G_BEGIN_DECLS

gboolean                 ide_gi_repository_add_gir_search_path      (IdeGiRepository      *self,
                                                                     const gchar          *path);
gboolean                 ide_gi_repository_get_updaate_on_build     (IdeGiRepository      *self);
const gchar             *ide_gi_repository_get_cache_path           (IdeGiRepository      *self);
IdeGiVersion            *ide_gi_repository_get_current_version      (IdeGiRepository      *self);
GPtrArray               *ide_gi_repository_get_project_girs         (IdeGiRepository      *self);
const gchar             *ide_gi_repository_get_current_runtime_id   (IdeGiRepository      *self);
GPtrArray               *ide_gi_repository_get_gir_search_paths     (IdeGiRepository      *self);
gboolean                 ide_gi_repository_remove_gir_search_path   (IdeGiRepository      *self,
                                                                     const gchar          *path);
void                     ide_gi_repository_set_update_on_build      (IdeGiRepository      *self,
                                                                     gboolean              state);

GFile                   *_ide_gi_repository_get_builddir            (IdeGiRepository      *self);
IdeGiRepository         *ide_gi_repository_new                      (IdeContext           *context,
                                                                     gboolean              update_on_build);
void                     ide_gi_repository_queue_update             (IdeGiRepository      *self,
                                                                     GCancellable         *cancellable);
void                     ide_gi_repository_update_async             (IdeGiRepository      *self,
                                                                     GCancellable         *cancellable,
                                                                     GAsyncReadyCallback   callback,
                                                                     gpointer              user_data);
gboolean                 ide_gi_repository_update_finish            (IdeGiRepository      *self,
                                                                     GAsyncResult         *result,
                                                                     GError              **error);

G_END_DECLS
