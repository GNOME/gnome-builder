/* ide-gi-base.h
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
#include <glib-object.h>

#include "./../ide-gi-blob.h"
#include "./../ide-gi-types.h"
#include "./../ide-gi-namespace.h"
#include "./../ide-gi-namespace-private.h"

#include "ide-gi-doc.h"

G_BEGIN_DECLS

#define IDE_GI_BASE_FIELDS      \
  volatile gint    ref_count;   \
  IdeGiNamespace  *ns;          \
  IdeGiCommonBlob *common_blob; \
  IdeGiBlobType    type;        \
  gint32           offset;

struct _IdeGiBase
{
  IDE_GI_BASE_FIELDS
};

IdeGiBase        *ide_gi_base_new                      (IdeGiNamespace *ns,
                                                        IdeGiBlobType   type,
                                                        gint32          offset);
void              ide_gi_base_dump                     (IdeGiBase      *self);
IdeGiDoc         *ide_gi_base_get_doc                  (IdeGiBase      *self);
const gchar      *ide_gi_base_get_name                 (IdeGiBase      *self);
gchar            *ide_gi_base_get_qualified_name       (IdeGiBase      *self);
const gchar      *ide_gi_base_get_deprecated_version   (IdeGiBase      *self);
const gchar      *ide_gi_base_get_version              (IdeGiBase      *self);

gboolean          ide_gi_base_is_deprecated            (IdeGiBase      *self);
gboolean          ide_gi_base_is_introspectable        (IdeGiBase      *self);
IdeGiStability    ide_gi_base_stability                (IdeGiBase      *self);

IdeGiNamespace   *ide_gi_base_get_namespace            (IdeGiBase      *self);
const gchar      *ide_gi_base_get_namespace_name       (IdeGiBase      *self);
IdeGiBlobType     ide_gi_base_get_object_type          (IdeGiBase      *self);
IdeGiBase        *ide_gi_base_ref                      (IdeGiBase      *self);
void              ide_gi_base_unref                    (IdeGiBase      *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IdeGiBase, ide_gi_base_unref)

G_END_DECLS
