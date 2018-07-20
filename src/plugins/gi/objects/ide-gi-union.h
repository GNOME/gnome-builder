/* ide-gi-union.h
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
#include <glib-object.h>

#include "./../ide-gi-blob.h"
#include "./../ide-gi-types.h"
#include "./../ide-gi-namespace.h"
#include "./../ide-gi-namespace-private.h"

#include "ide-gi-base.h"
#include "ide-gi-field.h"
#include "ide-gi-function.h"
#include "ide-gi-interface.h"
#include "ide-gi-record.h"

G_BEGIN_DECLS

struct _IdeGiUnion
{
  IDE_GI_BASE_FIELDS
};

void            ide_gi_union_free                 (IdeGiBase      *base);
IdeGiBase      *ide_gi_union_new                  (IdeGiNamespace *ns,
                                                   IdeGiBlobType   type,
                                                   gint32          offset);
void            ide_gi_union_dump                 (IdeGiUnion     *self,
                                                   guint           depth);
IdeGiUnion     *ide_gi_union_ref                  (IdeGiUnion     *self);
void            ide_gi_union_unref                (IdeGiUnion     *self);

const gchar    *ide_gi_union_get_g_type_name      (IdeGiUnion     *self);
const gchar    *ide_gi_union_get_g_get_type       (IdeGiUnion     *self);
const gchar    *ide_gi_union_get_c_type           (IdeGiUnion     *self);
const gchar    *ide_gi_union_get_c_symbol_prefix  (IdeGiUnion     *self);

guint16         ide_gi_union_get_n_fields         (IdeGiUnion     *self);
guint16         ide_gi_union_get_n_functions      (IdeGiUnion     *self);
guint16         ide_gi_union_get_n_records        (IdeGiUnion     *self);

IdeGiField     *ide_gi_union_get_field            (IdeGiUnion     *self,
                                                   guint16         nth);
IdeGiFunction  *ide_gi_union_get_function         (IdeGiUnion     *self,
                                                   guint16         nth);
IdeGiRecord    *ide_gi_union_get_record           (IdeGiUnion     *self,
                                                   guint16         nth);
IdeGiField     *ide_gi_union_lookup_field         (IdeGiUnion     *self,
                                                   const gchar    *name);
IdeGiFunction  *ide_gi_union_lookup_function      (IdeGiUnion     *self,
                                                   const gchar    *name);
IdeGiRecord    *ide_gi_union_lookup_record        (IdeGiUnion     *self,
                                                   const gchar    *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiUnion, ide_gi_union_unref)

G_END_DECLS
