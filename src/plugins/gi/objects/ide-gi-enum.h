/* ide-gi-enum.h
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
#include "ide-gi-function.h"
#include "ide-gi-value.h"

G_BEGIN_DECLS

struct _IdeGiEnum
{
  IDE_GI_BASE_FIELDS
};

void           ide_gi_enum_free                    (IdeGiBase       *base);
IdeGiBase     *ide_gi_enum_new                     (IdeGiNamespace  *ns,
                                                    IdeGiBlobType    type,
                                                    gint32           offset);
void           ide_gi_enum_dump                    (IdeGiEnum       *self,
                                                    guint            depth);
IdeGiEnum     *ide_gi_enum_ref                     (IdeGiEnum       *self);
void           ide_gi_enum_unref                   (IdeGiEnum       *self);

const gchar   *ide_gi_enum_get_c_type              (IdeGiEnum       *self);
const gchar   *ide_gi_enum_get_g_type_name         (IdeGiEnum       *self);
const gchar   *ide_gi_enum_get_g_get_type          (IdeGiEnum       *self);
const gchar   *ide_gi_enum_get_g_error_domain      (IdeGiEnum       *self);

guint16        ide_gi_enum_get_n_functions         (IdeGiEnum       *self);
guint16        ide_gi_enum_get_n_values            (IdeGiEnum       *self);

IdeGiFunction *ide_gi_enum_get_function            (IdeGiEnum       *self,
                                                    guint16          nth);
IdeGiValue    *ide_gi_enum_get_value               (IdeGiEnum       *self,
                                                    guint16          nth);
IdeGiValue    *ide_gi_enum_lookup_value            (IdeGiEnum       *self,
                                                    const gchar     *name);
IdeGiFunction *ide_gi_enum_lookup_function         (IdeGiEnum       *self,
                                                    const gchar     *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiEnum, ide_gi_enum_unref)

G_END_DECLS
