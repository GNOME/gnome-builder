/* ide-gi-class.h
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
#include "ide-gi-constant.h"
#include "ide-gi-field.h"
#include "ide-gi-function.h"
#include "ide-gi-interface.h"
#include "ide-gi-property.h"
#include "ide-gi-record.h"
#include "ide-gi-signal.h"
#include "ide-gi-union.h"

G_BEGIN_DECLS

struct _IdeGiClass
{
  IDE_GI_BASE_FIELDS
};

void              ide_gi_class_free                    (IdeGiBase       *base);
IdeGiBase        *ide_gi_class_new                     (IdeGiNamespace  *ns,
                                                        IdeGiBlobType    type,
                                                        gint32           offset);
void              ide_gi_class_dump                    (IdeGiClass      *self,
                                                        guint            depth);
IdeGiClass       *ide_gi_class_ref                     (IdeGiClass      *self);
void              ide_gi_class_unref                   (IdeGiClass      *self);

gboolean          ide_gi_class_is_abstract             (IdeGiClass      *self);
gboolean          ide_gi_class_is_fundamental          (IdeGiClass      *self);

const gchar      *ide_gi_class_get_g_type_name         (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_get_type          (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_type_struct       (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_ref_func          (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_unref_func        (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_set_value_func    (IdeGiClass      *self);
const gchar      *ide_gi_class_get_g_get_value_func    (IdeGiClass      *self);
const gchar      *ide_gi_class_get_c_type              (IdeGiClass      *self);
const gchar      *ide_gi_class_get_c_symbol_prefix     (IdeGiClass      *self);

guint16           ide_gi_class_get_n_interfaces        (IdeGiClass      *self);
guint16           ide_gi_class_get_n_fields            (IdeGiClass      *self);
guint16           ide_gi_class_get_n_properties        (IdeGiClass      *self);
guint16           ide_gi_class_get_n_functions         (IdeGiClass      *self);
guint16           ide_gi_class_get_n_signals           (IdeGiClass      *self);
guint16           ide_gi_class_get_n_constants         (IdeGiClass      *self);
guint16           ide_gi_class_get_n_unions            (IdeGiClass      *self);
guint16           ide_gi_class_get_n_records           (IdeGiClass      *self);
guint16           ide_gi_class_get_n_callbacks         (IdeGiClass      *self);

IdeGiInterface   *ide_gi_class_get_interface           (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiField       *ide_gi_class_get_field               (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiProperty    *ide_gi_class_get_property            (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiFunction    *ide_gi_class_get_function            (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiSignal      *ide_gi_class_get_signal              (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiConstant    *ide_gi_class_get_constant            (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiUnion       *ide_gi_class_get_union               (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiRecord      *ide_gi_class_get_record              (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiCallback    *ide_gi_class_get_callback            (IdeGiClass      *self,
                                                        guint16          nth);
IdeGiClass       *ide_gi_class_get_parent              (IdeGiClass      *self);
const gchar      *ide_gi_class_get_parent_qname        (IdeGiClass      *self);
gboolean          ide_gi_class_has_parent              (IdeGiClass      *self);

IdeGiInterface   *ide_gi_class_lookup_interface        (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiField       *ide_gi_class_lookup_field            (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiProperty    *ide_gi_class_lookup_property         (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiFunction    *ide_gi_class_lookup_function         (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiSignal      *ide_gi_class_lookup_signal           (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiConstant    *ide_gi_class_lookup_constant         (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiUnion       *ide_gi_class_lookup_union            (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiRecord      *ide_gi_class_lookup_record           (IdeGiClass      *self,
                                                        const gchar     *name);
IdeGiCallback    *ide_gi_class_lookup_callback         (IdeGiClass      *self,
                                                        const gchar     *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiClass, ide_gi_class_unref)

G_END_DECLS


