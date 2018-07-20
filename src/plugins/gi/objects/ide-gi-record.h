/* ide-gi-record.h
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
#include "ide-gi-callback.h"
#include "ide-gi-field.h"
#include "ide-gi-function.h"
#include "ide-gi-property.h"
#include "ide-gi-union.h"

G_BEGIN_DECLS

struct _IdeGiRecord
{
  IDE_GI_BASE_FIELDS
};

void             ide_gi_record_free                           (IdeGiBase       *base);
IdeGiBase       *ide_gi_record_new                            (IdeGiNamespace  *ns,
                                                               IdeGiBlobType    type,
                                                               gint32           offset);
void             ide_gi_record_dump                           (IdeGiRecord     *self,
                                                               guint            depth);
IdeGiRecord     *ide_gi_record_ref                            (IdeGiRecord     *self);
void             ide_gi_record_unref                          (IdeGiRecord     *self);

gboolean         ide_gi_record_is_disguised                   (IdeGiRecord     *self);
gboolean         ide_gi_record_is_foreign                     (IdeGiRecord     *self);

const gchar     *ide_gi_record_get_g_type_name                (IdeGiRecord     *self);
const gchar     *ide_gi_record_get_g_get_type                 (IdeGiRecord     *self);
const gchar     *ide_gi_record_get_g_is_gtype_struct_for      (IdeGiRecord     *self);
const gchar     *ide_gi_record_get_c_type                     (IdeGiRecord     *self);
const gchar     *ide_gi_record_get_c_symbol_prefix            (IdeGiRecord     *self);

guint16          ide_gi_record_get_n_callbacks                (IdeGiRecord     *self);
guint16          ide_gi_record_get_n_fields                   (IdeGiRecord     *self);
guint16          ide_gi_record_get_n_functions                (IdeGiRecord     *self);
guint16          ide_gi_record_get_n_properties               (IdeGiRecord     *self);
guint16          ide_gi_record_get_n_unions                   (IdeGiRecord     *self);

IdeGiCallback   *ide_gi_record_get_callback                   (IdeGiRecord      *self,
                                                               guint16          nth);
IdeGiField      *ide_gi_record_get_field                      (IdeGiRecord      *self,
                                                               guint16          nth);
IdeGiFunction   *ide_gi_record_get_function                   (IdeGiRecord      *self,
                                                               guint16          nth);
IdeGiProperty   *ide_gi_record_get_property                   (IdeGiRecord      *self,
                                                               guint16          nth);
IdeGiUnion      *ide_gi_record_get_union                      (IdeGiRecord      *self,
                                                               guint16          nth);
IdeGiCallback   *ide_gi_record_lookup_callback                (IdeGiRecord     *self,
                                                               const gchar     *name);
IdeGiField      *ide_gi_record_lookup_field                   (IdeGiRecord     *self,
                                                               const gchar     *name);
IdeGiFunction   *ide_gi_record_lookup_function                (IdeGiRecord     *self,
                                                               const gchar     *name);
IdeGiProperty   *ide_gi_record_lookup_property                (IdeGiRecord     *self,
                                                               const gchar     *name);
IdeGiUnion      *ide_gi_record_lookup_union                   (IdeGiRecord     *self,
                                                               const gchar     *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiRecord, ide_gi_record_unref)

G_END_DECLS
