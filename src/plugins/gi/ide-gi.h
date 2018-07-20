/* ide-gi.h
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

#include "ide-gi-macros.h"
#include "ide-gi-types.h"
#include "ide-gi-blob.h"
#include "ide-gi-crossref.h"

G_BEGIN_DECLS

typedef struct
{
  gchar           *name;
  guint32          object_offset;
  IdeGiPrefixType  type;
  IdeGiBlobType    object_type;

  guint            is_buildable : 1;
} IdeGiGlobalIndexEntry;

void      ide_gi_global_index_entry_clear      (gpointer data);

G_END_DECLS
