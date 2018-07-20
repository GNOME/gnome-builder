/* ide-gi-complete-item.h
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

#include <glib.h>

#include "ide-gi.h"
#include "ide-gi-blob.h"
#include "ide-gi-types.h"

#include "ide-gi-namespace.h"

#include "objects/ide-gi-base.h"

G_BEGIN_DECLS

typedef struct {
  gchar           *word;
  IdeGiPrefixType  type;
  guint8           major_version;
  guint8           minor_version;
  IdeGiNamespace  *ns;
  /* TODO: add an object id not instantiated yet - see get_object */
} IdeGiCompletePrefixItem;

typedef struct {
  gchar           *word;
  IdeGiBlobType    type;
  IdeGiBase       *object;
} IdeGiCompleteObjectItem;

typedef struct {
  gchar           *word;
  IdeGiNamespace  *ns;

  guint8           major_version;
  guint8           minor_version;
  IdeGiBlobType    object_type;
  guint32          object_offset;

  guint            is_buildable : 1;
} IdeGiCompleteGtypeItem;

void            ide_gi_complete_prefix_item_clear        (IdeGiCompletePrefixItem *self);
void            ide_gi_complete_object_item_clear        (IdeGiCompleteObjectItem *self);
void            ide_gi_complete_gtype_item_clear         (IdeGiCompleteGtypeItem  *self);

G_END_DECLS
