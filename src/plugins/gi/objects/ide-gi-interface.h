/* ide-gi-interface.h
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
#include "ide-gi-property.h"
#include "ide-gi-signal.h"

G_BEGIN_DECLS

struct _IdeGiInterface
{
 IDE_GI_BASE_FIELDS
};

void              ide_gi_interface_free                    (IdeGiBase       *base);
IdeGiBase        *ide_gi_interface_new                     (IdeGiNamespace  *ns,
                                                            IdeGiBlobType    type,
                                                            gint32           offset);
void              ide_gi_interface_dump                    (IdeGiInterface  *self,
                                                            guint            depth);
IdeGiInterface   *ide_gi_interface_ref                     (IdeGiInterface  *self);
void              ide_gi_interface_unref                   (IdeGiInterface  *self);

const gchar      *ide_gi_interface_get_g_type_name         (IdeGiInterface  *self);
const gchar      *ide_gi_interface_get_g_get_type          (IdeGiInterface  *self);
const gchar      *ide_gi_interface_get_c_type              (IdeGiInterface  *self);
const gchar      *ide_gi_interface_get_c_symbol_prefix     (IdeGiInterface  *self);

guint16           ide_gi_interface_get_n_callbacks         (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_constants         (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_fields            (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_functions         (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_prerequisites     (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_properties        (IdeGiInterface  *self);
guint16           ide_gi_interface_get_n_signals           (IdeGiInterface  *self);

IdeGiCallback    *ide_gi_interface_get_callback            (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiConstant    *ide_gi_interface_get_constant            (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiField       *ide_gi_interface_get_field               (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiFunction    *ide_gi_interface_get_function            (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiBase        *ide_gi_interface_get_prerequisite        (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiProperty    *ide_gi_interface_get_property            (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiSignal      *ide_gi_interface_get_signal              (IdeGiInterface  *self,
                                                            guint16          nth);
IdeGiCallback    *ide_gi_interface_lookup_callback         (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiConstant    *ide_gi_interface_lookup_constant         (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiField       *ide_gi_interface_lookup_field            (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiFunction    *ide_gi_interface_lookup_function         (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiBase        *ide_gi_interface_lookup_prerequisite     (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiProperty    *ide_gi_interface_lookup_property         (IdeGiInterface  *self,
                                                            const gchar     *name);
IdeGiSignal      *ide_gi_interface_lookup_signal           (IdeGiInterface  *self,
                                                            const gchar     *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiInterface, ide_gi_interface_unref)

G_END_DECLS
