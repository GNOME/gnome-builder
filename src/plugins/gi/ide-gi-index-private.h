/* ide-gi-index-private.h
 *
 * Copyright 2018 Sebastien Lafargue <slafargue@gnome.org>
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

#include <glib.h>
#include <ide.h>

#include "ide-gi-types.h"

#include "ide-gi-file-builder.h"
#include "ide-gi-repository.h"
#include "ide-gi-version.h"
#include "ide-gi-index.h"

G_BEGIN_DECLS

struct _IdeGiIndex
{
  IdeObject          parent_instance;

  GFile             *cache_dir;
  GFile             *staging_dir;
  IdeGiFileBuilder  *file_builder;
  gchar             *runtime_id;

  IdeGiRepository   *repository;
  GHashTable        *files;
  GHashTable        *versions;
  GQueue            *update_queue;
  GQueue            *remove_queue;
  IdeGiVersion      *current_version;
  GMutex             mutex;

  IdeGiIndexState    state;
  gint               pool_count;
  guint              version_count;
  guint              pool_all_pushed : 1;
  guint              is_updating     : 1;
  guint              update_on_build : 1;
};

typedef struct {
  GMappedFile *mapped_file;
  guint        count;
} NsRecord;

gboolean  _ide_gi_index_get_update_on_build   (IdeGiIndex   *self);
NsRecord *_ide_gi_index_get_ns_record         (IdeGiIndex   *self,
                                               const gchar  *name);
void      _ide_gi_index_version_remove        (IdeGiIndex   *self,
                                               IdeGiVersion *version);

void      _ide_gi_index_set_update_on_build   (IdeGiIndex   *self,
                                               gboolean      state);

G_END_DECLS
