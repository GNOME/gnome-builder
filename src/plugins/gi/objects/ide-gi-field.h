/* ide-gi-field.h
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

G_BEGIN_DECLS

struct _IdeGiField
{
  IDE_GI_BASE_FIELDS
};

void            ide_gi_field_free              (IdeGiBase       *base);
IdeGiBase      *ide_gi_field_new               (IdeGiNamespace  *ns,
                                                IdeGiBlobType    type,
                                                gint32           offset);
void            ide_gi_field_dump              (IdeGiField      *self,
                                                guint            depth);
IdeGiField     *ide_gi_field_ref               (IdeGiField      *self);
void            ide_gi_field_unref             (IdeGiField      *self);

gboolean        ide_gi_field_is_readable       (IdeGiField      *self);
gboolean        ide_gi_field_is_writable       (IdeGiField      *self);
gboolean        ide_gi_field_is_private        (IdeGiField      *self);

IdeGiTypeRef    ide_gi_field_get_typeref       (IdeGiField      *self);
guint16         ide_gi_field_get_bits          (IdeGiField      *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiField, ide_gi_field_unref)

G_END_DECLS
