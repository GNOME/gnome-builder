/* ide-gi-property.h
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

struct _IdeGiProperty
{
  IDE_GI_BASE_FIELDS
};

void                     ide_gi_property_free                       (IdeGiBase       *base);
IdeGiBase               *ide_gi_property_new                        (IdeGiNamespace  *ns,
                                                                     IdeGiBlobType    type,
                                                                     gint32           offset);
void                     ide_gi_property_dump                       (IdeGiProperty   *self,
                                                                     guint            depth);
IdeGiProperty           *ide_gi_property_ref                        (IdeGiProperty   *self);
void                     ide_gi_property_unref                      (IdeGiProperty   *self);

gboolean                 ide_gi_property_is_readable                (IdeGiProperty   *self);
gboolean                 ide_gi_property_is_writable                (IdeGiProperty   *self);
gboolean                 ide_gi_property_is_construct               (IdeGiProperty   *self);
gboolean                 ide_gi_property_is_construct_only          (IdeGiProperty   *self);

IdeGiTransferOwnership   ide_gi_property_get_transfer_ownership     (IdeGiProperty   *self);

IdeGiTypeRef             ide_gi_property_get_typeref                (IdeGiProperty   *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiProperty, ide_gi_property_unref)

G_END_DECLS
