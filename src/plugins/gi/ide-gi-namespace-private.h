/* ide-gi-namespace-private.h
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

#include "ide-gi-blob.h"
#include "ide-gi-types.h"
#include "ide-gi-namespace.h"

G_BEGIN_DECLS

gsize               _ide_gi_namespace_get_blob_size_from_type      (IdeGiNamespace    *self,
                                                                    IdeGiBlobType      type);
IdeGiCrossRef      *_ide_gi_namespace_get_crossref                 (IdeGiNamespace    *self,
                                                                    guint32            offset);
IdeGiNamespaceId    _ide_gi_namespace_get_id                       (IdeGiNamespace    *self);
guint8             *_ide_gi_namespace_get_table_from_type          (IdeGiNamespace    *self,
                                                                    IdeGiBlobType      type);
void                _ide_gi_namespace_free                         (IdeGiNamespace    *self);
IdeGiNamespace     *_ide_gi_namespace_new                          (IdeGiVersion      *version,
                                                                    IdeGiNamespaceId   id);

G_END_DECLS
